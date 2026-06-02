# Design Document

## Overview

第13章13.1では、μITRON風 `dly_tsk(uint32_t delay_ticks)` の最小入口を追加する。`dly_tsk()` はtask文脈APIとして扱い、dispatcherが保持するcurrent taskがRUNNINGで、かつ `delay_ticks > 0` の場合だけ、そのtaskをdelay理由のWAITINGへ遷移させる。

このspecはdelay queueやtick連動復帰を実装しない。13.1の到達点は、セマフォ待ちとは別の待ち理由とdelay tick値をTCB上で観測でき、WAITING化後に既存schedulerで次READY taskを選び、既存dispatcher/context switch境界へ進めることである。

## Goals

- `dly_tsk(uint32_t delay_ticks)` 風APIを公開する。
- `delay_ticks == 0` をエラーとして扱い、task状態を変更しない。
- RUNNING current taskをdelay WAITINGへ遷移させる。
- `wait_sem_id` とは別に、待ち理由とdelay残tickを観測できるようにする。
- WAITING化後に既存scheduler/dispatcher switch境界へ進む。
- `wai_sem()` / `sig_sem()` / semaphore FIFO wait queue / wakeup switch / `yield_tsk()` / timer IRQ preemption経路を維持する。

## Non-Goals

- sleep/delay queue
- tickごとのdelay decrement
- tick到達時READY復帰
- timeout付き `twai_sem`
- `slp_tsk` / `wup_tsk`
- priority順delay queue
- 同一優先度time slice
- round-robin
- timer IRQ handlerからの `dly_tsk()` 呼び出し
- timer IRQ handlerからの `wai_sem()` / `sig_sem()` / `yield_tsk()` / `dispatcher_switch_to()` 直接呼び出し

## Boundary Commitments

### This Spec Owns

- `itron_api.h` / `itron_api.c` の `dly_tsk(uint32_t delay_ticks)` API。
- TCBの待ち理由観測情報とdelay残tick観測情報。
- RUNNING current taskからdelay WAITINGへの状態遷移API。
- `dly_tsk()` のログ、次READY選択、dispatcher switch接続。
- README、Doxygenコメント、`docs/logs/qemu-serial.log`、spec成果物更新。

### Out of Boundary

- delay WAITING taskをREADYへ戻すtimer処理。
- delay queueやtimeout queueのデータ構造。
- semaphore wait queueへのdelay task登録。
- `sig_sem()` によるdelay task wakeup。
- IRQ handler内の直接dispatchやtask文脈API呼び出し。
- schedulerのREADY選択ルール変更。

### Allowed Dependencies

- `itron_api.c` は既存の `dispatcher_get_current()`、`scheduler_select_next()`、`dispatcher_switch_to()`、`task_get_mutable_by_id()` を利用してよい。
- `itron_api.c` はdelay WAITING化を `task.c` の新しいtask状態遷移APIへ委譲する。
- `scheduler.c` は既存どおり `TASK_STATE_READY` だけを選ぶ。delay WAITINGのためにschedulerルールを増やさない。
- `semaphore.c` は必要最小限の防御として、semaphore wait queueに入れるtaskがsemaphore waitingであることを確認してよい。

### Revalidation Triggers

- `tcb_t` の待ち理由フィールド名や意味を変更した場合。
- `task_mark_waiting_on_sem()` / `task_wake_waiting_on_sem_by_id()` の契約を変更した場合。
- `scheduler_select_next()` のREADY選択条件を変更した場合。
- timer IRQ handlerがtask文脈APIやdispatcherを直接呼ぶようになった場合。

## Architecture

### Existing Architecture Analysis

10.4の `yield_tsk()` はRUNNING current taskをREADYへ戻し、schedulerで次READYを選び、`dispatcher_switch_to()` に接続する。12.1の `wai_sem()` はsemaphore countが0のときRUNNING current taskをWAITINGへ落とし、12.3で対象semaphoreのFIFO wait queueへtask idを登録する。12.4の `sig_sem()` はsemaphore wait queueからdequeueしたtaskだけをREADYへ戻し、必要ならwakeup switchへ進む。

13.1の `dly_tsk()` は `wai_sem()` と似た「RUNNING current taskをWAITINGへ落として次READYへswitchする」形を使う。ただし、semaphore countやsemaphore wait queueには触れない。TCBには `wait_reason` と `delay_ticks_remaining` を追加し、`wait_sem_id` はsemaphore待ち専用の観測値として維持する。

### Architecture Pattern & Boundary Map

```text
dly_tsk(delay_ticks)
  -> validate delay_ticks > 0
  -> current = dispatcher_get_current()
  -> validate current != NULL && current->state == RUNNING
  -> task_mark_waiting_on_delay(current->id, delay_ticks)
       state = WAITING
       wait_reason = TASK_WAIT_REASON_DELAY
       delay_ticks_remaining = delay_ticks
       wait_sem_id = 0
  -> next = scheduler_select_next()
       READY task only
  -> if next exists:
       dispatcher_switch_to(current_mutable, next_mutable)
       action=delay-switch
     else:
       action=no-ready-task
```

### Wait Reason Model

`task_wait_reason_t` を追加する。

- `TASK_WAIT_REASON_NONE`: 待ちなし。READY/RUNNING/DORMANT/UNUSEDで使う。
- `TASK_WAIT_REASON_SEMAPHORE`: `wai_sem()` によるsemaphore待ち。
- `TASK_WAIT_REASON_DELAY`: `dly_tsk()` によるdelay待ち。

