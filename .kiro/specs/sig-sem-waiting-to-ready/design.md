# Design Document

## Overview

第12章12.2では、既存の簡易semaphore tableとtask table走査を使い、`sig_sem(int sem_id)` で対象セマフォを待つWAITING taskを1件だけREADYへ戻す。READY復帰時は `wait_sem_id` を未待ち状態へ戻し、semaphore countは増やさない。待ちtaskが見つからない場合だけcountを1増やす。

このspecは12.1のboot-time verification modelを拡張する。完全なwait queue、FIFO順制御、priority順制御、wakeup後preemption、`sig_sem()`直後の即時context switch、timeout付き `twai_sem`、sleep/delay queue、nested interrupt、同一優先度time slice、round-robin、完全な割り込み復帰フレーム切替は扱わない。

## Boundary Commitments

### Owned

- `sig_sem(int sem_id)` の12.2向けログと挙動を定義する。
- task table全体から `state == WAITING` かつ `wait_sem_id == sem_id` のtaskを1件探す。
- 見つかったtaskをREADYへ戻し、`wait_sem_id` を0へ戻す。
- READY復帰時はsemaphore countを増やさない。
- 待ちtaskがいない場合だけsemaphore countを1増やす。
- README、Doxygenコメント、QEMU serial log、spec成果物を更新する。

### Out of Boundary

- wait queue導入、複数WAITING taskのFIFO順制御、priority順制御。
- 12.4のwakeup後preemption判定、`sig_sem()`直後の即時context switch。
- timeout付き `twai_sem`、sleep/delay queue、nested interrupt。
- 同一優先度time slice、round-robin。
- timer IRQ handler本体からの `sig_sem()`、`wai_sem()`、`yield_tsk()`、`dispatcher_switch_to()` 呼び出し。
- 既存RTOS実装の参照、コピー、流用。

### Allowed Dependencies

- `semaphore.c` はsemaphore tableのID解決とcount更新を所有する。
- `semaphore.c` はtask moduleの探索・READY復帰APIを呼び、task stateの直接所有はしない。
- `task.c` はTCBの `state` と `wait_sem_id` の更新を所有する。
- `kernel.c` はboot-time smokeとして `wai_sem()` 後に `sig_sem()` を呼ぶが、timer IRQ handlerからは呼ばない。

## Architecture

```text
sig_sem(sem_id)
  -> find semaphore
  -> task_find_waiting_on_sem(sem_id)
     found:
       log waiting task found
       task_wake_one_waiting_on_sem(sem_id)
       log wakeup-no-switch
       return 0
     not found:
       log no waiting task
       increment semaphore count
       log count-up
       return 0
```

`task_find_waiting_on_sem()` と `task_wake_one_waiting_on_sem()` はwait queueではなくtask tableの線形走査を行う。複数taskが一致した場合は最初に見つかった1 taskだけを対象にする。この順序にFIFOやpriorityの意味は持たせず、12.3でwait queue導入時に置き換える。

## Components and Interfaces

### Semaphore Layer

- `int sig_sem(int sem_id)`
  - 呼び出しログを `[sig-sem] called: sem_id=N` として出力する。
  - semaphoreが存在しない場合は副作用なしで負値を返す。
  - 待ちtaskがいる場合はtask layerへREADY復帰を委譲し、countは増やさない。
  - 待ちtaskがいない場合はcountを1増やす。

### Task Layer

- `const tcb_t *task_find_waiting_on_sem(int sem_id)`
  - task tableから `WAITING` かつ `wait_sem_id == sem_id` のtaskを1件返す。
- `int task_wake_one_waiting_on_sem(int sem_id, int *woken_task_id)`
  - 一致taskを `READY` へ戻し、`wait_sem_id` を0へ戻す。
  - READY復帰ログでは `wait_sem_id=none state=READY` を確認できるようにする。

### Kernel Smoke

- 12.1の `wai_sem()` smokeでWAITING化したtaskに対して `sig_sem()` を呼び、READY復帰を観測する。
- 続けて待ちtaskなしの `sig_sem()` を呼び、count 0->1を観測する。
- 10.4 yield smoke、11.4 timer IRQ validation smoke、12.1 WAITING smokeの順序と責務は変更しない。

## File Structure Plan

- `kernel/include/semaphore.h`: `sig_sem()` のDoxygenコメントを12.2到達点へ更新。
- `kernel/semaphore.c`: `sig_sem()` のログとcount-up/wakeup分岐を12.2仕様へ更新。
- `kernel/task.c`: READY復帰ログを `wait_sem_id=none state=READY` で観測できるよう更新。
- `kernel/include/task.h`: 12.2の探索・READY復帰コメントを更新。
- `kernel/kernel.c`: 12.2 smokeとして `sig_sem()` wakeup/no-wait count-upを追加。
- `README.md`: 12.2到達点、未実装項目、Zenn tag候補を追加。
- `docs/logs/qemu-serial.log`: `make run` の最新出力で更新。
- `.kiro/specs/sig-sem-waiting-to-ready/requirements.md`, `design.md`, `tasks.md`: spec成果物。

## Testing Strategy

- `make` で通常buildが通ることを確認する。
- `make run` で `wai_sem()` のWAITING化、`sig_sem()` のREADY復帰、`wait_sem_id` clear、wakeup時count非増加、待ちtaskなしcount-upを確認する。
- `make run VALIDATE_TIMER_IRQ_ENTRY=1` で11.4のdispatch pending request/consume/clearログが維持されることを確認する。
- `rg` でtimer IRQ handler本体が `wai_sem()`、`sig_sem()`、`yield_tsk()`、`dispatcher_switch_to()` を直接呼ばないことを確認する。
- `.kiro/specs/sig-sem-waiting-to-ready/` が最終的に3ファイルだけであることを確認する。
