# Requirements Document

## Introduction
第10章10.4では、10.3で `yield_tsk()` が選べるようになった次READY task候補を、協調API経由で既存の `dispatcher_switch_to()` 境界へ接続する。実装は学習用RTOSの段階的検証として、RUNNING current taskをREADYへ戻し、次READY taskを選び、dispatcher境界を通してtask_context層のtask-to-task switch smokeへ進めることをQEMU serial logで観測可能にする。

## Boundary Context
- **In scope**: `yield_tsk()` からのRUNNING current task READY化、次READY task候補選択、`dispatcher_switch_to()` 接続、task_context層のtask-to-task switch観測、DORMANT current reject、README/spec/Doxygen/log更新。
- **Out of scope**: timer IRQからのyield呼び出し、interrupt exit boundaryからの実dispatch接続、dispatch pending消費、preemptive context switch、time slice、semaphore wakeup連携、sleep/delay queue、他のmuITRON風API完成、nested interrupt、APIC/IOAPIC/LAPIC、SMP。
- **Adjacent expectations**: 9.1 task_b -> task_c smoke、9.2 dispatcher境界ログ、9.3 RUNNING/READY状態遷移、9.4 entry return -> DORMANT確定、10.2 RUNNING->READYログ、10.3 next selectedログを維持する。

## Requirements

### Requirement 1: yield_tsk cooperative switch connection
**Objective:** As a RTOS learner, I want `yield_tsk()` to reach the existing dispatcher/context switch boundary, so that cooperative API based switching can be observed before interrupt-driven dispatch is introduced.

#### Acceptance Criteria
1. When `yield_tsk()` is called while the dispatcher current task is RUNNING and a different READY task exists, the kernel shall log the current task, return the current task to READY, log the next selected READY task, log switch begin, call the dispatcher switch boundary, and log switch end with the dispatcher result.
2. When `yield_tsk()` selects a next READY task, the kernel shall preserve the existing scheduler responsibility as READY task selection only and shall not make the scheduler perform RUNNING/READY transitions.
3. When `yield_tsk()` reaches the switch boundary, the kernel shall perform the actual RUNNING/READY state transition through the dispatcher boundary and shall then reach the task_context task-to-task switch path.
4. The kernel shall keep x86_64 context switch details outside the `yield_tsk()` API layer.

### Requirement 2: no-next-task and invalid-current handling
**Objective:** As a RTOS learner, I want explicit rejection and deferral logs, so that unsupported yield states are distinguishable from successful cooperative switch attempts.

#### Acceptance Criteria
1. If `yield_tsk()` is called without a dispatcher current task, then the kernel shall reject the call as `invalid-current-state` and shall not change task states.
2. If `yield_tsk()` is called while the dispatcher current task is not RUNNING, then the kernel shall log the current task state, reject the call as `invalid-current-state`, and shall not return DORMANT, READY, or WAITING tasks to READY.
3. If `yield_tsk()` returns the RUNNING current task to READY but no next READY task exists, then the kernel shall log `no-ready-task` and `deferred: reason=no-next-task` without calling the dispatcher switch boundary.
4. While entry return finalization has made a task DORMANT, the kernel shall preserve the 9.4 DORMANT finalization and shall not treat `yield_tsk()` as entry return or task restart.

### Requirement 3: scope isolation from interrupt and preemption paths
**Objective:** As a RTOS learner, I want the cooperative switch path isolated from interrupt-time dispatch, so that 10.4 does not accidentally implement preemption.

#### Acceptance Criteria
1. The kernel shall not call `yield_tsk()` from the timer IRQ handler.
2. The kernel shall not call `dispatcher_switch_to()` from the timer IRQ handler or interrupt exit boundary.
3. The kernel shall not consume dispatch pending as part of this feature.
4. When `make run VALIDATE_TIMER_IRQ_ENTRY=1` is executed, the timer IRQ validation path shall continue to produce its existing observation logs without performing a dispatcher/context switch.

### Requirement 4: documentation and validation evidence
**Objective:** As a maintainer, I want the 10.4 milestone documented and validated, so that the repository state communicates exactly what was added and what remains deferred.

#### Acceptance Criteria
1. The README shall describe that 10.4 connects `yield_tsk()` to dispatcher/context switch and shall state that timer IRQ, dispatch pending, and preemptive switch remain unconnected.
2. The source Doxygen comments shall describe the 10.4 responsibility boundary in Japanese.
3. The QEMU serial log artifact shall include evidence that cooperative `yield_tsk()` reaches `dispatcher_switch_to()` and the task_context task-to-task switch path.
4. The specification directory for this feature shall contain only `requirements.md`, `design.md`, and `tasks.md` after the workflow is complete.
