# Requirements Document

## Introduction
この仕様は、μITRON 風 RTOS の第5章 5.1 として、task ごとの独立 stack 領域を TCB 上で管理し、QEMU シリアルログで観測できるようにする。対象は stack foundation であり、実際の stack switch や context switch ではない。第4章で導入した cooperative runner、entry 直接呼び出し、entry return の観測イベント扱いは維持する。

## Boundary Context
- **In scope**: task ごとの静的 stack 領域、TCB 上の `stack_base` / `stack_size` / `stack_top`、task 登録ログと dump ログでの stack 情報表示、stack 領域の非重複と `stack_top = stack_base + stack_size` の観測。
- **Out of scope**: CPU RSP への `stack_top` ロード、実 stack 切り替え、独立 stack 上での entry 実行、context switch、register save/restore、assembler、interrupt、timer、preemption、task 終了状態追加、scheduler 公平性改善、ready queue、μITRON 互換 API 追加。
- **Adjacent expectations**: scheduler は READY task 選択、dispatcher は selected task の current commit、kernel cooperative runner は boot-time verification model としての entry 直接呼び出しを継続する。

## Requirements

### Requirement 1: task stack metadata registration
**Objective:** As a RTOS learner, I want each registered task to carry explicit stack metadata in its TCB, so that later context-switch work has an observable foundation without changing execution yet.

#### Acceptance Criteria
1. When a task is registered with a valid stack region, the task module shall store `stack_base`, `stack_size`, and `stack_top` in the task's TCB.
2. When a task is registered, the task module shall set `stack_top` to the address corresponding to `stack_base + stack_size` for a downward-growing x86_64 stack.
3. When a task is registered, the task module shall preserve the existing task id, name, entry, priority, and READY state behavior.
4. If a task is registered with a missing stack base or zero stack size, then the task module shall reject the registration without producing a partially valid task.

### Requirement 2: stack metadata observability
**Objective:** As a RTOS learner, I want stack metadata visible in serial logs, so that I can confirm each task has its own stack region before stack switching exists.

#### Acceptance Criteria
1. When task registration succeeds, the system shall print a registration log containing task id, name, state, priority, entry, `stack_base`, `stack_size`, and `stack_top`.
2. When task dump is requested, the system shall print each registered task with task id, name, priority, state, entry, `stack_base`, `stack_size`, and `stack_top`.
3. When QEMU serial output is inspected after boot, the system shall show distinct stack metadata for `task_a`, `task_b`, and `task_c`.
4. When stack metadata is printed, the system shall make it possible to verify that each `stack_top` equals its task's `stack_base + stack_size`.

### Requirement 3: independent static stack regions
**Objective:** As a RTOS learner, I want sample tasks to use separate static stack regions, so that stack ownership can be observed independently of execution.

#### Acceptance Criteria
1. When the kernel registers the sample tasks, the kernel shall provide a separate static stack region for each task.
2. When the QEMU serial log is inspected, the system shall show that the sample task stack ranges do not overlap.
3. While stack foundation is in use, the system shall treat the task stack regions as metadata only and shall not execute task entries on those stacks.

### Requirement 4: cooperative runner compatibility
**Objective:** As a RTOS learner, I want the Chapter 4 cooperative runner behavior to remain unchanged, so that stack metadata can be introduced without changing execution semantics.

#### Acceptance Criteria
1. While cooperative runner verification is running, the system shall continue selecting READY tasks, committing current, calling entry directly as a normal C function, observing entry return, and returning RUNNING tasks to READY candidates.
2. While cooperative runner verification is running, the system shall not perform stack switching, CPU register save/restore, assembler dispatch, interrupt dispatch, timer dispatch, or preemption.
3. When task entry functions run, the system shall keep the existing direct-call log order around scheduler selection, dispatcher commit, entry call, entry return, cooperative return, and READY recandidacy.

### Requirement 5: educational traceability
**Objective:** As a maintainer of a learning RTOS, I want the code and comments to state the limits of this phase, so that future context-switch work does not misinterpret stack metadata as active execution state.

#### Acceptance Criteria
1. The system shall document that `stack_top` is a future initial stack pointer candidate for context switching.
2. The system shall document that this feature does not load `stack_top` into CPU RSP.
3. The system shall document that this feature does not add task termination, DORMANT transition, scheduler fairness changes, round-robin, ready queue, or μITRON-compatible APIs.
