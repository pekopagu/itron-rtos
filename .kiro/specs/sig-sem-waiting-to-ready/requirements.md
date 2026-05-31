# Requirements Document

## Project Description

μITRON風RTOSを学習目的で段階的に開発している開発者は、12.1で `wai_sem()` によりRUNNINGからWAITINGへ落ちたtaskをまだREADYへ戻せない。第12章12.2では、task文脈の `sig_sem()` 風APIにより、対象セマフォを待つWAITING taskを1件だけREADYへ戻し、待ちtaskがいない場合だけsemaphore countを増やす挙動を、boot-time verification modelとして観測できるようにする。

## Requirements

### Requirement 1: `sig_sem()` task文脈API入口

**User Story:** RTOS学習者として、task文脈から呼ぶ `sig_sem()` 風APIを観測したい。そうすることで、セマフォ返却と待ちtask復帰がtimer IRQ handlerではなくtask文脈APIの責務であることを確認できる。

#### Acceptance Criteria

1. WHEN `sig_sem()` is called THEN the system SHALL log the target `sem_id`.
2. IF the target semaphore does not exist THEN the system SHALL reject the call without changing task state or semaphore count.
3. WHILE timer IRQ handler body runs THEN the system SHALL NOT call `sig_sem()`.

### Requirement 2: WAITING taskの検索とREADY復帰

**User Story:** RTOS学習者として、対象セマフォを待つWAITING taskがREADYへ戻ることを確認したい。そうすることで、12.1の `wai_sem()` によるWAITING化から12.2の復帰入口までの最小状態遷移をつなげて観測できる。

#### Acceptance Criteria

1. GIVEN a task is `WAITING` on the target semaphore WHEN `sig_sem()` is called THEN the system SHALL find one matching task.
2. WHEN a matching WAITING task is found THEN the system SHALL transition that task from `WAITING` to `READY`.
3. WHEN the task becomes READY THEN the system SHALL clear its `wait_sem_id` to the non-waiting state.
4. WHEN a WAITING task is returned to READY THEN the system SHALL log the found task, the READY state, and the `WAITING->READY` action.
5. WHEN a WAITING task is returned to READY THEN the system SHALL NOT increment the semaphore count.

### Requirement 3: 待ちtaskなし時のcount増加

**User Story:** RTOS学習者として、待ちtaskがいない場合に `sig_sem()` がsemaphore countを増やすことを確認したい。そうすることで、wakeup経路とcount-up経路がログ上で明確に区別できる。

#### Acceptance Criteria

1. GIVEN no task is `WAITING` on the target semaphore WHEN `sig_sem()` is called THEN the system SHALL log that no waiting task exists.
2. GIVEN no waiting task exists WHEN the semaphore count can be increased THEN the system SHALL increment the count by 1.
3. WHEN the count is incremented THEN the system SHALL log the count before and after.
4. WHEN the count is incremented THEN the system SHALL complete with action `count-up`.

### Requirement 4: 既存経路の維持

**User Story:** RTOS学習者として、12.2の追加が既存の協調switch、timer IRQ preemptionログ、12.1のWAITING化を壊していないことを確認したい。

#### Acceptance Criteria

1. WHEN normal build is executed THEN the system SHALL build successfully.
2. WHEN normal `make run` is executed THEN the system SHALL complete the boot-time smoke log.
3. WHEN `make run VALIDATE_TIMER_IRQ_ENTRY=1` is executed THEN the system SHALL preserve dispatch pending request, consume, and clear log ordering.
4. WHILE timer IRQ handler body runs THEN the system SHALL NOT call `wai_sem()`, `sig_sem()`, `yield_tsk()`, or `dispatcher_switch_to()` directly.

### Requirement 5: 文書と成果物

**User Story:** RTOS学習者として、第12章12.2の到達点と未実装範囲をREADME、spec、Doxygenコメント、QEMU serial logから確認したい。

#### Acceptance Criteria

1. WHEN implementation is complete THEN README SHALL describe Chapter 12 Section 12.2 and the remaining out-of-scope items.
2. WHEN implementation is complete THEN source comments SHALL describe that wait queue, FIFO/priority ordering, wakeup-after-preemption, timeout wait, same-priority time slice, and round-robin are not implemented yet.
3. WHEN implementation is complete THEN `docs/logs/qemu-serial.log` SHALL include the updated smoke output.
4. WHEN implementation is complete THEN `.kiro/specs/sig-sem-waiting-to-ready/` SHALL contain only `requirements.md`, `design.md`, and `tasks.md`.
