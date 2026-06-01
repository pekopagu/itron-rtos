# Design Document

## Overview

第12章12.4では、`sig_sem()` がFIFO wait queueからWAITING taskを取り出してREADYへ戻した直後に、current RUNNING taskとwoken READY taskのpriorityを比較する。priority値が小さいtaskを高優先度として扱い、woken taskがcurrentより高優先度の場合だけ既存の `dispatcher_switch_to(from, to)` 境界へ接続する。

この仕様はtask文脈APIとしての `sig_sem()` を拡張する。timer IRQ handler本体、dispatch pending request/consume、割り込み復帰フレーム切替には責務を混ぜない。同一優先度と低優先度wakeupはno-switchとしてログに残し、time sliceやround-robinには進めない。

### Goals

- `sig_sem()` のWAITING->READY後にpreemption判定を追加する。
- 高優先度wakeup時だけ既存dispatcher switch境界へ進む。
- 12.3のFIFO wait queue、count制御、`wait_sem_id` clearを維持する。
- 10.4 `yield_tsk()` と11.4 timer IRQ deferred dispatchログを維持する。

### Non-Goals

- priority順wait queue
- timeout付き `twai_sem`
- sleep/delay queue
- nested interrupt
- 同一優先度time slice、round-robin
- timer IRQ handlerからの `sig_sem()` / `wai_sem()` / `yield_tsk()` / `dispatcher_switch_to()` 呼び出し
- 完全な割り込み復帰フレーム切替

## Boundary Commitments

### This Spec Owns

- `sig_sem()` wakeup後のtask文脈preemption判定。
- current RUNNING taskとwoken READY taskのpriority比較。
- 高優先度wakeup時の `[sig-sem] preempt required` / switch begin/end / `wakeup-switch` ログ。
- 同一または低優先度wakeup時の `[sig-sem] preempt not required` / `wakeup-no-switch` ログ。
- README、Doxygenコメント、`docs/logs/qemu-serial.log`、spec成果物の12.4更新。

### Out of Boundary

- wait queueのpriority順化。
- timeout、sleep/delay queue、timer連携待ち。
- IRQ中のdispatch pending経路との統合。
- 同一優先度time slice、round-robin。
- dispatcher/context switch基盤の根本変更。

### Allowed Dependencies

- `semaphore.c` は `dispatcher_get_current()` と `dispatcher_switch_to()` をtask文脈switch境界として利用してよい。
- `semaphore.c` は `task_get_by_id()` / `task_get_mutable_by_id()` を使い、READY復帰済みtaskの読み取りとswitch対象取得を行ってよい。
- `semaphore.c` はtask stateのREADY復帰を引き続き `task_wake_waiting_on_sem_by_id()` に委譲する。
- timer IRQ handlerとdispatch pending moduleは変更しない。

### Revalidation Triggers

- `dispatcher_switch_to()` のfrom/to許容state変更。
- `task_wake_waiting_on_sem_by_id()` のREADY復帰契約変更。
- `sig_sem()` のログ順序変更。
- timer IRQ handlerが直接dispatcherやsemaphore APIを呼ぶ変更。

## Architecture

### Existing Architecture Analysis

12.3の `sig_sem()` は `sem_dequeue_waiter()` で対象semaphoreのFIFO wait queueからtask idを取り出し、`task_wake_waiting_on_sem_by_id()` でWAITING->READYへ戻す。待ちtaskがいる場合はcountを増やさず、queueが空の場合だけcountを増やす。

10.4の `yield_tsk()` と11.4のtimer IRQ deferred dispatchは、すでに `dispatcher_switch_to()` を切替境界として共有している。ただし11.4はIRQ起点のdispatch pending消費であり、12.4の `sig_sem()` はtask文脈APIなので、pending stateには触れず直接dispatcher境界へ進む。

### Architecture Pattern & Boundary Map

```text
sig_sem(sem_id)
  -> sem_dequeue_waiter(sem_id, &task_id)
     empty:
       count++
       action=count-up
     dequeued:
       task_wake_waiting_on_sem_by_id(task_id, sem_id)
       current = dispatcher_get_current()
       woken = task_get_by_id(task_id)
       compare woken->priority < current->priority
         true:
           dispatcher_switch_to(current_mutable, woken_mutable)
           action=wakeup-switch
         false:
           action=wakeup-no-switch
```

