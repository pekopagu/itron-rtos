# Requirements Document

## Introduction
この feature は、μITRON風RTOSの第3章 3.3「currentタスクとRUNNING状態」として、READYタスクの選択結果を kernel runtime / dispatch 境界で current task として確定するための要求を定義する。

既存の `task-management-initial` は TCB、task_table、タスク登録、task dump、タスク状態定義を扱うが、currentタスク管理は扱わない。既存の `simple-priority-scheduler` は READY 状態のタスクから次候補を選択するが、currentタスク管理、TCB状態変更、READY から RUNNING への状態遷移は扱わない。

本 feature は両者の間に位置する状態確定の振る舞いを対象とし、`scheduler_select_next()` が返した selected task を kernel/dispatch 層で current task として commit し、READY → RUNNING の論理状態遷移を観測可能にする。RUNNING は CPU 実行中を意味せず、「schedulerにより選択されたタスクが、kernel/dispatch層により current として採用済みである状態」と定義する。

## Boundary Context
- **In scope**: current task の未設定状態と設定状態、selected task と current task の区別、READY → RUNNING の論理状態遷移、scheduler の副作用なし選択の維持、task table の非公開性、QEMUシリアルログでの selected と committed の区別、公開APIコメントの要求。
- **Out of scope**: task entry関数の実行、`task_start()`、`sta_tsk()` などのμITRON風外部API、コンテキストスイッチ、アセンブラ、レジスタ保存・復元、スタック切り替え、割り込み、タイマ、プリエンプション、動的メモリ。
- **Adjacent expectations**: `task-management-initial` は TCBと基本状態を提供し、`simple-priority-scheduler` は READYタスクの選択結果のみを返す。第4章では current task と RUNNING 状態を利用して task entry 実行モデルへ接続する。

## Requirements

### Requirement 1: currentタスク管理
**Objective:** As a kernel開発者, I want current task の概念を状態確定結果として扱えること, so that schedulerの選択結果をタスク実行前の最終状態として観測できる

#### Acceptance Criteria
1. The current-task-running-state feature shall allow the current task to be represented as unset before any selected task is committed.
2. When the kernel/dispatch layer commits a selected READY task, the current-task-running-state feature shall treat that task as the current task.
3. When the current task is queried or logged after a successful commit, the current-task-running-state feature shall identify the committed task as current.
4. While no task has been committed, the current-task-running-state feature shall not report any registered task as current.
5. When task registration and scheduler selection have completed but commit has not completed, the current-task-running-state feature shall keep the current task unset.

### Requirement 2: selected task と current task の分離
**Objective:** As a kernel開発者, I want schedulerが返す候補とcommit済みのcurrentを区別したい, so that 選択と状態確定の責務を混同せずに検証できる

#### Acceptance Criteria
1. When `scheduler_select_next()` returns a READY task, the current-task-running-state feature shall treat the returned task as a selected task.
2. When a task is only selected and not committed, the current-task-running-state feature shall not treat that task as the current task.
3. When a task is only selected and not committed, the current-task-running-state feature shall keep that task state unchanged by the selection result.
4. When the selected task is committed by the kernel/dispatch layer, the current-task-running-state feature shall treat the selected task as the current task.
5. When selected task information and current task information are both observed, the current-task-running-state feature shall allow them to be distinguished as separate concepts.

### Requirement 3: READYからRUNNINGへの論理状態遷移
**Objective:** As a kernel開発者, I want commitによりREADYタスクをRUNNINGへ遷移させたい, so that RUNNING状態をタスク実行前の論理的なcurrent確定状態として扱える

#### Acceptance Criteria
1. When a selected task in `TASK_STATE_READY` is committed, the current-task-running-state feature shall transition that task state to `TASK_STATE_RUNNING`.
2. When a selected task in `TASK_STATE_READY` is committed, the current-task-running-state feature shall make that task the current task.
3. If the selected task is not in `TASK_STATE_READY`, then the current-task-running-state feature shall reject the commit.
4. If the selected task is not in `TASK_STATE_READY`, then the current-task-running-state feature shall not change the current task.
5. If the selected task is not in `TASK_STATE_READY`, then the current-task-running-state feature shall not change that task state to `TASK_STATE_RUNNING`.
6. The current-task-running-state feature shall define `TASK_STATE_RUNNING` as a logical state meaning that a scheduler-selected task has been adopted as current by the kernel/dispatch layer.
7. The current-task-running-state feature shall not define `TASK_STATE_RUNNING` as proof that the task entry function is executing on the CPU.

### Requirement 4: scheduler責務の維持
**Objective:** As a kernel開発者, I want schedulerをREADYタスクの選択だけに限定したい, so that current管理とRUNNING遷移をschedulerから分離できる

#### Acceptance Criteria
1. When `scheduler_select_next()` is called, the simple-priority-scheduler feature shall select only from tasks in `TASK_STATE_READY`.
2. When `scheduler_select_next()` returns a task, the simple-priority-scheduler feature shall not modify the returned task state.
3. When `scheduler_select_next()` returns a task, the simple-priority-scheduler feature shall not make the returned task current.
4. When `scheduler_select_next()` returns a task, the simple-priority-scheduler feature shall not transition any task to `TASK_STATE_RUNNING`.
5. While scheduler selection is exercised, the simple-priority-scheduler feature shall not retain current task state.
6. If no READY task exists, then `scheduler_select_next()` shall continue to report that no selected task exists without changing any task state.

