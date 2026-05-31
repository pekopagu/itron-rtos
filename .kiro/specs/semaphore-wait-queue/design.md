# Design Document

## Overview

第12章12.3では、semaphore tableの各entryに固定長FIFO wait queueを追加する。`wai_sem()` がcount 0でRUNNING current taskをWAITINGへ落とした後、対象semaphoreのwait queueへtask idをenqueueする。`sig_sem()` は12.2までのtask table全体走査をやめ、対象semaphoreのwait queueから1 task idをdequeueし、そのtaskをREADYへ戻す。

このspecはboot-time verification modelの拡張であり、実CPU実行状態としての完全なブロック、割り込み復帰時の即時切替、priority順待ち解除、timeout、同一優先度time slice、round-robinは扱わない。`sig_sem()` と `wai_sem()` はtask文脈APIとして扱い、timer IRQ handler本体からは呼ばない。

## Boundary Commitments

### Owned

- `semaphore_t` にsemaphoreごとのFIFO wait queue情報を追加する。
- `sem_init()` と `sem_create()` でwait queueをemptyにする。
- `wai_sem()` のWAITING化成功後に対象semaphore wait queueへtask idをenqueueする。
- `sig_sem()` は対象semaphore wait queueからdequeueしたtaskだけをREADYへ戻す。
- READY復帰時に `wait_sem_id` を未待ち状態へ戻す。
- 待ちtaskをREADYへ戻した場合はsemaphore countを増やさない。
- wait queueが空の場合のみsemaphore countを1増やす。
- wait queue enqueue/dequeue/empty状態をログで観測可能にする。
- README、Doxygenコメント、QEMU serial log、spec成果物を12.3の到達点へ更新する。

### Out of Boundary

- priority順wait queue。
- timeout付き `twai_sem`、sleep/delay queue。
- 12.4のwakeup後preemption判定、`sig_sem()` 直後の即時context switch。
- nested interrupt、同一優先度time slice、round-robin。
- 完全な割り込み復帰フレーム切替。
- timer IRQ handler本体からの `sig_sem()`、`wai_sem()`、`yield_tsk()`、`dispatcher_switch_to()` 呼び出し。
- 既存RTOS実装の参照、コピー、流用。

### Allowed Dependencies

- `itron_api.c` は `sem_enqueue_waiter()` を使ってWAITING化済みtaskを対象semaphore queueへ登録する。
- `semaphore.c` はsemaphore table、count、wait queueを所有し、task idから名前を引くためにtaskの読み取りAPIを使う。
- `semaphore.c` はdequeue後のREADY復帰をtask moduleへ委譲し、TCBの直接書き換えは行わない。
- `task.c` はTCBの `state` と `wait_sem_id` の更新を所有する。
- scheduler、dispatcher、dispatch pending、timer IRQの既存責務は変更しない。

## Architecture

```text
wai_sem(sem_id)
  -> sem_take_if_available(sem_id)
     count > 0:
       decrement count
       complete no-switch
     count == 0:
       task_mark_waiting_on_sem(current->id, sem_id)
       sem_enqueue_waiter(sem_id, current->id)
       scheduler_select_next()
       dispatcher_switch_to(current, next)

sig_sem(sem_id)
  -> sem_dequeue_waiter(sem_id, &task_id)
     dequeued:
       task_get_by_id(task_id) for log
       task_wake_waiting_on_sem_by_id(task_id, sem_id)
       complete wakeup-no-switch
     empty:
       increment count
       complete count-up
```

wait queueは固定長のtask id配列、head、tail、countで表現する。enqueueはtailへ書き込み、tailを循環更新し、countを増やす。dequeueはheadから読み出し、headを循環更新し、countを減らす。固定長は `MAX_TASKS` とし、学習用の全task登録数を上限にする。

`task_wake_waiting_on_sem_by_id(task_id, sem_id)` を追加し、dequeueされたtask idだけをREADY復帰対象にする。これにより `sig_sem()` からtask table全体を探索して待ちtaskを探す経路を廃止する。

## Components and Interfaces

### Semaphore Layer

- `semaphore_t`
  - `wait_queue[MAX_TASKS]`
  - `wait_head`
  - `wait_tail`
  - `wait_count`
