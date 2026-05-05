# Requirements Document

## Introduction
この feature は、μITRON風RTOSの第4章 4.1「entry関数の扱い」として、dispatcherでcurrentとしてcommit済みのタスクについて、TCBに保持されたentry関数を最小モデルで呼び出すための要求を定義する。

既存の `task-management-initial` はTCBとentry保持を扱うが、entry実行は扱わない。既存の `simple-priority-scheduler` はREADYタスク選択のみを扱い、実行や状態変更は行わない。既存の `current-task-running-state` はselected taskをcurrentとしてcommitし、READYからRUNNINGへの論理状態遷移を扱うが、entry関数は呼び出さない。

本 feature は、それらの責務境界を維持したまま、起動時検証フローで `scheduler_select_next()`、`dispatcher_commit_current(selected)`、`dispatcher_get_current()`、`current->entry()` の順に最小実行モデルを観測可能にする。4.1のRUNNINGは「currentとして採用済みの論理状態」に加えて「entry呼び出し対象になったcurrent task」を意味するが、CPUで継続実行中、独立スタック上で実行中、コンテキスト復元済みという意味は持たない。

## Boundary Context
- **In scope**: current taskのentry直接呼び出し、entry呼び出し前提条件の確認、entry呼び出し前ログ、entry returnログ、return後の暫定停止ループ、QEMUシリアルログでのentry実行観測、Doxygen形式コメント、既存責務分離の維持、第5章への接続条件。
- **Out of scope**: `task_runner.c` / `task_runner.h` の追加、コンテキストスイッチ、アセンブラ、レジスタ保存・復元、スタック切り替え、独立タスクスタック上での実行、割り込み、タイマ、プリエンプション、複数タスクの交互実行、正式なtask終了状態、μITRON互換API、既存RTOS実装の参照・コピー・流用。
- **Adjacent expectations**: schedulerはREADYタスク選択のみを維持する。dispatcherはcurrent commitのみを維持する。task管理はTCBと状態管理のみを維持する。第5章では4.1の直接entry呼び出しをコンテキストスイッチ境界へ置き換える。

## Requirements

### Requirement 1: current task entryの直接呼び出し
**Objective:** As a kernel開発者, I want current taskのentryを最小モデルで直接呼び出せること, so that コンテキストスイッチ導入前にentry関数の扱いをQEMUログで確認できる

#### Acceptance Criteria
1. When READY task selection and current commit have completed successfully, the task-entry-runner feature shall call the committed current task entry as a normal C function call.
2. When the task entry is called in 4.1, the task-entry-runner feature shall use the current task returned after current commit as the entry call target.
3. When the task entry is called in 4.1, the task-entry-runner feature shall not require a dedicated `task_runner.c` file.
4. When the task entry is called in 4.1, the task-entry-runner feature shall not require a dedicated `task_runner.h` file.
5. The task-entry-runner feature shall preserve the execution model order as scheduler selection, current commit, current acquisition, and current task entry call.

### Requirement 2: entry呼び出し前提条件
**Objective:** As a kernel開発者, I want entry呼び出し前の前提条件を明確にしたい, so that 未確定または不正なTCBを実行対象にしないことを確認できる

#### Acceptance Criteria
1. When the task-entry-runner feature is about to call an entry function, the task-entry-runner feature shall require the current task to be non-NULL.
2. When the task-entry-runner feature is about to call an entry function, the task-entry-runner feature shall require the current task state to be `TASK_STATE_RUNNING`.
3. When the task-entry-runner feature is about to call an entry function, the task-entry-runner feature shall require the current task entry to be non-NULL.
4. If the current task is NULL, then the task-entry-runner feature shall not call any task entry function.
5. If the current task state is not `TASK_STATE_RUNNING`, then the task-entry-runner feature shall not call that task entry function.
6. If the current task entry is NULL, then the task-entry-runner feature shall not call that task entry function.
7. If an entry precondition is not satisfied, then the task-entry-runner feature shall make the skipped entry call observable in boot-time log output.

### Requirement 3: RUNNING状態の4.1での意味
**Objective:** As a kernel開発者, I want RUNNINGの意味を4.1向けに拡張しすぎず定義したい, so that 第5章のコンテキストスイッチ設計と矛盾しない状態モデルを維持できる

