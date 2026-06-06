# Requirements Document

## Introduction

学習用 μITRON 風 RTOS の開発者は、これまで静的な `task_register()` によってタスクを登録し、登録直後の READY 状態を scheduler / dispatcher / yield / semaphore / delay queue の検証に使ってきた。第14章14.1では、既存の静的登録経路を壊さず、μITRON 風 API 層として「生成」と「起動」を分離した `cre_tsk()` / `sta_tsk()` を追加する。

`cre_tsk()` は task 定義を TCB へ登録するが、直後の状態を DORMANT とし scheduler READY 候補には入れない。`sta_tsk()` は DORMANT task だけを READY へ遷移させ、READY 化後に既存の preemption pending 判定へ接続する。

## Boundary Context

- **In scope**: `cre_tsk()` / `sta_tsk()` の宣言と実装、task 生成属性構造体、DORMANT 登録、DORMANT から READY への起動、READY 候補化、既存 preemption pending 判定への接続、14.1 の README / Doxygen / serial log / spec 更新。
- **Out of scope**: `del_tsk`, `ter_tsk`, `ext_tsk`, `exd_tsk`, `slp_tsk`, `wup_tsk`, `sus_tsk`, `rsm_tsk`, `act_tsk`, `stacd` 本格対応、task ID 自動採番の高度化、`tskatr` 本格対応、stack 動的確保、priority 変更、エラーコード体系の本格整理、同一優先度 time slice、round-robin、timer IRQ handler からの task API 呼び出し、既存 RTOS 実装の参照・コピー・流用。
- **Adjacent expectations**: 既存の `task_register()`、scheduler、dispatcher、`yield_tsk()`、`wai_sem()` / `sig_sem()`、`dly_tsk()` / `twai_sem()`、delay queue tick wakeup、timer IRQ deferred dispatch の挙動を維持する。

## Requirements

### Requirement 1: `cre_tsk()` API と生成属性
**Objective:** 学習用 RTOS 開発者として、μITRON 風 task 生成 API を呼び出し、entry / priority / stack / name を TCB に登録したい。

#### Acceptance Criteria
1. When `cre_tsk()` is declared, the system shall expose it through the μITRON-like API header.
2. When task create parameters are declared, the system shall include entry function, priority, stack base, stack size, and name.
3. When `cre_tsk()` is called with valid parameters, the system shall register the task definition into an unused TCB.
4. When `cre_tsk()` completes successfully, the system shall return a valid task ID or accept the requested task ID according to the learning API contract.
5. When `cre_tsk()` is called, the system shall log the requested task ID, task name, priority, and result.

### Requirement 2: `cre_tsk()` 後の DORMANT 状態
**Objective:** 生成と起動を分離するため、`cre_tsk()` 直後の task を scheduler READY 候補に入れないようにしたい。

#### Acceptance Criteria
1. When `cre_tsk()` creates a task, the system shall set the initial task state to DORMANT.
2. When `cre_tsk()` has completed and `sta_tsk()` has not been called for that task, the system shall not include the task in scheduler READY candidates.
3. When task dump or logs show the created task, the system shall show `state=DORMANT`.
4. When `cre_tsk()` fails validation or capacity checks, the system shall not create a partial READY or DORMANT task.

### Requirement 3: `sta_tsk()` DORMANT to READY 起動
**Objective:** 生成済み DORMANT task を明示的に起動し、scheduler が選択できる READY task にしたい。

#### Acceptance Criteria
1. When `sta_tsk()` is declared, the system shall expose it through the μITRON-like API header.
2. When `sta_tsk()` is called for a DORMANT task, the system shall move that task from DORMANT to READY.
3. When `sta_tsk()` succeeds, the system shall make the started task visible to the scheduler as a READY candidate.
4. When `sta_tsk()` logs the transition, the system shall show DORMANT to READY, task ID, task name, priority, and state.

### Requirement 4: `sta_tsk()` の不正状態保護
**Objective:** DORMANT でない task を起動しようとしたときに、既存状態を壊さず失敗させたい。

#### Acceptance Criteria
1. When `sta_tsk()` targets a READY task, the system shall reject the call without changing task state.
2. When `sta_tsk()` targets a RUNNING task, the system shall reject the call without changing task state.
3. When `sta_tsk()` targets a WAITING task, the system shall reject the call without changing task state or wait metadata.
4. When `sta_tsk()` targets an invalid or unknown task ID, the system shall reject the call without changing any task.
5. When `sta_tsk()` rejects a task, the system shall log the observed state and expected DORMANT state.

### Requirement 5: READY 化後の preemption pending 接続
**Objective:** 高優先度 task を `sta_tsk()` したとき、既存の preemption 判定と dispatch pending 経路へ自然につなげたい。

#### Acceptance Criteria
1. When `sta_tsk()` makes a task READY, the system shall evaluate the current RUNNING task and the highest-priority READY candidate through the existing scheduler/preemption policy.
2. When the started task is higher priority than current, the system shall set dispatch pending through the existing pending request boundary.
3. When the started task is not higher priority than current, the system shall not request dispatch pending only because of `sta_tsk()`.
4. When preemption is evaluated after `sta_tsk()`, the system shall log current task, ready task, priorities, and pending result.
5. While timer IRQ handler body runs, the system shall not call `cre_tsk()`, `sta_tsk()`, or `dispatcher_switch_to()` directly.

### Requirement 6: 既存経路と成果物の維持
**Objective:** 14.1 の追加で既存の task / scheduler / dispatcher / semaphore / delay queue / timer IRQ 検証を退行させない。

#### Acceptance Criteria
1. When normal build is executed, the system shall build successfully.
2. When normal `make run` is executed, the system shall show `cre_tsk()` creation, DORMANT state, `sta_tsk()` READY transition, scheduler candidate evidence, invalid-state rejection, and high-priority preemption pending evidence.
3. When `make run VALIDATE_TIMER_IRQ_ENTRY=1` is executed, the system shall preserve timer IRQ preemption and deferred dispatch behavior.
4. When existing `yield_tsk()`, `wai_sem()` / `sig_sem()`, `dly_tsk()` / `twai_sem()`, and delay queue tick paths run, the system shall preserve their existing observable behavior.
5. When implementation is complete, README, Doxygen comments, serial log, and this spec shall describe the 14.1 behavior and current non-goals.
6. When implementation is complete, `.kiro/specs/create-start-task-api/` shall contain only `requirements.md`, `design.md`, and `tasks.md`.
