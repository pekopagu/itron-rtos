# Requirements Document

## Introduction

第10章10.2では、10.1で追加した μITRON 風 `yield_tsk()` API を一段進め、RUNNING 状態の current task を READY へ戻せることを観測可能にする。対象は学習用 RTOS の開発者であり、現状は `yield_tsk()` が current task をログ出力するだけで状態遷移を行わない。今回の変更では、9.4で確定した entry return -> DORMANT の扱いと、10.1で明確化した「`yield_tsk()` は entry return ではない」という設計を維持したまま、current が RUNNING の場合だけ READY へ戻す。

## Boundary Context

- **In scope**: RUNNING current task の READY 化、タスク管理層の RUNNING->READY API 利用、`yield_tsk()` ログ更新、DORMANT current の reject 維持、README/Doxygen/docs log 更新。
- **Out of scope**: 次 task 選択、`scheduler_select_next()` 呼び出し、`dispatcher_switch_to()` 接続、`task_context_switch_to_task_pair()` 接続、実 context switch、dispatch pending 消費、interrupt exit/timer IRQ からの dispatch 接続、preemption、time slice、他 μITRON 風 API 実装。
- **Adjacent expectations**: 9.1 task_b -> task_c smoke、9.2 dispatcher switch boundary、9.3 RUNNING/READY 遷移、9.4 entry return -> DORMANT、10.1 の `yield_tsk()` invalid-current-state ログは維持する。

## Requirements

### Requirement 1: RUNNING current task の READY 化

**Objective:** 学習用 RTOS 開発者として、`yield_tsk()` が実行中 current task を READY 候補へ戻す最小状態遷移を観測したい。

#### Acceptance Criteria

1. When `yield_tsk()` が current task 存在中かつ `TASK_STATE_RUNNING` の状態で呼ばれたとき, the kernel shall current task の id、name、state を `[yield] called` ログへ出力する。
2. When `yield_tsk()` が RUNNING current task に対して呼ばれたとき, the kernel shall task 管理層の RUNNING->READY 遷移 API を通じて対象 task を READY へ戻す。
3. When RUNNING->READY 遷移が成功したとき, the kernel shall `[yield] state transition: current ... RUNNING->READY` をログへ出力する。
4. When RUNNING->READY 遷移が成功したとき, the kernel shall `YIELD_TSK_OK` を返す。

### Requirement 2: 不正 current 状態の reject 維持

**Objective:** 学習用 RTOS 開発者として、`yield_tsk()` が entry return や DORMANT task を READY へ戻さないことを確認したい。

#### Acceptance Criteria

1. If current task が存在しない場合, the kernel shall 従来どおり invalid-current-state として reject し、負の戻り値を返す。
2. If current task が RUNNING ではない場合, the kernel shall current task の id、name、state をログへ出力したうえで invalid-current-state として reject する。
3. If current task が DORMANT の場合, the kernel shall task を READY へ戻さない。
4. The kernel shall `yield_tsk()` を entry return の代替として扱わない。

### Requirement 3: 10.2 の非接続境界

**Objective:** 学習用 RTOS 開発者として、10.2 が協調スケジューリング完成回ではなく READY 化の到達点で止まることを確認したい。

#### Acceptance Criteria

1. When `yield_tsk()` が RUNNING current task を READY へ戻したとき, the kernel shall `[yield] deferred: reason=scheduler-not-connected-yet` をログへ出力する。
2. The kernel shall `yield_tsk()` 内で `scheduler_select_next()` を呼ばない。
3. The kernel shall `yield_tsk()` 内で `dispatcher_switch_to()` を呼ばない。
4. The kernel shall `yield_tsk()` 内で `task_context_switch_to_task_pair()` を呼ばない。
5. The kernel shall timer IRQ handler から `dispatcher_switch_to()` を呼ばず、dispatch pending も消費しない。

### Requirement 4: 既存 smoke と文書の維持

**Objective:** 学習用 RTOS 開発者として、10.2 の追加後も 9.x と 10.1 の観測点が壊れていないことを確認したい。

#### Acceptance Criteria

1. When 通常 smoke を実行したとき, the kernel shall 9.1 の task_b -> task_c context switch smoke ログを維持する。
2. When 通常 smoke を実行したとき, the kernel shall 9.2 の dispatcher switch boundary ログを維持する。
3. When 通常 smoke を実行したとき, the kernel shall 9.3 の RUNNING/READY 状態遷移ログを維持する。
4. When 通常 smoke を実行したとき, the kernel shall 9.4 の entry return -> DORMANT ログを維持する。
5. The repository shall README、Doxygen コメント、`docs/logs/qemu-serial.log` に 10.2 の到達点と未実装範囲を記載する。
6. The spec directory shall `.kiro/specs/yield-running-to-ready/` に `requirements.md`、`design.md`、`tasks.md` の3ファイルだけを保持する。
