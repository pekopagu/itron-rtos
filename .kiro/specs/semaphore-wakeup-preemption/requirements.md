# Requirements Document

## Project Description

μITRON風RTOSを学習目的で段階的に開発している開発者は、第12章12.3で `sig_sem()` がFIFO wait queueからWAITING taskをREADYへ戻せる状態になった。第12章12.4では、そのREADY復帰直後にcurrent RUNNING taskとのpriority比較を行い、woken taskが高優先度の場合だけ既存dispatcher経路へ進むことを観測できるようにする。

## Boundary Context

- **In scope**: `sig_sem()` のwakeup後preemption判定、priority値が小さいtaskを高優先度とする比較、高優先度wakeup時の `dispatcher_switch_to()` 接続、同一/低優先度wakeup時のno-switchログ、12.3 FIFO wait queue/count制御/`wait_sem_id` clear維持、README/Doxygen/log/spec更新。
- **Out of scope**: priority順wait queue、timeout付き `twai_sem`、sleep/delay queue、nested interrupt、同一優先度time slice、round-robin、完全な割り込み復帰フレーム切替、timer IRQ handlerからの `sig_sem()` 呼び出し。
- **Adjacent expectations**: 10.4 `yield_tsk()` 協調context switch、11.4 timer IRQ preemption/dispatch pendingログ安定化、12.3 FIFO dequeue順を壊さない。`sig_sem()` はtask文脈APIとして扱い、IRQ中のdispatch pending責務と混ぜない。

## Requirements

### Requirement 1: wakeup後preemption判定

**Objective:** RTOS学習者として、`sig_sem()` がWAITING taskをREADYへ戻した直後にpreemption要否を判定することを確認したい。これにより、semaphore wakeupが既存のpriority規則に沿って切替へ進む境界を理解できる。

#### Acceptance Criteria

1. When `sig_sem()` dequeues a WAITING task and returns it to READY, the system shall compare the current RUNNING task priority with the woken READY task priority after `wait_sem_id` is cleared.
2. When the woken task priority value is smaller than the current task priority value, the system shall treat the woken task as higher priority.
3. When the woken task is higher priority than current, the system shall log the preemption check and required reason `wakeup-higher-priority`.
4. When the woken task has the same priority as current, the system shall log no-switch reason `same-or-lower-priority`.
5. When the woken task has lower priority than current, the system shall log no-switch reason `same-or-lower-priority`.

### Requirement 2: 高優先度wakeup時のswitch接続

**Objective:** RTOS学習者として、高優先度taskがsemaphore wakeupされた場合に既存dispatcher境界へ進むことを確認したい。これにより、task文脈APIからのpreemption相当切替をtimer IRQ経路と分離して理解できる。

#### Acceptance Criteria

1. When a higher-priority woken task is selected for switching, the system shall call the existing dispatcher switch boundary with current as `from` and woken READY task as `to`.
2. When the switch begins, the system shall log `[sig-sem] switch begin` with from/to task id and name.
3. When the dispatcher switch returns, the system shall log `[sig-sem] switch end: result=<result>`.
4. When the higher-priority wakeup switch path completes successfully, the system shall complete `sig_sem()` with action `wakeup-switch`.
5. While this task-context wakeup switch runs, the system shall not use the timer IRQ dispatch pending request/consume path.

### Requirement 3: 12.3 semaphore wake queue契約の維持

**Objective:** RTOS学習者として、wakeup後preemption判定を追加しても12.3のFIFO wait queueとsemaphore count制御が壊れていないことを確認したい。

#### Acceptance Criteria

1. When `sig_sem()` finds a waiting task, the system shall dequeue from the target semaphore FIFO wait queue.
2. When a WAITING task is returned to READY, the system shall clear `wait_sem_id` to the non-waiting state.
3. When a WAITING task is returned to READY, the system shall not increment semaphore count.
4. When the target semaphore wait queue is empty, the system shall increment semaphore count by 1 only if count is below `max_count`.
5. When `wai_sem()` enqueues multiple tasks in future scenarios, this feature shall not change FIFO dequeue ordering.

### Requirement 4: 既存経路の維持

**Objective:** RTOS学習者として、12.4の追加が10.4と11.4の既存観測経路を壊していないことを確認したい。

#### Acceptance Criteria

1. When normal build is executed, the system shall build successfully.
2. When normal `make run` is executed, the system shall complete the boot-time smoke log.
3. When `make run VALIDATE_TIMER_IRQ_ENTRY=1` is executed, the system shall preserve timer IRQ dispatch pending request, consume, clear, and no-dispatch log behavior.
4. While timer IRQ handler body runs, the system shall not call `sig_sem()`, `wai_sem()`, `yield_tsk()`, or `dispatcher_switch_to()` directly.
5. When `yield_tsk()` smoke runs, the system shall preserve the existing cooperative context switch path.

### Requirement 5: 文書と成果物

**Objective:** RTOS学習者として、第12章12.4の到達点と未実装範囲をREADME、Doxygenコメント、serial log、spec成果物から確認したい。

#### Acceptance Criteria

1. When implementation is complete, README shall describe Chapter 12 Section 12.4 and remaining out-of-scope items.
2. When implementation is complete, meaningful changed interfaces or helpers shall have Japanese Doxygen comments describing purpose, assumptions, limitations, and non-goals.
3. When validation is complete, `docs/logs/qemu-serial.log` shall contain fresh `make run` output including wakeup preemption evidence.
4. When implementation is complete, `.kiro/specs/semaphore-wakeup-preemption/` shall contain only `requirements.md`, `design.md`, and `tasks.md`.