### Requirement 5: task管理との責務分離
**Objective:** As a kernel開発者, I want TCB状態変更を明示的なtask管理の振る舞いとして扱いたい, so that task_tableを直接公開せずに状態遷移を検証できる

#### Acceptance Criteria
1. The current-task-running-state feature shall keep `task_table` from being directly exposed as externally writable state.
2. When a task state must be changed for current commit, the current-task-running-state feature shall require the state change to occur through an explicit task-management behavior.
3. When a task state is changed to `TASK_STATE_RUNNING`, the current-task-running-state feature shall make the change observable through task dump or equivalent task-state log output.
4. When scheduler code observes task records, the current-task-running-state feature shall preserve read-only scheduler observation of task records.
5. While current commit behavior is exercised, the current-task-running-state feature shall not require callers to write directly to `task_table`.

### Requirement 6: ログと観測性
**Objective:** As a kernel開発者, I want selectedとcurrent/RUNNING committedをログで区別したい, so that QEMU上で状態確定の流れを確認できる

#### Acceptance Criteria
1. When scheduler selection succeeds during boot-time verification, the current-task-running-state feature shall make the selected task observable in QEMU serial log output.
2. When current commit succeeds during boot-time verification, the current-task-running-state feature shall make the current/RUNNING committed task observable in QEMU serial log output.
3. When both selection and commit are exercised, the current-task-running-state feature shall make selected output distinguishable from current/RUNNING committed output.
4. When task state output is produced after current commit, the current-task-running-state feature shall make `RUNNING` observable for the committed task.
5. If current commit is rejected, then the current-task-running-state feature shall make the rejection observable without reporting a new current/RUNNING committed task.

### Requirement 7: Doxygen形式コメント
**Objective:** As a kernel開発者, I want 公開ヘッダの追加APIとcurrent commitの制約をコメントで確認したい, so that 第4章以降の拡張時に今回の責務境界を誤解しない

#### Acceptance Criteria
1. Where a public header API is added or changed by this feature, the current-task-running-state feature shall provide a Doxygen-format comment for that API.
2. Where a public header API is added or changed by this feature, the Doxygen-format comment shall describe the API responsibility.
3. Where a public header API is added or changed by this feature, the Doxygen-format comment shall describe what this feature does.
4. Where a public header API is added or changed by this feature, the Doxygen-format comment shall describe what this feature does not do.
5. Where a public header API is added or changed by this feature, the Doxygen-format comment shall describe the future extension assumption.
6. Where current commit behavior is documented, the current-task-running-state feature shall state that the behavior performs only the logical READY → RUNNING state transition.
7. Where current commit behavior is documented, the current-task-running-state feature shall state that the behavior does not call the task entry function.
8. Where current commit behavior is documented, the current-task-running-state feature shall state that the behavior does not perform context switching.
9. Where current commit behavior is documented, the current-task-running-state feature shall state that the behavior does not switch stacks.
10. Where current commit behavior is documented, the current-task-running-state feature shall state that the behavior does not save or restore registers.
11. Where comments are added or changed by this feature, the current-task-running-state feature shall keep the comments consistent with the implemented behavior.
12. Where comments are added or changed by this feature, the current-task-running-state feature shall not refer to existing RTOS implementation code or structure.

### Requirement 8: 非要求と実行制御の除外
**Objective:** As a kernel開発者, I want current/RUNNING導入を実行制御から切り離したい, so that 第3章では状態確定だけを完了条件にできる

#### Acceptance Criteria
1. When current commit succeeds, the current-task-running-state feature shall not call the committed task entry function.
2. When current commit succeeds, the current-task-running-state feature shall not introduce `task_start()`.
3. When current commit succeeds, the current-task-running-state feature shall not introduce μITRON-style external APIs such as `sta_tsk()`.
4. When current commit succeeds, the current-task-running-state feature shall not perform a context switch.
5. When current commit succeeds, the current-task-running-state feature shall not use assembly code.
6. When current commit succeeds, the current-task-running-state feature shall not save or restore CPU registers.
7. When current commit succeeds, the current-task-running-state feature shall not switch stacks.
8. When current commit succeeds, the current-task-running-state feature shall not handle interrupts, timers, or preemption.
9. While current-task-running-state behavior is exercised, the current-task-running-state feature shall not require dynamic memory allocation.

### Requirement 9: 第4章への接続
**Objective:** As a kernel開発者, I want current task と RUNNING状態を第4章の入力として残したい, so that タスク実行モデルを段階的に追加できる

#### Acceptance Criteria
1. When current commit succeeds, the current-task-running-state feature shall make the committed current task available as the task that future execution-model work can use.
2. When current commit succeeds, the current-task-running-state feature shall preserve the committed task entry information without executing it.
3. When current commit succeeds, the current-task-running-state feature shall preserve the committed task stack information without switching to it.
4. The current-task-running-state feature shall keep the completion condition limited to current task adoption and logical RUNNING state observability.
5. The current-task-running-state feature shall leave task entry execution and context-switch-based execution model behavior to future work.

