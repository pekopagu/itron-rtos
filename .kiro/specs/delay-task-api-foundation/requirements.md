# Requirements Document

## Project Description

μITRON風RTOSを学習目的で段階的に開発している開発者は、12.1〜12.4で `wai_sem()` によるセマフォ待ち、`sig_sem()` によるREADY復帰、wait queue、wakeup後preemptionを観測できるようになった。第13章13.1では、セマフォ待ちとは別に時間待ちを扱うため、`dly_tsk(uint32_t delay_ticks)` 風APIの入口を追加し、RUNNING current taskをdelay理由のWAITINGへ落として、次READY taskへの既存dispatcher/context switch境界に進めるようにする。

## Boundary Context

- **In scope**: `dly_tsk(uint32_t delay_ticks)` 風API入口、`delay_ticks == 0` のエラー扱い、RUNNING current taskのdelay WAITING化、semaphore待ちと区別できる観測情報、WAITING化後の次READY選択、既存dispatcher/context switch境界への接続、README/Doxygen/log/spec成果物更新。
- **Out of scope**: sleep/delay queue、tickごとのdelay decrement、tick到達時READY復帰、timeout付き `twai_sem`、`slp_tsk` / `wup_tsk`、priority順delay queue、同一優先度time slice、round-robin、timer IRQ handlerからの `dly_tsk()` / semaphore API / `yield_tsk()` / `dispatcher_switch_to()` 呼び出し。
- **Adjacent expectations**: 12.1〜12.4の `wai_sem()` / `sig_sem()` / semaphore FIFO wait queue / wakeup switch、10.4の `yield_tsk()` 協調switch、11.4のtimer IRQ preemption / dispatch pendingログ仕様を維持する。

## Requirements

### Requirement 1: `dly_tsk()` API入口と入力検証

**Objective:** RTOS学習者として、時間待ちAPIの入口と無効なdelay指定の扱いをログから確認したい。これにより、13.1がdelay queueではなくAPI入口とWAITING化の基礎であることを明確にできる。

#### Acceptance Criteria

1. When `dly_tsk(delay_ticks)` is called with a RUNNING current task, the system shall log the call with `delay_ticks`, current task id, name, and state.
2. When `delay_ticks == 0`, the system shall reject the request as invalid delay.
3. When `delay_ticks == 0`, the system shall not change the current task state or any delay observation field.
4. When `delay_ticks == 0`, the system shall complete with result `-1` and action `invalid-delay`.
5. When current task is absent or not RUNNING, the system shall reject the request as invalid current state.

### Requirement 2: RUNNING current taskのdelay WAITING化

**Objective:** RTOS学習者として、`dly_tsk()` がRUNNING current taskを時間待ちとしてWAITINGへ落とすことを確認したい。これにより、セマフォ待ちとは別の待ち理由を導入する準備ができる。

#### Acceptance Criteria

1. When `delay_ticks > 0` and current task is RUNNING, the system shall transition only that current task from RUNNING to WAITING.
2. When the task enters delay WAITING, the system shall record delay wait observation data that includes the remaining delay tick value.
3. When the task enters delay WAITING, the system shall record a wait reason that is distinct from semaphore waiting.
4. When the task enters delay WAITING, the system shall not set `wait_sem_id` to a semaphore id.
5. When the task enters delay WAITING, the system shall log that the task is delay waiting with id, name, delay tick value, and WAITING state.

### Requirement 3: delay待ちとsemaphore待ちの分離

**Objective:** RTOS学習者として、delay待ちtaskが `wai_sem()` / `sig_sem()` / semaphore FIFO wait queueに混ざらないことを確認したい。これにより、12章で作った同期待ちの意味を壊さずに時間待ちを追加できる。

#### Acceptance Criteria

1. When `wai_sem()` moves a task to WAITING, the system shall continue to identify that task as semaphore waiting.
2. When `dly_tsk()` moves a task to WAITING, the system shall identify that task as delay waiting and not semaphore waiting.
3. When `sig_sem()` wakes a semaphore waiter, the system shall not wake a task whose wait reason is delay.
4. When `wai_sem()` enqueues a waiting task, the system shall not enqueue a delay waiting task into the semaphore wait queue.
5. When task dump or logs show WAITING tasks, the system shall expose enough information to distinguish semaphore waiting from delay waiting.

### Requirement 4: WAITING化後のREADY選択とswitch境界接続

**Objective:** RTOS学習者として、`dly_tsk()` がcurrent taskをWAITINGへ落とした後、既存schedulerとdispatcher/context switch境界に進むことを確認したい。これにより、時間待ち入口が既存のタスク切替モデルと接続される。

#### Acceptance Criteria

1. When `dly_tsk()` transitions current to WAITING, the system shall ask the scheduler for the next READY task.
2. When the scheduler returns a READY task, the system shall log the selected task id, name, priority, and state.
3. When a READY task is selected, the system shall call the existing dispatcher switch boundary with the delay-waiting task as `from` and the selected READY task as `to`.
4. When the dispatcher switch returns, the system shall log switch begin, switch end, result, and completed action `delay-switch`.
5. When no READY task exists, the system shall log that no next READY task exists and safely return without inventing idle scheduling.

### Requirement 5: 既存経路の維持

**Objective:** RTOS学習者として、13.1の追加後も12章のセマフォ待ち、10.4のyield協調switch、11.4のtimer IRQ preemption / dispatch pending観測が壊れていないことを確認したい。これにより、新しいdelay入口の影響範囲を限定できる。

#### Acceptance Criteria

1. When normal build is executed, the system shall build successfully.
2. When normal `make run` is executed, the system shall complete the boot-time smoke log including delay WAITING evidence and existing semaphore evidence.
3. When `make run VALIDATE_TIMER_IRQ_ENTRY=1` is executed, the system shall preserve timer IRQ preemption / dispatch pending behavior.
4. While timer IRQ handler body runs, the system shall not call `dly_tsk()`, `wai_sem()`, `sig_sem()`, `yield_tsk()`, or `dispatcher_switch_to()` directly.
5. When the existing yield and semaphore smoke paths run, the system shall preserve their switch and wakeup behavior.

### Requirement 6: 成果物と未実装範囲の明示

**Objective:** RTOS学習者として、第13章13.1の到達点と未実装範囲をREADME、Doxygenコメント、serial log、specから確認したい。これにより、delay API基礎と今後のtimer連携を混同しない。

#### Acceptance Criteria

1. When implementation is complete, README shall describe Chapter 13 Section 13.1 and remaining out-of-scope items.
2. When implementation is complete, changed public interfaces and meaningful helpers shall have Japanese Doxygen comments describing purpose, assumptions, limitations, and non-goals.
3. When validation is complete, `docs/logs/qemu-serial.log` shall contain fresh `make run` output including delay WAITING evidence.
4. When implementation is complete, `.kiro/specs/delay-task-api-foundation/` shall contain only `requirements.md`, `design.md`, and `tasks.md`.
