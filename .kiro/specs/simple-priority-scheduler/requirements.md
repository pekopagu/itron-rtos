# Requirements Document

## Introduction
この feature は、μITRON風RTOSの第8回として、READY状態のタスクから次に実行対象とみなすタスクを1つ選択する簡易優先度スケジューラを追加する。
対象ユーザーは kernel 開発者である。現在は x86_64 + QEMU で最小 kernel が起動し、HAL console 経由で COM1 へログ出力できる。また、TCB、静的 task table、`task_init()`、`task_register()`、`task_dump()` による初期タスク管理は存在する。
この feature により、タスク実行やコンテキストスイッチを導入する前段階として、登録済みタスクの状態と優先度を管理し、READYタスクだけを対象に最小 priority 値のタスクを選択できるようにする。確認は QEMU `-serial stdio` のログで行う。

## Boundary Context
- **In scope**: タスク状態の定義、タスク優先度の保持、登録時の優先度設定、READYタスクからの次タスク選択、同一優先度時の登録順選択、選択結果のログ確認、公開要素のDoxygen文書化。
- **Out of scope**: 実タスク関数の実行、`task_start()`、`context_switch()`、スタック切り替え、割り込み、タイマ、プリエンプション、ラウンドロビン、動的メモリ、スリープ処理、待ち解除処理。
- **Adjacent expectations**: 既存の task 管理と HAL console 出力の境界を維持し、scheduler側に arch 依存の動作を持ち込まない。第9回以降のコンテキストスイッチ、タイマ割り込み、同一優先度ラウンドロビンが追加しやすいよう、今回の観測可能な責務を「選択のみ」に限定する。

## Requirements

### Requirement 1: タスク状態管理
**Objective:** As a kernel 開発者, I want タスクがスケジューラ選択に使える状態を持つこと, so that READYタスクだけを次タスク選択の候補として扱える

#### Acceptance Criteria
1. The simple-priority-scheduler feature shall define task states `DORMANT`, `READY`, `RUNNING`, and `WAITING` as task management states.
2. The simple-priority-scheduler feature shall keep `READY` as the primary selectable state for 第8回.
3. When a task is registered successfully, the simple-priority-scheduler feature shall make the task observable as `READY`.
4. While a task is not in `READY`, the simple-priority-scheduler feature shall exclude the task from next-task selection.
5. The simple-priority-scheduler feature shall keep task state observable in task dump or equivalent serial log output.

### Requirement 2: タスク優先度管理
**Objective:** As a kernel 開発者, I want 各タスクが優先度を持つこと, so that μITRON風に優先度順で次タスクを選べる

#### Acceptance Criteria
1. The simple-priority-scheduler feature shall allow each registered task to have a `priority` value.
2. When a task is registered, the simple-priority-scheduler feature shall store the priority specified for that task.
3. The simple-priority-scheduler feature shall treat a smaller numeric priority value as a higher priority.
4. When registered tasks are displayed, the simple-priority-scheduler feature shall make each task priority observable in serial log output.
5. The simple-priority-scheduler feature shall keep the maximum task count at `256`.

### Requirement 3: READYタスクからの次タスク選択
**Objective:** As a kernel 開発者, I want READYタスクの中から次に実行すべきタスクを1つ選択できること, so that 実行機構を入れる前にスケジューリング規則を検証できる

#### Acceptance Criteria
1. When next-task selection is requested, the simple-priority-scheduler feature shall consider only tasks whose state is `READY`.
2. When one or more `READY` tasks exist, the simple-priority-scheduler feature shall select one task with the smallest numeric priority value.
3. When multiple `READY` tasks have the same highest priority, the simple-priority-scheduler feature shall select the task that was registered earliest among them.
4. If no `READY` task exists, then the simple-priority-scheduler feature shall return or report that no selectable task exists.
5. When next-task selection completes with a selected task, the simple-priority-scheduler feature shall make the selected task identity and priority observable.
6. When next-task selection completes without a selected task, the simple-priority-scheduler feature shall make the no-selection result observable.

### Requirement 4: 選択のみで実行しない制約
**Objective:** As a kernel 開発者, I want 第8回の責務をタスク選択だけに限定すること, so that コンテキストスイッチ前の段階を安全に検証できる

