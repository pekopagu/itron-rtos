# Requirements Document

## Introduction

学習用 μITRON 風 RTOS を段階的に開発している開発者が、13.4 で timer tick 到達時の delay queue READY 復帰を追加し、14.1 で `cre_tsk()` / `sta_tsk()` による task 生成と起動の分離を追加した。14.2 では、μITRON 風 API 層として `slp_tsk()` / `wup_tsk(tskid)` を追加し、RUNNING task を sleep 待ちの WAITING へ遷移させ、sleep 待ち task だけを READY へ復帰できるようにする。

## Boundary Context

- **In scope**: `slp_tsk()` / `wup_tsk(tskid)` の宣言と実装、sleep 待ち wait reason、RUNNING から WAITING(sleep) への遷移、WAITING(sleep) から READY への復帰、READY 候補化、READY 化後の既存 preemption pending 判定への接続、sleep 待ち以外への `wup_tsk()` 失敗ログ、README / Doxygen / serial log / spec 更新。
- **Out of scope**: `tslp_tsk`、`rel_wai`、`sus_tsk` / `rsm_tsk`、`del_tsk`、`ter_tsk`、`ext_tsk`、`exd_tsk`、wakeup 要求カウントの本格対応、`wup_tsk()` の起床要求蓄積、sleep 待ちの timeout 対応、priority 順 wait queue、同一優先度 time slice、round-robin、timer IRQ handler からの `slp_tsk()` / `wup_tsk()` 呼び出し、timer IRQ handler からの直接 `dispatcher_switch_to()` 呼び出し、既存 RTOS 実装の参照・コピー・流用。
- **Adjacent expectations**: 既存の `cre_tsk()` / `sta_tsk()`、`yield_tsk()`、`wai_sem()` / `sig_sem()`、`dly_tsk()` / `twai_sem()`、delay queue tick READY 復帰、timer IRQ deferred dispatch の観測可能な挙動を維持する。

## Requirements

### Requirement 1: `slp_tsk()` sleep 待ち API

**Objective:** 開発者が現在 RUNNING 中の task を明示的に sleep 待ちへ遷移させ、scheduler READY 候補から外せるようにする。

#### Acceptance Criteria

1. When `slp_tsk()` is declared, the system shall expose it through the μITRON-like API header.
2. When `slp_tsk()` is called by a RUNNING current task, the system shall log the current task id, name, priority, and state.
3. When `slp_tsk()` succeeds, the system shall move the current task from RUNNING to WAITING.
4. When `slp_tsk()` succeeds, the system shall set the task wait reason to sleep.
5. When `slp_tsk()` completes, the system shall log the result and sleep action.

### Requirement 2: sleep 待ち task の scheduler 除外と次 task 選択

**Objective:** sleep 待ちへ入った task を READY 候補から外し、既存 scheduler / dispatcher 経路で次の READY task へ進めるようにする。

#### Acceptance Criteria

1. While a task is WAITING with sleep reason, the system shall not include the task in scheduler READY candidates.
2. When `slp_tsk()` moves the current task to WAITING, the system shall allow the scheduler to select the next READY task.
3. When the scheduler selects the next READY task after sleep, the system shall log the selected task id, name, priority, and state.
4. When dispatch is requested after sleep, the system shall log the previous WAITING task and next READY task.
5. While timer IRQ handler body runs, the system shall not call `slp_tsk()`, `wup_tsk()`, or `dispatcher_switch_to()` directly.

### Requirement 3: `wup_tsk()` による sleep 待ち READY 復帰

**Objective:** 開発者が指定 task ID の sleep 待ち task だけを READY へ戻し、scheduler 候補に入れられるようにする。

#### Acceptance Criteria

1. When `wup_tsk()` is declared, the system shall expose it through the μITRON-like API header.
2. When `wup_tsk(tskid)` is called, the system shall log the requested task id and current task context.
3. When `wup_tsk(tskid)` targets a WAITING task with sleep reason, the system shall move the task from WAITING to READY.
4. When `wup_tsk(tskid)` succeeds, the system shall clear or replace wait metadata so that the task is observable as READY rather than sleep waiting.
5. When `wup_tsk(tskid)` succeeds, the system shall log WAITING(sleep) to READY transition and READY candidate evidence.

### Requirement 4: `wup_tsk()` の不正状態保護

**Objective:** sleep 待ちでない task を `wup_tsk()` で誤って READY 化せず、既存の semaphore / delay / timeout wait を壊さないようにする。

#### Acceptance Criteria

1. When `wup_tsk(tskid)` targets a DORMANT task, the system shall reject the call without changing task state.
2. When `wup_tsk(tskid)` targets a READY task, the system shall reject the call without changing task state.
3. When `wup_tsk(tskid)` targets a RUNNING task, the system shall reject the call without changing task state.
4. When `wup_tsk(tskid)` targets a WAITING task whose reason is semaphore, delay, or timeout semaphore wait, the system shall reject the call without changing task state or wait metadata.
5. When `wup_tsk(tskid)` rejects a task, the system shall log the observed state, observed wait reason, expected WAITING sleep condition, result, and invalid-state action.

### Requirement 5: READY 化後 preemption pending と既存経路維持

**Objective:** `wup_tsk()` で高優先度 task が READY になった場合に既存の preemption pending 方針へ接続し、14.2 以前の動作と成果物を維持する。

#### Acceptance Criteria

1. When `wup_tsk(tskid)` makes a task READY, the system shall evaluate the current RUNNING task and the woken READY task through the existing preemption policy.
2. When the woken task has higher priority than current, the system shall set dispatch pending through the existing pending request boundary.
3. When preemption is evaluated after `wup_tsk(tskid)`, the system shall log current task, ready task, priorities, and pending result.
4. When normal build is executed, the system shall build successfully.
5. When normal `make run` is executed, the system shall show sleep entry, wakeup success, invalid wakeup failure, READY candidate evidence, and high-priority wakeup preemption pending evidence.
6. When `make run VALIDATE_TIMER_IRQ_ENTRY=1` is executed, the system shall preserve timer IRQ preemption and deferred dispatch behavior.
7. When existing `cre_tsk()` / `sta_tsk()` / `yield_tsk()` / `wai_sem()` / `sig_sem()` / `dly_tsk()` / `twai_sem()` paths run, the system shall preserve their existing observable behavior.
8. When implementation is complete, README, Doxygen comments, serial log, and this spec shall describe the 14.2 behavior and current non-goals.