`task_mark_waiting_on_sem()` は `wait_reason=TASK_WAIT_REASON_SEMAPHORE`、`wait_sem_id=sem_id`、`delay_ticks_remaining=0` を設定する。`task_wake_waiting_on_sem_by_id()` は `TASK_WAIT_REASON_SEMAPHORE` かつ `wait_sem_id` 一致を要求し、READY復帰時に `wait_reason=NONE`、`wait_sem_id=0`、`delay_ticks_remaining=0` に戻す。`task_mark_waiting_on_delay()` は `wait_reason=TASK_WAIT_REASON_DELAY`、`wait_sem_id=0`、`delay_ticks_remaining=delay_ticks` を設定する。

## File Structure Plan

### Modified Files

- `kernel/include/task.h`: `task_wait_reason_t`、TCB観測フィールド、delay WAITING遷移API、wait reason accessorを追加する。
- `kernel/task.c`: TCB初期化、登録ログ、dump、semaphore WAITING/READY復帰、delay WAITING遷移を更新する。
- `kernel/include/itron_api.h`: `dly_tsk()` 宣言、戻り値定義、Doxygenコメントを追加する。
- `kernel/itron_api.c`: `dly_tsk()` 本体、ログ、READY選択、dispatcher switch接続を追加する。
- `kernel/semaphore.c`: semaphore wait queue enqueue/wakeupがdelay待ちを扱わない防御確認を追加する。
- `kernel/kernel.c`: 13.1用delay smoke taskと起動時検証フローを追加する。
- `README.md`: Chapter 13 Section 13.1、Zenn tag候補、未実装範囲、Doxygen説明を更新する。
- `docs/logs/qemu-serial.log`: fresh `make run` 出力で更新する。
- `.kiro/specs/delay-task-api-foundation/requirements.md`, `design.md`, `tasks.md`: 最終成果物として3ファイルだけを残す。

## Components and Interfaces

| Component | Domain | Intent | Requirements | Contracts |
| --- | --- | --- | --- | --- |
| DelayTaskAPI | ITRON-like API Layer | `dly_tsk()` の入力検証、delay WAITING化、switch接続 | 1, 2, 4 | Service |
| TaskWaitReasonState | Task Management | semaphore待ちとdelay待ちの観測分離 | 2, 3 | State |
| SemaphoreWaitIsolation | Semaphore Layer | delay待ちtaskをsemaphore wait queue/wakeup対象にしない | 3, 5 | Guard |
| DocumentationEvidence | Docs | 13.1到達点と未実装範囲を記録する | 6 | Document |

### DelayTaskAPI

**Responsibilities & Constraints**

- `delay_ticks == 0` をエラーとして扱う。
- current taskがRUNNINGであることを確認する。
- delay WAITING化後にschedulerへ次READY選択を依頼する。
- 次READYがあれば既存 `dispatcher_switch_to()` へ進む。
- 次READYがなければログを出して安全に戻る。
- timer IRQ handlerやdispatch pending moduleには依存しない。

**Service Contract**

- Preconditions: task文脈から呼ばれ、dispatcher currentがRUNNINGであること。
- Postconditions: 成功時、呼び出し元current taskはdelay WAITINGとなり、次READY taskがあればdispatcher switch境界が呼ばれる。
- Invariants: `wait_sem_id` はdelay待ちでは0のまま。delay WAITING taskはsemaphore wait queueへ入らない。

### TaskWaitReasonState

**Responsibilities & Constraints**

- TCBに待ち理由とdelay残tickを保持する。
- READY/RUNNING化されるtaskの待ち情報をNONEへ戻す。
- task dumpで待ち理由とdelay残tickを観測可能にする。
- tick decrementやREADY復帰は行わない。

## Requirements Traceability

| Requirement | Summary | Components | Interfaces | Flows |
| --- | --- | --- | --- | --- |
| 1.1, 1.2, 1.3, 1.4, 1.5 | `dly_tsk()` API入口と入力検証 | DelayTaskAPI | `dly_tsk()` | invalid/current validation |
| 2.1, 2.2, 2.3, 2.4, 2.5 | delay WAITING化 | DelayTaskAPI, TaskWaitReasonState | `task_mark_waiting_on_delay()` | delay wait flow |
| 3.1, 3.2, 3.3, 3.4, 3.5 | semaphore待ちとの分離 | TaskWaitReasonState, SemaphoreWaitIsolation | task/semaphore APIs | wait isolation |
| 4.1, 4.2, 4.3, 4.4, 4.5 | READY選択とswitch接続 | DelayTaskAPI | scheduler/dispatcher | delay switch flow |
| 5.1, 5.2, 5.3, 5.4, 5.5 | 既存経路維持 | DelayTaskAPI, SemaphoreWaitIsolation | build/run/timer validation | regression |
| 6.1, 6.2, 6.3, 6.4 | 成果物と未実装範囲 | DocumentationEvidence | README/log/spec | documentation |

## Testing Strategy

- `make` で通常buildが通ることを確認する。
- `make run` で `dly_tsk()` のinvalid-delay、delay WAITING化、次READY選択、dispatcher switch、task dump上の `wait_reason=delay` と `delay_ticks_remaining` を確認する。
- `make run` で `wai_sem()` のWAITING化、semaphore FIFO wait queue、`sig_sem()` のWAITING->READY、wakeup switchが維持されることを確認する。
- `make run VALIDATE_TIMER_IRQ_ENTRY=1` でtimer IRQ preemption / dispatch pending経路が維持されることを確認する。
- `rg` でtimer IRQ handler本体から `dly_tsk()` / `wai_sem()` / `sig_sem()` / `yield_tsk()` / `dispatcher_switch_to()` を直接呼んでいないことを確認する。
- `.kiro/specs/delay-task-api-foundation/` が最終的に `requirements.md`、`design.md`、`tasks.md` の3ファイルだけであることを確認する。