#### Acceptance Criteria
1. The task-entry-runner feature shall continue to define `TASK_STATE_RUNNING` as a logical state meaning that a task has been adopted as current.
2. While 4.1 entry handling is exercised, the task-entry-runner feature shall treat `TASK_STATE_RUNNING` as the state of the current task that is eligible for entry calling.
3. While 4.1 entry handling is exercised, the task-entry-runner feature shall not define `TASK_STATE_RUNNING` as proof that the task is continuously executing on the CPU.
4. While 4.1 entry handling is exercised, the task-entry-runner feature shall not define `TASK_STATE_RUNNING` as proof that the task is executing on an independent task stack.
5. While 4.1 entry handling is exercised, the task-entry-runner feature shall not define `TASK_STATE_RUNNING` as proof that CPU context has been restored.
6. When entry handling documentation describes `TASK_STATE_RUNNING`, the task-entry-runner feature shall state the 4.1 meaning separately from future context-switch execution semantics.

### Requirement 4: entry呼び出しログとreturn観測
**Objective:** As a kernel開発者, I want entry呼び出し前後のログを確認したい, so that entryが実際に呼ばれたこととreturnしたことをQEMUシリアルログで追跡できる

#### Acceptance Criteria
1. When the task-entry-runner feature is about to call the current task entry, the task-entry-runner feature shall make the entry call attempt observable in QEMU serial log output.
2. When the entry call attempt is logged, the task-entry-runner feature shall identify the current task in the log output.
3. When the current task entry function executes, the task-entry-runner feature shall allow the task entry body log to appear in QEMU serial log output.
4. When the current task entry returns, the task-entry-runner feature shall make the entry return observable in QEMU serial log output.
5. When entry call and entry return logs are both emitted, the task-entry-runner feature shall allow the call-before-entry and return-after-entry events to be distinguished.
6. When QEMU is run with `-serial stdio`, the task-entry-runner feature shall make the entry call behavior verifiable from the serial stream.

### Requirement 5: entry return後の暫定扱い
**Objective:** As a kernel開発者, I want entry return後の扱いを暫定範囲に限定したい, so that 正式なtask終了状態を4.1へ持ち込まずに検証を終えられる

#### Acceptance Criteria
1. When the current task entry returns in 4.1, the task-entry-runner feature shall treat the return as an observable boot-time verification event.
2. When the current task entry returns in 4.1, the task-entry-runner feature shall proceed to the existing halt behavior or an equivalent stop loop.
3. When the current task entry returns in 4.1, the task-entry-runner feature shall not require a formal task exit state.
4. When the current task entry returns in 4.1, the task-entry-runner feature shall not require a transition from `TASK_STATE_RUNNING` to `TASK_STATE_DORMANT`.
5. When the current task entry returns in 4.1, the task-entry-runner feature shall not require a transition from `TASK_STATE_RUNNING` to any wait or termination state.
6. When the current task entry returns in 4.1, the task-entry-runner feature shall not schedule another task as a result of the return.

### Requirement 6: scheduler責務の維持
**Objective:** As a kernel開発者, I want schedulerをREADY task選択だけに保ちたい, so that entry実行の副作用がschedulerへ混入しないことを確認できる

#### Acceptance Criteria
1. When `scheduler_select_next()` is called, the task-entry-runner feature shall preserve the scheduler behavior as READY task selection only.
2. When `scheduler_select_next()` returns a task, the task-entry-runner feature shall not require scheduler behavior to call that task entry.
3. When `scheduler_select_next()` returns a task, the task-entry-runner feature shall not require scheduler behavior to change that task state.
4. When `scheduler_select_next()` returns a task, the task-entry-runner feature shall not require scheduler behavior to commit that task as current.
5. While 4.1 entry handling is exercised, the task-entry-runner feature shall preserve scheduler behavior as free of HAL console output responsibility.

### Requirement 7: dispatcher責務の維持
**Objective:** As a kernel開発者, I want dispatcherをcurrent commitだけに保ちたい, so that entry実行開始とcurrent確定を混同せずに設計を進められる

