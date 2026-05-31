# Requirements Document

## Project Description

μITRON風RTOSを学習目的で段階的に開発している開発者は、第12章12.1で `wai_sem()` によりRUNNING taskをWAITINGへ落とし、第12章12.2で `sig_sem()` によりWAITING taskをREADYへ戻す経路を観測できるようになった。ただし12.2の `sig_sem()` はtask table全体を走査して待ちtaskを探しており、対象semaphoreごとの待ち行列という責務がまだ表現されていない。第12章12.3では、対象semaphoreにFIFO wait queueを持たせ、`wai_sem()` でenqueueし、`sig_sem()` でdequeueしたtaskだけをREADYへ戻す経路へ整理する。

## Boundary Context

- **In scope**: semaphoreごとの固定長FIFO wait queue、`wai_sem()` のWAITING化時enqueue、`sig_sem()` のdequeueによるWAITING->READY、`wait_sem_id` clear、ログ、README、Doxygen、QEMU serial log更新。
- **Out of scope**: priority順wait queue、timeout付き `twai_sem`、sleep/delay queue、12.4のwakeup後preemption、`sig_sem()` 直後の即時context switch、nested interrupt、同一優先度time slice、round-robin、完全な割り込み復帰フレーム切替。
- **Adjacent expectations**: 10.4の `yield_tsk()` 協調context switch経路、11.4のtimer IRQ preemption / dispatch pendingログ安定化経路、12.1の `wai_sem()` RUNNING->WAITING経路を壊さない。timer IRQ handler本体から `sig_sem()`、`wai_sem()`、`yield_tsk()`、`dispatcher_switch_to()` を直接呼ばない。

## Requirements

### Requirement 1: semaphoreごとのFIFO wait queue

**User Story:** RTOS学習者として、semaphoreごとの待ちtaskを明示的なFIFO queueで観測したい。そうすることで、12.2までのtask table全体走査ではなく、対象semaphoreの責務として待ち解除の入口を理解できる。

#### Acceptance Criteria

1. WHEN semaphore table is initialized THEN the system SHALL initialize each semaphore wait queue to empty.
2. WHEN a semaphore is created THEN the system SHALL provide an empty FIFO wait queue for that semaphore.
3. WHEN wait queue state changes THEN the system SHALL log enqueue, dequeue, and empty states with sem_id, task id/name when applicable, and queue_count.
4. The system SHALL keep wait queue storage fixed length or task id array based for this learning implementation.

### Requirement 2: `wai_sem()`によるwait queue enqueue

**User Story:** RTOS学習者として、`wai_sem()` で取得できないRUNNING taskがWAITINGになった時点で対象semaphoreのwait queueへ登録されることを確認したい。そうすることで、待ち状態と待ち行列の対応をログで追跡できる。

#### Acceptance Criteria

1. GIVEN target semaphore count is 0 WHEN `wai_sem()` is called by a RUNNING current task THEN the system SHALL transition that task from RUNNING to WAITING.
2. WHEN the task transitions to WAITING THEN the system SHALL set the task wait semaphore id to the target semaphore id.
3. WHEN WAITING transition succeeds THEN the system SHALL enqueue that task id to the target semaphore wait queue.
4. WHEN enqueue succeeds THEN the system SHALL log `[sem-wq] enqueue` with sem_id, task id, task name, and queue_count.
5. IF enqueue fails THEN the system SHALL reject the wait path with an observable error log and SHALL NOT silently continue.

### Requirement 3: `sig_sem()`によるwait queue dequeueとREADY復帰

**User Story:** RTOS学習者として、`sig_sem()` がtask table全体を探すのではなく、対象semaphoreのwait queueから1 taskだけ取り出してREADYへ戻すことを確認したい。そうすることで、semaphore単位の待ち解除責務を理解できる。

#### Acceptance Criteria

1. GIVEN target semaphore wait queue is not empty WHEN `sig_sem()` is called THEN the system SHALL dequeue one task id from that wait queue.
2. WHEN a task id is dequeued THEN the system SHALL log `[sem-wq] dequeue` with sem_id, task id, task name, and queue_count.
3. WHEN a WAITING task is dequeued THEN the system SHALL transition that task from WAITING to READY.
4. WHEN the task becomes READY THEN the system SHALL clear its `wait_sem_id` to the non-waiting state.
5. WHEN a WAITING task is returned to READY THEN the system SHALL NOT increment the semaphore count.
6. WHEN wakeup completes THEN the system SHALL log action `wakeup-no-switch`.

### Requirement 4: wait queue空時のcount-up

**User Story:** RTOS学習者として、待ちtaskがいない場合だけ `sig_sem()` がsemaphore countを増やすことを確認したい。そうすることで、待ちtaskへの資源引き渡しとcount蓄積の違いをログで区別できる。

#### Acceptance Criteria

1. GIVEN target semaphore wait queue is empty WHEN `sig_sem()` is called THEN the system SHALL log `[sem-wq] empty` with sem_id.
2. GIVEN no waiting task exists WHEN semaphore count can be increased THEN the system SHALL increment the count by 1.
3. WHEN the count is incremented THEN the system SHALL log the count before and after.
4. WHEN the count is incremented THEN the system SHALL complete with action `count-up`.

### Requirement 5: 既存経路の維持

**User Story:** RTOS学習者として、12.3のwait queue導入が10.4、11.4、12.1、12.2の観測経路を壊していないことを確認したい。そうすることで、段階的なRTOS構築の各章の到達点を安定して積み上げられる。

#### Acceptance Criteria

1. WHEN normal build is executed THEN the system SHALL build successfully.
2. WHEN normal `make run` is executed THEN the system SHALL complete the boot-time smoke log.
3. WHEN `make run VALIDATE_TIMER_IRQ_ENTRY=1` is executed THEN the system SHALL preserve dispatch pending request, consume, and clear log ordering.
4. WHILE timer IRQ handler body runs THEN the system SHALL NOT call `wai_sem()`, `sig_sem()`, `yield_tsk()`, or `dispatcher_switch_to()` directly.
5. WHEN READY task priority is evaluated THEN the system SHALL continue to treat smaller priority values as higher priority.

### Requirement 6: 文書と成果物

**User Story:** RTOS学習者として、第12章12.3の到達点と未実装範囲をREADME、spec、Doxygenコメント、QEMU serial logから確認したい。そうすることで、次の12.4以降で扱う責務を混同せずに学習を継続できる。

#### Acceptance Criteria

1. WHEN implementation is complete THEN README SHALL describe Chapter 12 Section 12.3 and remaining out-of-scope items.
2. WHEN implementation is complete THEN source comments SHALL describe that priority wait queue, wakeup-after-preemption, timeout, same-priority time slice, and round-robin are not implemented yet.
3. WHEN implementation is complete THEN `docs/logs/qemu-serial.log` SHALL include the updated smoke output.
4. WHEN implementation is complete THEN `.kiro/specs/semaphore-wait-queue/` SHALL contain only `requirements.md`, `design.md`, and `tasks.md`.
