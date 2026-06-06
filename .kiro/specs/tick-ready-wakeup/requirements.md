# Requirements Document

## Project Description

学習用 μITRON 風 RTOS を段階的に開発している開発者が、13.3 までに `dly_tsk()` と `twai_sem()` を delay queue へ登録できるようにした。現在は remaining tick が観測値に留まり、timer tick 到達時に WAITING task が READY へ戻らない。13.4 では timer tick ごとに delay queue を進め、delay 待ちおよび timeout 付き semaphore 待ち task を READY へ復帰できるようにする。

## Boundary Context

- **In scope**: delay queue tick 処理、remaining tick 減算、timeout 到達 entry の削除、delay 待ち task の READY 復帰、timeout 付き semaphore 待ち task の READY 復帰、timeout 時の semaphore wait queue からの削除、READY 復帰後の preemption pending 連携、timer tick からの呼び出し、README/Doxygen/log/spec 更新。
- **Out of scope**: `sig_sem()` 成功時の delay queue 削除、timeout error code の本格整理、task への timeout 戻り値伝達、`pol_sem`、`slp_tsk` / `wup_tsk`、priority 順 wait queue、delta queue 最適化、time slice、round-robin、timer IRQ handler からの task API 呼び出し、timer IRQ handler からの直接 `dispatcher_switch_to()` 呼び出し、既存 RTOS 実装の参照・コピー・流用。

## Requirements

### Requirement 1: delay queue tick による remaining tick 減算

**Objective:** timer tick が進むたびに delay queue 上の各 task の remaining tick を 1 ずつ減算し、減算前後を観測できるようにする。

#### Acceptance Criteria

1. When `timer_tick()` runs, the system shall invoke delay queue tick processing once after incrementing the system tick count.
2. When delay queue tick processing begins, the system shall log the current delay queue count.
3. When a delay queue entry is processed and its remaining tick is greater than zero, the system shall decrement it by one.
4. When a delay queue entry is processed, the system shall log task id, task name, remaining tick before and after, wait reason, and task state.
5. When remaining tick is still greater than zero after decrement, the system shall keep the task in the delay queue.

### Requirement 2: delay 待ち task の READY 復帰

**Objective:** `dly_tsk()` により delay queue へ登録された task を、remaining tick が 0 になった時点で WAITING から READY へ復帰させる。

#### Acceptance Criteria

1. When a delay wait entry reaches remaining tick zero, the system shall remove the entry from the delay queue.
2. When a delay wait entry reaches timeout, the system shall move the task from WAITING to READY.
3. When the task is moved to READY, the system shall clear wait reason, wait semaphore id, and remaining tick observation fields.
4. When delay wakeup occurs, the system shall log timeout reached, WAITING to READY transition, and delay queue removal.
5. When the task becomes READY, the system shall make it visible to the scheduler as a READY candidate.

### Requirement 3: timeout 付き semaphore 待ち task の READY 復帰

**Objective:** `twai_sem()` により semaphore wait queue と delay queue の両方へ登録された task を、timeout 到達時に READY へ復帰させる。

#### Acceptance Criteria

1. When a timeout semaphore wait entry reaches remaining tick zero, the system shall remove the task from the target semaphore wait queue.
2. When semaphore wait queue removal succeeds, the system shall log semaphore id, task id, task name, timeout reason, and queue count.
3. When timeout semaphore wait reaches timeout, the system shall move the task from WAITING to READY.
4. When the task is moved to READY, the system shall clear wait reason, wait semaphore id, and remaining tick observation fields.
5. When timeout semaphore wakeup occurs, the system shall log timeout reached, semaphore wait queue removal, WAITING to READY transition, and delay queue removal.

### Requirement 4: preemption pending 連携

**Objective:** timer tick によって READY 復帰した task が current より高優先度の場合、timer IRQ handler 本体で直接 context switch せず dispatch pending へつなげる。

#### Acceptance Criteria

1. When one or more tasks become READY during delay queue tick processing, the system shall evaluate preemption after the wakeup.
2. When a woken READY task has higher priority than current, the system shall set dispatch pending through the existing IRQ preemption path.
3. When preemption is evaluated, the system shall log current task, ready candidate task, and the pending reason.
4. While timer IRQ handler body runs, the system shall not call `dispatcher_switch_to()` directly.
5. While timer IRQ handler body runs, the system shall not call `twai_sem()`, `dly_tsk()`, `wai_sem()`, `sig_sem()`, or `yield_tsk()`.

### Requirement 5: 既存経路と成果物の維持

**Objective:** 13.4 の追加で既存の semaphore、delay、yield、timer IRQ validation 経路を壊さず、成果物に到達点を残す。

#### Acceptance Criteria

1. When normal build is executed, the system shall build successfully.
2. When normal `make run` is executed, the system shall show delay timeout wakeup and semaphore timeout wakeup evidence.
3. When `make run VALIDATE_TIMER_IRQ_ENTRY=1` is executed, the system shall preserve timer IRQ preemption and dispatch pending observation behavior.
4. When `wai_sem()` / `sig_sem()` / `dly_tsk()` / `twai_sem()` existing smoke paths run, the system shall preserve their existing queueing and wakeup behavior except for the newly implemented timeout wakeup.
5. When implementation is complete, README, Doxygen comments, serial log, and this spec shall describe the 13.4 behavior and current non-goals.