#### Acceptance Criteria
1. When `dispatcher_commit_current()` succeeds, the task-entry-runner feature shall preserve dispatcher behavior as current commit behavior.
2. When `dispatcher_commit_current()` succeeds, the task-entry-runner feature shall not require dispatcher behavior to call the committed task entry.
3. When `dispatcher_commit_current()` succeeds, the task-entry-runner feature shall not require dispatcher behavior to perform context switching.
4. When `dispatcher_commit_current()` succeeds, the task-entry-runner feature shall not require dispatcher behavior to switch stacks.
5. When `dispatcher_get_current()` is used for 4.1 entry handling, the task-entry-runner feature shall treat the returned current task as a read-only input for entry call validation.

### Requirement 8: task管理責務の維持
**Objective:** As a kernel開発者, I want task管理をTCBと状態管理に限定したい, so that entry実行制御をtask.cへ混ぜずに既存のtask table境界を維持できる

#### Acceptance Criteria
1. While 4.1 entry handling is exercised, the task-entry-runner feature shall preserve task management behavior as TCB storage and state management behavior.
2. When a task entry is called in 4.1, the task-entry-runner feature shall not require task management behavior to call the entry function.
3. When a task entry is called in 4.1, the task-entry-runner feature shall not require task management behavior to switch stacks.
4. When a task entry is called in 4.1, the task-entry-runner feature shall not require task management behavior to create or restore CPU context.
5. While 4.1 entry handling is exercised, the task-entry-runner feature shall keep task entry, stack base, and stack size as TCB information without treating them as independent stack execution state.

### Requirement 9: 非要求と実行制御の除外
**Objective:** As a kernel開発者, I want 4.1の非要求を明示したい, so that 最小entry実行モデルが第5章以降の責務を先取りしないことを確認できる

#### Acceptance Criteria
1. The task-entry-runner feature shall not introduce context switching.
2. The task-entry-runner feature shall not introduce assembly code for entry handling.
3. The task-entry-runner feature shall not save or restore CPU registers.
4. The task-entry-runner feature shall not switch stacks.
5. The task-entry-runner feature shall not execute the task entry on an independent task stack.
6. The task-entry-runner feature shall not introduce interrupt-driven execution.
7. The task-entry-runner feature shall not introduce timer-driven execution.
8. The task-entry-runner feature shall not introduce preemption.
9. The task-entry-runner feature shall not introduce alternating execution of multiple tasks.
10. The task-entry-runner feature shall not introduce a formal task termination state.
11. The task-entry-runner feature shall not introduce μITRON-compatible external APIs.
12. The task-entry-runner feature shall not refer to, copy, or adapt implementation code or structure from existing RTOS implementations.

### Requirement 10: Doxygen形式コメント
**Objective:** As a kernel開発者, I want 4.1のentry実行モデルと制約をDoxygen形式コメントで確認したい, so that 後続章で責務境界を誤解せずに拡張できる

#### Acceptance Criteria
1. Where public or file-level documentation is changed for 4.1, the task-entry-runner feature shall provide Doxygen-format comments.
2. Where entry calling behavior is documented, the task-entry-runner feature shall describe that the current task entry is called directly as a normal C function call.
3. Where entry calling behavior is documented, the task-entry-runner feature shall describe the entry call preconditions for current task, RUNNING state, and non-NULL entry.
4. Where entry return behavior is documented, the task-entry-runner feature shall describe the return handling as temporary boot-time verification behavior.
5. Where RUNNING state is documented for 4.1, the task-entry-runner feature shall describe that RUNNING does not mean independent stack execution or restored CPU context.
6. Where adjacent responsibilities are documented, the task-entry-runner feature shall state that scheduler selection, dispatcher commit, and task state management responsibilities remain separate.
7. Where comments are added or changed by this feature, the task-entry-runner feature shall keep the comments consistent with the observable behavior.

### Requirement 11: 第5章への接続条件
**Objective:** As a kernel開発者, I want 4.1の直接entry呼び出しを第5章で置き換えられる形にしたい, so that コンテキストスイッチ導入時に実行境界を明確に移行できる

#### Acceptance Criteria
1. The task-entry-runner feature shall leave the current task as the input to future context-switch-based execution.
2. The task-entry-runner feature shall leave the task entry pointer as the future initial execution target.
3. The task-entry-runner feature shall leave stack information as future context setup input without using it for 4.1 stack switching.
4. When 4.1 entry handling is documented, the task-entry-runner feature shall identify the direct entry call as behavior that future context-switch work can replace.
5. While 4.1 entry handling is exercised, the task-entry-runner feature shall preserve the selected-to-current-to-entry sequence as the conceptual connection to Chapter 5.
