# Design Document

## Overview

第12章12.1では、既存のセマフォ基盤をtask文脈の`wai_sem()`入口へ接続する。取得成功時はcountを減らしてswitchしない。取得不能時はdispatcherが保持するcurrent taskをRUNNINGからWAITINGへ落とし、その後schedulerで次のREADY taskを選んで既存の`dispatcher_switch_to()`境界へ進む。

このspecはboot-time verification modelの拡張であり、実CPU実行状態としての完全なブロック、wait queue、`sig_sem()`による復帰、timeout、wakeup後preemption、同一優先度time slice、round-robinは扱わない。

## Boundary Commitments

### Owned

- `wai_sem(int sem_id)`をtask文脈APIとして公開する。
- セマフォcount取得成功時のno-switchログを整える。
- count 0時にRUNNING current taskをWAITINGへ遷移させる。
- WAITING化後のscheduler選択と`dispatcher_switch_to()`接続を行う。
- `dispatcher_switch_to()`が12.1のWAITING from taskを切替境界として受け付ける。
- README、Doxygenコメント、QEMU log、spec文書を更新する。

### Out of Boundary

- `sig_sem()`による待ちtask復帰の新規実装または高度化。
- wait queue、FIFO/priority queue、timeout付き`twai_sem`。
- semaphore wakeup後のpreemption判定。
- sleep/delay queue、nested interrupt、同一優先度time slice、round-robin。
- timer IRQ handler本体からの`wai_sem()`、`yield_tsk()`、`dispatcher_switch_to()`直接呼び出し。
- 既存RTOS実装の参照、コピー、翻訳。

### Allowed Dependencies

- `itron_api.c`は`dispatcher_get_current()`、`scheduler_select_next()`、`dispatcher_switch_to()`、task accessor、semaphore helperを使う。
- `semaphore.c`はセマフォtableとcount操作を所有する。
- `task.c`はTCB stateと`wait_sem_id`を所有する。
- `dispatcher.c`はswitch boundaryとcurrent task更新を所有する。

## Architecture

```text
wai_sem(sem_id)
  -> dispatcher_get_current()
  -> semaphore lookup / count check
     count > 0:
       decrement count
       log acquired/completed no-switch
     count == 0:
       task_mark_waiting_on_sem(current->id, sem_id)
       scheduler_select_next()
       if next READY:
         dispatcher_switch_to(current, next)
       else:
         log unsupported no-ready
```

`scheduler_select_next()`は既存通り`TASK_STATE_READY`のみを候補にするため、WAITING化されたtaskは候補に含まれない。`yield_tsk()`はRUNNING->READY後に同じ`dispatcher_switch_to()`へ進むため、dispatcherはREADY fromとWAITING fromの両方を明示的な上位API由来のfrom状態として受け付ける。

## Components and Interfaces

### ITRON API Layer

- `int wai_sem(int sem_id)`
  - currentがNULLまたはRUNNING以外なら負値を返す。
  - countがある場合はセマフォcountを減らし、switchしない。
  - countが0の場合はcurrentをWAITINGへ落とし、次READY taskがあればdispatcherへ進む。

### Semaphore Layer

- `const semaphore_t *sem_get_by_id(int sem_id)`
- `int sem_take_if_available(int sem_id, int *count_before, int *count_after)`

セマフォtable探索とcount更新だけを担当し、scheduler/dispatcherには依存しない。

### Task Layer

- `task_mark_waiting_on_sem()`をRUNNING current task待ち入りで使う。
- コメントを12.1向けに更新し、READYからの旧6.1観測待ち入りは主経路ではないことを明記する。

### Dispatcher Layer

- `dispatcher_switch_to()`はfromがRUNNING、READY、WAITINGのいずれかであることを受け付ける。
- RUNNING fromだけをREADYへ戻す既存動作は維持する。
- WAITING fromは12.1で既にtask API層が状態遷移済みのため、dispatcherでは追加状態変更しない。

## File Structure Plan

- `kernel/include/itron_api.h`: `wai_sem()`の公開宣言と戻り値定義を追加。
- `kernel/itron_api.c`: task文脈`wai_sem()`本体とログを追加。
- `kernel/include/semaphore.h`: task_id付き`wai_sem()`宣言を削除し、count helperを追加。
- `kernel/semaphore.c`: セマフォcount helperを追加し、旧task_id付き`wai_sem()`を廃止。
- `kernel/dispatcher.c`, `kernel/include/dispatcher.h`: WAITING fromを受け付ける説明と検証を更新。
- `kernel/kernel.c`: 12.1 smokeをcurrent task文脈で実行するよう更新。
- `README.md`: 12.1到達点とZenn tag候補を追加。
- `docs/logs/qemu-serial.log`: `make run`結果で更新。
- `.kiro/specs/wai-sem-running-to-waiting/requirements.md`, `design.md`, `tasks.md`: spec成果物。

## Testing Strategy

- `make`で通常buildを確認する。
- `make run`で取得成功no-switch、取得不能WAITING、次READYへのswitchログを確認する。
- `make run VALIDATE_TIMER_IRQ_ENTRY=1`で11.4のdispatch pendingログ経路が維持されることを確認する。
- `rg`でtimer IRQ handler本体が`wai_sem()`、`yield_tsk()`、`dispatcher_switch_to()`を直接呼ばないことを確認する。
- `.kiro/specs/wai-sem-running-to-waiting/`のファイル数が3つだけであることを確認する。
