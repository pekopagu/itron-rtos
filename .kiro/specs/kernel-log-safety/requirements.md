# Requirements Document

## Introduction
この feature は、第8回 `simple-priority-scheduler` 実装後の小改善として、kernel側の scheduler 選択結果ログ関数 `kernel_log_scheduler_selection(const char *label, const tcb_t *task)` のNULL安全性と可読性を改善する。

対象ユーザーは kernel 開発者である。現在は scheduler の選択結果を QEMU シリアルログで確認できるが、ログ補助関数内の `task == 0` 判定や `label`、`task->name`、状態文字列のNULL扱いが明確ではない。この feature により、正常系ログ形式を保ったまま、NULL入力でもクラッシュせず読みやすい代替文字列を表示できるようにする。

## Boundary Context
- **In scope**: `kernel_log_scheduler_selection()` のNULL判定、`label` のNULL表示、`task->name` のNULL表示、状態文字列のNULL表示、同関数のDoxygenコメント維持、既存ログ形式の維持、buildとQEMUログ確認。
- **Out of scope**: scheduler API変更、`scheduler.c` / `scheduler.h` 変更、task API変更、Makefile変更、task実行、RUNNING遷移、コンテキストスイッチ、割り込み、タイマ、プリエンプション、動的メモリ、標準 `printf` 導入。
- **Adjacent expectations**: schedulerの責務は第8回設計どおり「選択のみ」に留め、ログ責務はkernel側に残す。HAL / arch 境界を維持し、既存RTOS実装のコード構造を模倣しない。

## Requirements

### Requirement 1: NULL判定とログ表示の安全化
**Objective:** As a kernel 開発者, I want scheduler選択結果ログがNULL入力を安全に扱うこと, so that QEMUログ確認時に補助ログ関数でクラッシュしない

#### Acceptance Criteria
1. When `kernel_log_scheduler_selection()` checks whether `task` is absent, the kernel-log-safety feature shall use an explicit `NULL` comparison.
2. If `label` is `NULL`, then the kernel-log-safety feature shall display `"(no-label)"` as the label text.
3. If `label` is not `NULL`, then the kernel-log-safety feature shall display the provided label text.
4. If `task` is `NULL`, then the kernel-log-safety feature shall display `selected: none`.
5. If `task` is not `NULL` and `task->name` is `NULL`, then the kernel-log-safety feature shall display `"(null)"` as the task name.
6. If `task` is not `NULL` and `task->name` is not `NULL`, then the kernel-log-safety feature shall display the provided task name.
7. If the state string resolved for a selected task is `NULL`, then the kernel-log-safety feature shall display `"UNKNOWN"` as the state text.
8. If the state string resolved for a selected task is not `NULL`, then the kernel-log-safety feature shall display the resolved state text.

### Requirement 2: 既存ログ形式の維持
**Objective:** As a kernel 開発者, I want 正常系のscheduler選択ログ形式が大きく変わらないこと, so that 第8回の既存QEMU確認手順をそのまま使える

#### Acceptance Criteria
1. When no task is selected, the kernel-log-safety feature shall keep a log line containing `selected: none`.
2. When a task is selected, the kernel-log-safety feature shall keep a log line containing `selected: id=`, `name=`, `prio=`, and `state=`.
3. When QEMU is run with serial output, the kernel-log-safety feature shall allow both the no-selection log and selected-task log to remain observable.
4. While normal scheduler verification is exercised, the kernel-log-safety feature shall keep the selected task identity, priority, and state visible in the log.

### Requirement 3: scheduler責務と既存APIの非変更
**Objective:** As a kernel 開発者, I want ログ安全化がschedulerやtaskの公開契約を変更しないこと, so that 第8回simple-priority-schedulerの設計境界を維持できる

#### Acceptance Criteria
1. The kernel-log-safety feature shall not change scheduler selection behavior.
2. The kernel-log-safety feature shall not move logging responsibility into scheduler behavior.
3. The kernel-log-safety feature shall not require changes to `scheduler.c`.
4. The kernel-log-safety feature shall not require changes to `scheduler.h`.
5. The kernel-log-safety feature shall not require changes to task public APIs.
6. The kernel-log-safety feature shall not require changes to build configuration.
7. While scheduler selection is logged, the kernel-log-safety feature shall preserve the existing HAL / arch boundary.

### Requirement 4: 第8回の非実行制約の維持
**Objective:** As a kernel 開発者, I want ログ改善がタスク実行機構を混入しないこと, so that 第8回の「選択のみ」方針を壊さずに安全化できる

#### Acceptance Criteria
1. The kernel-log-safety feature shall not introduce `task_start()`.
2. The kernel-log-safety feature shall not introduce `context_switch()`.
3. The kernel-log-safety feature shall not call any task entry function.
4. The kernel-log-safety feature shall not change any selected task to `RUNNING`.
5. The kernel-log-safety feature shall not switch stacks.
6. The kernel-log-safety feature shall not introduce interrupt-driven scheduling.
7. The kernel-log-safety feature shall not introduce timer-driven scheduling.
8. The kernel-log-safety feature shall not introduce preemption.
9. The kernel-log-safety feature shall not use dynamic memory allocation.
10. The kernel-log-safety feature shall not use standard `printf` output.

### Requirement 5: コメントと検証
**Objective:** As a kernel 開発者, I want 変更後もコメントと検証条件が明確であること, so that 小改善の完了を安全に判断できる

#### Acceptance Criteria
1. The kernel-log-safety feature shall preserve Doxygen-style comments for `kernel_log_scheduler_selection()`.
2. The kernel-log-safety feature shall document that the log helper is display-only and does not execute tasks or change task state.
3. When the project is built, the kernel-log-safety feature shall allow `make` to complete successfully.
4. When QEMU serial verification is run, the kernel-log-safety feature shall allow the normal scheduler selection logs to remain equivalent in meaning to the existing logs.
5. The kernel-log-safety feature shall keep the code structure independent from existing RTOS implementation code structures.