- `int sem_enqueue_waiter(int sem_id, int task_id)`
  - 対象semaphoreにtask idをFIFO enqueueする。
  - task nameを読み取り、`[sem-wq] enqueue` を出力する。
- `int sem_dequeue_waiter(int sem_id, int *task_id)`
  - 対象semaphoreからtask idをFIFO dequeueする。
  - 空なら `[sem-wq] empty` を出力して `SEM_WAIT_QUEUE_EMPTY` を返す。
  - 取り出したtask idのtask nameを読み取り、`[sem-wq] dequeue` を出力する。
- `int sig_sem(int sem_id)`
  - wait queue優先でdequeueする。
  - dequeue成功時はcountを増やさず、READY復帰だけを行う。
  - queue空時のみcount-upする。

### Task Layer

- `int task_wake_waiting_on_sem_by_id(int task_id, int sem_id)`
  - 指定taskが `WAITING` かつ `wait_sem_id == sem_id` の場合だけREADYへ戻す。
  - READY復帰時に `wait_sem_id = 0` へ戻す。
  - `[task] ready: ... wait_sem_id=none state=READY` を出力する。
- 既存の `task_find_waiting_on_sem()` と `task_wake_one_waiting_on_sem()` は12.2の探索経路であり、12.3の `sig_sem()` からは使用しない。必要なら互換用に残すが、新経路ではwait queueを正とする。

### ITRON API Layer

- `wai_sem(int sem_id)`
  - `task_mark_waiting_on_sem()` 成功後に `sem_enqueue_waiter()` を呼ぶ。
  - enqueue失敗時はエラーログを出し、以降のscheduler/dispatcher接続へ進まない。
  - 10.4の `yield_tsk()` 経路は変更しない。

### Kernel Smoke

- 12.3の通常smokeは、`wai_sem()` でwait queue enqueueを観測し、続く `sig_sem()` でdequeueとREADY復帰を観測する。
- 待ちtaskなしの `sig_sem()` では `[sem-wq] empty` とcount-upを観測する。
- FIFOは最低限、enqueue/dequeueログのqueue_countとtask id順で確認する。

## File Structure Plan

- `kernel/include/semaphore.h`: `semaphore_t` のwait queue field、`SEM_WAIT_QUEUE_EMPTY`、`sem_enqueue_waiter()`、`sem_dequeue_waiter()` の宣言とDoxygen更新。
- `kernel/semaphore.c`: wait queue初期化、enqueue/dequeue helper、`sig_sem()` のwait queue経由化、Doxygen更新。
- `kernel/include/task.h`: `task_wake_waiting_on_sem_by_id()` の宣言とDoxygen追加。
- `kernel/task.c`: dequeue済みtask idをREADYへ戻すhelperを追加し、`wait_sem_id` clearログを維持。
- `kernel/itron_api.c`: `wai_sem()` WAITING化後のenqueue接続とDoxygen更新。
- `kernel/kernel.c`: 12.3 smoke説明コメントを更新し、既存の `wai_sem()` / `sig_sem()` smokeでwait queueログを観測できるようにする。
- `README.md`: 12.3到達点、未実装項目、Zenn tag候補を更新。
- `docs/logs/qemu-serial.log`: `make run` の最新出力で更新。
- `.kiro/specs/semaphore-wait-queue/requirements.md`, `design.md`, `tasks.md`: spec成果物。最終状態ではこの3ファイルだけを残す。

## Testing Strategy

- `make` で通常buildが通ることを確認する。
- `make run` で `wai_sem()` のWAITING化、`[sem-wq] enqueue`、`sig_sem()` の `[sem-wq] dequeue`、READY復帰、`wait_sem_id=none`、wakeup時count非増加、queue空時count-upを確認する。
- `make run VALIDATE_TIMER_IRQ_ENTRY=1` で11.4のdispatch pending request/consume/clearログ経路が維持されることを確認する。
- `rg` でtimer IRQ handler本体から `sig_sem()`、`wai_sem()`、`yield_tsk()`、`dispatcher_switch_to()` を直接呼んでいないことを確認する。
- `.kiro/specs/semaphore-wait-queue/` が最終的に3ファイルだけであることを確認する。
