# Requirements Document

## Project Description

μITRON風RTOSを学習目的で段階的に実装している開発者は、第6章6.1で作ったセマフォ基盤がまだ実行中タスクの切り替え境界へ接続されていないため、取得できないセマフォを待ったRUNNING taskをWAITINGへ落とし、次のREADY taskへ進む入口を観測できるようにしたい。

## Requirements

### Requirement 1: `wai_sem()` task文脈API

**User Story:** RTOS学習者として、RUNNING current taskから呼ぶ`wai_sem()`風APIを観測したい。そうすることで、セマフォ待ち入りがtask文脈のAPIでありtimer IRQ handlerの責務ではないことを確認できる。

#### Acceptance Criteria

1. WHEN `wai_sem()` is called THEN the system SHALL log sem_id, current task id, name, and state.
2. IF current task does not exist THEN the system SHALL reject the call as an error.
3. IF current task is not RUNNING THEN the system SHALL reject the call as an error.
4. WHILE timer IRQ handler is running THEN the system SHALL NOT call `wai_sem()`.

### Requirement 2: セマフォ取得成功時のno-switch

**User Story:** RTOS学習者として、セマフォcountが残っている場合はtask切り替えなしで取得成功することを確認したい。そうすることで、待ち入り時だけ状態遷移とdispatchが始まることを切り分けられる。

#### Acceptance Criteria

1. GIVEN target semaphore count is greater than 0 WHEN `wai_sem()` is called THEN the system SHALL decrement the count by 1.
2. GIVEN target semaphore count is greater than 0 WHEN `wai_sem()` completes THEN the system SHALL keep current task RUNNING.
3. GIVEN target semaphore count is greater than 0 WHEN `wai_sem()` completes THEN the system SHALL NOT call `dispatcher_switch_to()`.
4. WHEN acquisition succeeds THEN the system SHALL log the count before and after, result 0, and action `no-switch`.

### Requirement 3: セマフォ待ち入り時のRUNNINGからWAITINGへの遷移

**User Story:** RTOS学習者として、取得できないセマフォを待ったRUNNING current taskがWAITINGへ落ちることを確認したい。そうすることで、第6章のセマフォ状態とtask状態遷移を接続できる。

#### Acceptance Criteria

1. GIVEN target semaphore count is 0 WHEN `wai_sem()` is called by a RUNNING current task THEN the system SHALL transition that task from RUNNING to WAITING.
2. WHEN the task transitions to WAITING THEN the system SHALL set the task wait semaphore id to the target semaphore id.
3. WHEN the task transitions to WAITING THEN the system SHALL log `RUNNING->WAITING` with current task id and name.
4. IF no READY task exists after WAITING transition THEN the system SHALL log an unsupported no-ready condition and SHALL NOT call `dispatcher_switch_to()`.

### Requirement 4: WAITING後の次READY task選択とdispatcher接続

**User Story:** RTOS学習者として、WAITING化後にschedulerがWAITING taskを候補から除外し、次のREADY taskへdispatcher境界経由で進むことを確認したい。

#### Acceptance Criteria

1. WHEN current task becomes WAITING THEN the system SHALL call scheduler selection for the next READY task.
2. WHEN scheduler selects the next task THEN the system SHALL select only READY tasks and SHALL NOT include WAITING tasks.
3. IF a READY task exists THEN the system SHALL call `dispatcher_switch_to()` with the WAITING current task as from and the selected READY task as to.
4. WHEN switch starts and ends THEN the system SHALL log wai-sem switch begin/end and preserve existing dispatcher boundary begin/end logs.

### Requirement 5: 既存経路の維持

**User Story:** RTOS学習者として、12.1のセマフォ待ち入り接続が10.4の協調yield経路と11.4のtimer IRQ preemptionログ経路を壊していないことを確認したい。

#### Acceptance Criteria

1. WHEN normal build is executed THEN the system SHALL build successfully.
2. WHEN normal `make run` is executed THEN the system SHALL complete the boot-time smoke log.
3. WHEN `make run VALIDATE_TIMER_IRQ_ENTRY=1` is executed THEN the system SHALL preserve dispatch pending request, consume, and clear log ordering.
4. WHILE timer IRQ handler body runs THEN the system SHALL NOT call `yield_tsk()` directly.
5. WHILE timer IRQ handler body runs THEN the system SHALL NOT call `dispatcher_switch_to()` directly.

### Requirement 6: 文書とログ

**User Story:** RTOS学習者として、第12章12.1の到達点と未実装範囲をREADME、spec、Doxygenコメント、QEMU serial logで確認したい。

#### Acceptance Criteria

1. WHEN implementation is complete THEN README SHALL describe Chapter 12 Section 12.1 and the remaining out-of-scope items.
2. WHEN implementation is complete THEN source comments SHALL describe that this is not `sig_sem()` wakeup, wait queue, timeout, time slice, or round-robin implementation.
3. WHEN implementation is complete THEN `docs/logs/qemu-serial.log` SHALL include the updated smoke output.
4. WHEN implementation is complete THEN `.kiro/specs/wai-sem-running-to-waiting/` SHALL contain only `requirements.md`, `design.md`, and `tasks.md`.