#### Acceptance Criteria
1. When next-task selection is requested, the simple-priority-scheduler feature shall not call any registered task entry function.
2. When next-task selection is requested, the simple-priority-scheduler feature shall not switch stacks.
3. When next-task selection is requested, the simple-priority-scheduler feature shall not perform a context switch.
4. The simple-priority-scheduler feature shall not introduce `task_start()` as part of 第8回.
5. The simple-priority-scheduler feature shall not introduce interrupt-driven scheduling as part of 第8回.
6. The simple-priority-scheduler feature shall not introduce timer-driven scheduling as part of 第8回.
7. The simple-priority-scheduler feature shall not introduce preemption as part of 第8回.
8. While 第8回 scheduler behavior is exercised, the simple-priority-scheduler feature shall keep selected tasks from becoming executed tasks.

### Requirement 5: 静的管理と既存task管理との整合
**Objective:** As a kernel 開発者, I want 既存の静的タスク管理と整合したスケジューラ選択を行うこと, so that freestanding kernelで動的メモリなしに検証できる

#### Acceptance Criteria
1. The simple-priority-scheduler feature shall not require dynamic memory allocation.
2. The simple-priority-scheduler feature shall use the existing registered task records as the source of selectable tasks.
3. The simple-priority-scheduler feature shall preserve the fixed maximum task count of `256`.
4. The simple-priority-scheduler feature shall remain compatible with existing task registration and task dump behavior.
5. While the kernel is built as a freestanding C environment, the simple-priority-scheduler feature shall not require hosted runtime facilities.

### Requirement 6: QEMUシリアルログでの確認
**Objective:** As a kernel 開発者, I want QEMUシリアルログで登録状態と選択結果を確認できること, so that 実行機構なしで第8回の完了を判断できる

#### Acceptance Criteria
1. When multiple tasks are registered during boot-time verification, the simple-priority-scheduler feature shall allow their state and priority to be shown in serial log output.
2. When next-task selection is exercised during boot-time verification, the simple-priority-scheduler feature shall show the selected task in serial log output.
3. When QEMU is run with `-serial stdio`, the simple-priority-scheduler feature shall allow the scheduler selection result to be observed from the serial stream.
4. While scheduler verification logs are emitted, the simple-priority-scheduler feature shall not require direct arch-specific serial calls from scheduler behavior.
5. While scheduler verification logs are emitted, the simple-priority-scheduler feature shall preserve the HAL console boundary used by existing kernel logs.

### Requirement 7: 文書化とDoxygenコメント
**Objective:** As a kernel 開発者, I want schedulerとtask管理の公開要素がDoxygenで説明されること, so that 第8回の記事と将来拡張の意図をコードから追跡できる

#### Acceptance Criteria
1. The simple-priority-scheduler feature shall provide Doxygen comments for all public functions introduced or changed by this feature.
2. The simple-priority-scheduler feature shall provide explanatory comments for task state enum values.
3. The simple-priority-scheduler feature shall provide explanatory comments for TCB fields that are introduced or changed by this feature.
4. The simple-priority-scheduler feature shall document that next-task selection chooses only from `READY` tasks.
5. The simple-priority-scheduler feature shall document that smaller numeric priority values mean higher priority.
6. The simple-priority-scheduler feature shall document that same-priority tasks are selected by registration order in 第8回.
7. The simple-priority-scheduler feature shall document that 第8回 performs selection only and does not execute tasks.
8. The simple-priority-scheduler feature shall document the relationship to μITRON-style priority scheduling.
9. The simple-priority-scheduler feature shall document that scheduler behavior preserves the HAL boundary and does not own arch-specific serial output.

### Requirement 8: 将来拡張に対する観測可能な境界
**Objective:** As a kernel 開発者, I want 第8回の境界が将来拡張と混ざらないこと, so that 第9回以降でラウンドロビンやコンテキストスイッチを段階的に追加できる

#### Acceptance Criteria
1. The simple-priority-scheduler feature shall keep round-robin scheduling out of 第8回 behavior.
2. The simple-priority-scheduler feature shall keep sleep and wakeup behavior out of 第8回 behavior.
3. The simple-priority-scheduler feature shall keep context creation and context switching out of 第8回 behavior.
4. The simple-priority-scheduler feature shall keep timer interrupt scheduling out of 第8回 behavior.
5. While same-priority tasks are present, the simple-priority-scheduler feature shall use registration order rather than time slicing.
6. The simple-priority-scheduler feature shall make the 第8回 completion condition verifiable by task registration logs, task state and priority logs, and scheduler selection logs.
