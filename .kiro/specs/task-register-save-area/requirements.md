# Requirements Document

## Introduction
この仕様は、μITRON風RTOSの第5章5.2として、taskごとのCPU register保存領域をTCB上で明確に扱えるようにする。対象は「register save/restore」ではなく「register save area」であり、将来の最小context switchへ進むための観測可能な準備段階である。第5章5.1で導入したtaskごとの `stack_base`、`stack_size`、`stack_top` を前提に、各taskが独立したcontext保存領域を持つことをQEMUシリアルログで確認できるようにする。

## Boundary Context
- **In scope**: x86_64向けregister保存領域の定義、taskごとのTCB内context保持、task登録時のcontext初期化、`context.rsp` と `stack_top` の対応、登録ログとdumpログでのcontext表示、既存cooperative runnerのログ順序維持。
- **Out of scope**: CPU registerの実保存、CPU registerの実復元、context switch、stack switch、assembler実装、task stack上でのentry実行、interrupt、timer、preemption、yield API追加、task終了状態追加、`TASK_STATE_EXITED` 追加、DORMANT遷移、scheduler公平性改善、round-robin、ready queue、μITRON互換API追加。
- **Adjacent expectations**: `task-stack-foundation` が提供する `stack_top` は `context.rsp` の初期候補として利用される。schedulerはREADY task選択、dispatcherはcurrent commit、kernel cooperative runnerはboot-time verification modelとしてのentry直接呼び出しを継続する。

## Requirements

### Requirement 1: task register save area metadata
**Objective:** As a RTOS learner, I want each task to have an explicit CPU register save area in its TCB, so that future context-switch work has a visible per-task state container.

#### Acceptance Criteria
1. When a task is registered successfully, the task module shall associate that task with a register save area containing `rsp`, `rbp`, `rbx`, `r12`, `r13`, `r14`, and `r15`.
2. When multiple tasks are registered successfully, the task module shall keep each task's register save area independent from the other registered tasks.
3. The system shall treat the register save area as task metadata only during this feature.

### Requirement 2: register save area initialization
**Objective:** As a RTOS learner, I want each task's register save area to be initialized deterministically, so that serial logs can be compared with stack metadata.

#### Acceptance Criteria
1. When a task is registered successfully, the task module shall initialize `context.rsp` to the task's `stack_top` value.
2. When a task is registered successfully, the task module shall initialize `context.rbp`, `context.rbx`, `context.r12`, `context.r13`, `context.r14`, and `context.r15` to zero.
3. If task registration is rejected because stack metadata is invalid, then the task module shall not expose a partially initialized register save area for that rejected task.
4. While this feature is active, the system shall not load `context.rsp` into the CPU RSP register.

### Requirement 3: register save area observability
**Objective:** As a RTOS learner, I want register save area values visible in serial logs, so that I can confirm each task has context metadata before real register switching exists.

#### Acceptance Criteria
1. When task registration succeeds, the system shall print a registration log containing task id, name, state, priority, entry, `stack_base`, `stack_size`, `stack_top`, and all register save area fields.
2. When task dump is requested, the system shall print each registered task with task id, name, priority, state, entry, `stack_base`, `stack_size`, `stack_top`, and all register save area fields.
3. When QEMU serial output is inspected after boot, the system shall show `context.rsp`, `context.rbp`, `context.rbx`, `context.r12`, `context.r13`, `context.r14`, and `context.r15` for `task_a`, `task_b`, and `task_c`.
4. When context metadata is printed, the system shall make it possible to verify that each task's `context.rsp` corresponds to that task's `stack_top`.

### Requirement 4: cooperative runner compatibility
**Objective:** As a RTOS learner, I want the Chapter 4 cooperative runner behavior to remain unchanged, so that register save area preparation does not change execution semantics.

#### Acceptance Criteria
1. While cooperative runner verification is running, the system shall continue selecting READY tasks, committing current, calling entry directly as a normal C function, observing entry return, and returning RUNNING tasks to READY candidates.
2. While cooperative runner verification is running, the system shall not perform stack switching, CPU register save/restore, assembler dispatch, interrupt dispatch, timer dispatch, or preemption.
3. When task entry functions run, the system shall keep the existing direct-call log order around scheduler selection, dispatcher commit, entry call, entry return, cooperative return, and READY recandidacy.

### Requirement 5: educational traceability
**Objective:** As a maintainer of a learning RTOS, I want the code and comments to state the limits of this phase, so that future context-switch work does not mistake prepared metadata for active CPU state.

#### Acceptance Criteria
1. The system shall document that the register save area is intended for a future minimal context switch.
2. The system shall document that this feature does not save live CPU register values into the register save area.
3. The system shall document that this feature does not restore CPU register values from the register save area.
4. The system shall document that `context.rsp` is a future restore candidate derived from `stack_top`, not a value currently loaded into CPU RSP.
