# Requirements Document

## Introduction

第10章10.1では、協調API編の入口として μITRON 風の `yield_tsk()` API を追加する。利用者は学習用RTOSの起動時 smoke で、実行中 task が entry return とは別に「自発的にCPUを譲る要求」を出したことを観測できる必要がある。現状は9.1から9.4で task-to-task context switch smoke、dispatcher switch boundary、RUNNING/READY遷移、entry return後のDORMANT確定が成立しているため、これらを維持したまま、10.1では API 入口とログ観測だけを追加する。

## Boundary Context

- **In scope**: `yield_tsk()` の公開入口、呼び出しログ、current task の有無と状態に応じた戻り値、README/spec/Doxygen/docs log の10.1到達点更新。
- **Out of scope**: RUNNING taskをREADYへ戻す処理、次task選択、dispatcher/context switch接続、dispatch pending消費、interrupt exit/timer IRQからのyield接続、preemptive context switch、他μITRON風API。
- **Adjacent expectations**: 9.1から9.4の既存smokeログと責務境界は維持される。

## Requirements

### Requirement 1: yield_tsk風API入口

**Objective:** 学習用RTOS開発者として、協調API編の最初の公開入口を持ち、後続章で実dispatchへ拡張できる観測点を得たい。

#### Acceptance Criteria

1. When `yield_tsk()` が呼ばれたとき, the kernel shall `yield_tsk` 呼び出しが発生したことをシリアルログで観測可能にする。
2. When current task が存在しRUNNINGであるとき, the kernel shall current task の id、name、state をログへ出力する。
3. When current task が存在しRUNNINGであるとき, the kernel shall 成功または観測成功を示す戻り値 `0` を返す。
4. If current task が存在しない、またはRUNNINGではない場合, the kernel shall 不正状態として扱い、理由をログへ出力し、負値を返す。
5. The kernel shall `yield_tsk()` を entry return とは別の自発的yield要求として扱う。

### Requirement 2: 10.1の非実装境界維持

**Objective:** 学習用RTOS開発者として、10.1が協調スケジューリング完成回ではないことをログと構造から確認したい。

#### Acceptance Criteria

1. When `yield_tsk()` がRUNNING current taskから呼ばれたとき, the kernel shall `switch-not-connected-yet` 相当の延期理由をログへ出力する。
2. While 10.1の実装である間, the kernel shall `yield_tsk()` 内でRUNNING taskをREADYへ戻さない。
3. While 10.1の実装である間, the kernel shall `yield_tsk()` 内で次task選択を行わない。
4. While 10.1の実装である間, the kernel shall `yield_tsk()` 内でdispatcherまたはcontext switchを呼び出さない。

### Requirement 3: 既存smokeと文書化の維持

**Objective:** 学習用RTOS開発者として、9.xで作った検証モデルを壊さずに10.1の追加分だけを確認したい。

#### Acceptance Criteria

1. When 通常の起動smokeを実行したとき, the kernel shall 9.1の task_b から task_c への task-to-task context switch smoke を維持する。
2. When 通常の起動smokeを実行したとき, the kernel shall 9.2の `dispatcher_switch_to(from, to)` 境界ログを維持する。
3. When 通常の起動smokeを実行したとき, the kernel shall 9.3のRUNNING/READY状態遷移ログを維持する。
4. When 通常の起動smokeを実行したとき, the kernel shall 9.4の entry return からDORMANTへの確定ログを維持する。
5. The kernel documentation shall README、spec、Doxygenコメントで10.1の到達点と未実装範囲を明示する。
6. The repository shall `docs/logs/qemu-serial.log` に10.1の観測結果を反映する。