判定はREADY復帰後に行うため、`wait_sem_id` clearとREADYログがpreempt checkより前に出る。これにより、woken taskはdispatcherのto条件であるREADYを満たした状態でswitch対象になる。

## File Structure Plan

### Modified Files

- `kernel/semaphore.c`: `sig_sem()` のwakeup後preemption判定、switch接続、ログ、Doxygenコメントを更新する。
- `kernel/include/semaphore.h`: `sig_sem()` コメントを12.4のtask文脈preemption判定に更新する。
- `kernel/kernel.c`: semaphore smokeのpriority設定とコメントを12.4に合わせる。
- `README.md`: 12.4の進捗、到達点、未実装範囲、Zenn tag候補を追記する。
- `docs/logs/qemu-serial.log`: `make run` の最新出力で更新する。
- `.kiro/specs/semaphore-wakeup-preemption/requirements.md`, `design.md`, `tasks.md`: 最終成果物として3ファイルだけ残す。

## Components and Interfaces

| Component | Domain | Intent | Requirements | Contracts |
| --- | --- | --- | --- | --- |
| SemaphoreWakeupPreemption | Semaphore Layer | READY復帰後にpriority比較し、必要時だけswitchへ進む | 1, 2, 3 | Service |
| DispatcherSwitchBoundary | Dispatcher Layer | 既存のtask-to-task switch境界を再利用する | 2, 4 | Service |
| DocumentationEvidence | Docs | 12.4到達点と検証証跡を残す | 5 | Document |

### Semaphore Layer

#### SemaphoreWakeupPreemption

**Responsibilities & Constraints**

- READY復帰後にcurrentとwokenを読み取り、priority値だけを比較する。
- `woken->priority < current->priority` の場合だけswitchへ進む。
- 同一priorityはtime slice対象外としてno-switchにする。
- 待ちtaskがいる場合はcountを増やさない。
- wait queue dequeue順はFIFOのまま維持する。
- IRQ pending stateには触れない。

**Service Contract**

- Preconditions: `sig_sem()` はtask文脈から呼ばれる。woken taskは `task_wake_waiting_on_sem_by_id()` 成功後にREADYである。
- Postconditions: 高優先度wakeup時はdispatcher switch境界へ進む。同一/低優先度ではcurrentを維持する。
- Invariants: `wait_sem_id` はREADY復帰時に0へ戻る。queueが空でないwakeupではsemaphore countを増やさない。

## Requirements Traceability

| Requirement | Summary | Components | Interfaces | Flows |
| --- | --- | --- | --- | --- |
| 1.1, 1.2, 1.3, 1.4, 1.5 | wakeup後priority判定 | SemaphoreWakeupPreemption | `sig_sem()` | wakeup flow |
| 2.1, 2.2, 2.3, 2.4, 2.5 | 高優先度wakeup switch | SemaphoreWakeupPreemption, DispatcherSwitchBoundary | `dispatcher_switch_to()` | switch flow |
| 3.1, 3.2, 3.3, 3.4, 3.5 | 12.3契約維持 | SemaphoreWakeupPreemption | wait queue/task APIs | semaphore flow |
| 4.1, 4.2, 4.3, 4.4, 4.5 | 既存経路維持 | DispatcherSwitchBoundary | build/run/timer validation | validation |
| 5.1, 5.2, 5.3, 5.4 | 文書と成果物 | DocumentationEvidence | README/log/spec | documentation |

## Testing Strategy

- `make` で通常buildが通ることを確認する。
- `make run` で `sig_sem()` のdequeue、READY復帰、preempt check、高優先度wakeup switch、count-upを確認する。
- `make run VALIDATE_TIMER_IRQ_ENTRY=1` で11.4の高優先度deferred dispatchと同一優先度no-dispatchログが維持されることを確認する。
- `rg` でtimer IRQ handler本体から `sig_sem()` / `wai_sem()` / `yield_tsk()` / `dispatcher_switch_to()` を直接呼んでいないことを確認する。
- `.kiro/specs/semaphore-wakeup-preemption/` が最終的に3ファイルだけであることを確認する。
