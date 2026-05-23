# Requirements Document

## Introduction

この仕様は、μITRON風RTOSの第9章9.3「RUNNING/READY状態遷移を実切替に接続」を扱う。対象ユーザーは、RTOSのdispatcher、scheduler、task contextの責務分離を段階的に学習・実装する開発者である。

第9章9.1ではtask_bからtask_cへのtask-to-task context switch smokeを実装し、第9章9.2では上位の切替開始点を`dispatcher_switch_to(from, to)`境界へ移した。現在の実装では、この境界が入力検証とtask_context補助APIへの委譲を担う一方で、RUNNING/READY状態遷移はまだ実切替境界の観測点として整理されていない。今回の変更では、`dispatcher_switch_to()`内でfrom taskをRUNNINGからREADYへ戻し、to taskをREADYからRUNNINGへ進める状態遷移を明示し、QEMU serial logで観測できるようにする。

## Boundary Context

- **In scope**: `dispatcher_switch_to(from, to)`内のRUNNING/READY事前確認、from taskのRUNNING->READY遷移、to taskのREADY->RUNNING遷移、current task更新、状態遷移ログ、9.1 task-to-task smoke維持、9.2 dispatcher switch boundaryログ維持、README/Doxygen/spec/log更新。
- **Out of scope**: entry return時の正式なDORMANT/READY確定、task終了状態の最終設計、interrupt exit boundaryからの実dispatch、dispatch pending消費、timer IRQからの実切替、preemptive context switch、yield_tsk API、semaphore wakeup連携、sleep/delay queue、同一優先度タイムスライス、nested interrupt、iretq通常割り込み復帰モデル完成、APIC/IOAPIC/LAPIC、SMP、μITRON API完成。
- **Adjacent expectations**: schedulerはREADY taskを選ぶだけに留める。dispatcherはcurrent commit、switch boundary、RUNNING/READY遷移を担当する。task_context層はstack/register contextに近い処理を担当し、dispatcher責務を持たない。arch/x86_64側へscheduler/dispatcher内部を漏らさず、kernel commonへPIC、vector番号、I/O port、entry stub詳細を漏らさない。

## Requirements

### Requirement 1: dispatcher境界での状態遷移観測

**Objective:** As a RTOS学習者, I want `dispatcher_switch_to()`境界でRUNNING/READY状態遷移を観測できる, so that 実切替と論理状態更新の接続点を理解できる

#### Acceptance Criteria

1. When `dispatcher_switch_to(from, to)` is invoked with a valid switching pair, the RTOS shall verify that `from` is RUNNING and `to` is READY before entering the real switch path.
2. When the verified switch boundary advances, the RTOS shall log that `from` transitions from RUNNING to READY.
3. When the verified switch boundary advances, the RTOS shall log that `to` transitions from READY to RUNNING.
4. When the state transition logs are emitted, the RTOS shall include task id and task name for both transition targets.
5. If `from` is not RUNNING or `to` is not READY, then the RTOS shall reject the switch request before changing either task state.

### Requirement 2: dispatcher責務としてのcurrent task整理

**Objective:** As a maintainer, I want current task handling to remain owned by dispatcher, so that scheduler and task_context boundaries do not silently absorb dispatcher responsibility

#### Acceptance Criteria

1. When `dispatcher_switch_to()` changes `to` into RUNNING, the RTOS shall update the dispatcher current task to the destination task.
2. When the state transition fails, the RTOS shall not update the dispatcher current task to an uncommitted destination.
3. The scheduler shall continue to select READY tasks without committing current task state or invoking context switching.
4. The task_context layer shall not commit dispatcher current task ownership.

### Requirement 3: 既存smokeと非対象境界の維持

**Objective:** As a maintainer, I want the 9.1 and 9.2 smoke behavior to keep working while 9.3 adds state transitions, so that the incremental RTOS construction remains verifiable

#### Acceptance Criteria

1. When `make run` is executed, the RTOS shall still execute the task_b to task_c task-to-task context switch smoke.
2. When the smoke runs, the RTOS shall still emit the dispatcher switch boundary begin and end logs added in 9.2.
3. The RTOS shall keep `task_context_switch_to_task_pair()` as a boot-time smoke helper API.
4. The RTOS shall not call `dispatcher_switch_to()` from the timer IRQ handler or interrupt exit boundary in this feature.
5. The RTOS shall not consume dispatch pending state in this feature.

### Requirement 4: 9.3の到達点を文書と検証証跡に残す

**Objective:** As a maintainer, I want documentation and serial log evidence to describe the exact 9.3 boundary, so that later chapters can build on a clear baseline

#### Acceptance Criteria

1. When source documentation is reviewed, public or boundary-facing dispatcher/task-context comments shall describe that 9.3 connects RUNNING/READY transitions to the dispatcher switch boundary.
2. When source documentation is reviewed, comments shall state that entry return final handling, dispatch pending consumption, interrupt exit connection, and timer IRQ real switching remain deferred.
3. README shall include a Chapter 9 Section 9.3 summary and the tag candidate `v9.3-dispatcher-state-transition-switch`.
4. `docs/logs/qemu-serial.log` shall be updated with serial evidence that shows dispatcher state transition logs inside the task-to-task smoke flow.
5. `.kiro/specs/dispatcher-state-transition-switch/` shall contain only `requirements.md`, `design.md`, and `tasks.md` after implementation cleanup.

### Requirement 5: buildとtimer IRQ validationの維持

**Objective:** As a maintainer, I want normal and timer-validation runs to keep passing, so that the new dispatcher state transition does not accidentally become preemption or interrupt dispatch

#### Acceptance Criteria

1. When `make` is executed, the RTOS shall build successfully.
2. When `make run` is executed, the RTOS shall boot through the normal smoke flow and show the 9.3 dispatcher state transition logs.
3. When `make run VALIDATE_TIMER_IRQ_ENTRY=1` is executed, the RTOS shall preserve the timer IRQ validation path without connecting it to real dispatch.
4. When boundary validation is performed, the RTOS shall show no new scheduler/dispatcher dependency leakage into `arch/x86_64`.
5. When boundary validation is performed, the RTOS shall show no dispatch pending consumption from `dispatcher_switch_to()`.
