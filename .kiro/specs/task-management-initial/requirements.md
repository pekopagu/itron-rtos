# Requirements Document

## Introduction
この feature は、μITRON風RTOSの第7回として初期タスク管理機能を定義する。
現在の kernel は x86_64 + QEMU で起動し、HAL 経由で COM1 シリアル出力を行えるが、OS内部でタスクを管理対象として登録・一覧確認する仕組みをまだ持っていない。

この feature により、開発者はタスクを実行せずに、TCB、タスク状態、最大256件の静的タスクテーブル、ID採番、タスク登録、タスク一覧出力、スタック情報保持を確認できるようになる。
確認は QEMU `-serial stdio` のログで行い、kernel からのログ出力は HAL 経由のみとする。

## Boundary Context
- **In scope**: TCB定義、タスク状態定義、最大256件の静的タスクテーブル、`task_init`、`task_register`、`task_dump`、ID採番、負のエラーコード、スタック情報保持、HAL経由のログ出力、QEMUシリアルログ確認。
- **Out of scope**: スケジューラ、コンテキストスイッチ、タスク実行、entry関数呼び出し、スタック初期化、コンテキスト作成、割り込み、タイマ、動的メモリ。
- **Adjacent expectations**: 既存の HAL console 境界を維持し、kernel 共通部は arch 固有の serial 実装を直接利用しない。既存の起動ログと QEMU 起動確認の流れを維持する。

## Requirements

### Requirement 1: TCBとタスク状態の定義
**Objective:** As a kernel開発者, I want タスク管理対象を表す最小のTCBと状態を定義したい, so that タスク実行前の段階でOS内部のタスク情報を一貫して扱える

#### Acceptance Criteria
1. The task-management-initial feature shall define a task entry function pointer type equivalent to `typedef void (*task_entry_t)(void)`.
2. The task-management-initial feature shall define task states `TASK_STATE_UNUSED`, `TASK_STATE_DORMANT`, `TASK_STATE_READY`, and `TASK_STATE_RUNNING`.
3. The task-management-initial feature shall define a TCB that holds `id`, `name`, `entry`, `priority`, `state`, `stack_base`, and `stack_size`.
4. The task-management-initial feature shall represent `id` as an integer value where `0` is invalid.
5. The task-management-initial feature shall retain stack information as `stack_base` and `stack_size` without requiring stack initialization.

### Requirement 2: 静的タスクテーブル
**Objective:** As a kernel開発者, I want 固定長のタスクテーブルでタスクを管理したい, so that 動的メモリなしで登録済みタスクを追跡できる

#### Acceptance Criteria
1. The task-management-initial feature shall define the maximum number of tasks as `MAX_TASKS` equal to `256`.
2. The task-management-initial feature shall manage task records using a fixed-length table with `MAX_TASKS` entries.
3. The task-management-initial feature shall not use dynamic memory allocation for task records.
4. The task-management-initial feature shall determine an empty slot only by `state == TASK_STATE_UNUSED`.
5. The task-management-initial feature shall not determine an empty slot by `id == 0`.
6. The task-management-initial feature shall not determine an empty slot by `name == NULL`.

### Requirement 3: task_initによる初期化
**Objective:** As a kernel開発者, I want 起動時にタスク管理状態を既知の値へ初期化したい, so that 登録処理と一覧出力を決定的に確認できる

#### Acceptance Criteria
1. When `task_init()` is called, the task-management-initial feature shall initialize every slot in the task table.
2. When `task_init()` is called, the task-management-initial feature shall set every slot state to `TASK_STATE_UNUSED`.
3. When `task_init()` is called, the task-management-initial feature shall set every slot `id` to `0`.
4. When `task_init()` is called, the task-management-initial feature shall set every slot `name`, `entry`, and `stack_base` to `NULL`.
5. When `task_init()` is called, the task-management-initial feature shall set every slot `priority` and `stack_size` to `0`.
6. When `task_init()` is called, the task-management-initial feature shall initialize `next_task_id` to `1`.
7. When `task_init()` completes, the task-management-initial feature shall make the task table ready to accept new registrations.

### Requirement 4: task_registerによるタスク登録
**Objective:** As a kernel開発者, I want タスク情報を登録してIDを受け取りたい, so that OS内部で管理対象タスクを識別できる

#### Acceptance Criteria
1. When `task_register()` succeeds, the task-management-initial feature shall return a task id greater than or equal to `1`.
2. When `task_register()` succeeds, the task-management-initial feature shall store the provided `name`, `entry`, `priority`, `stack_base`, and `stack_size` in the selected task record.
3. When `task_register()` succeeds, the task-management-initial feature shall set the registered task state to `TASK_STATE_READY`.
4. When `task_register()` succeeds, the task-management-initial feature shall not call the registered `entry` function.
5. When `task_register()` succeeds, the task-management-initial feature shall not initialize the provided stack memory.
6. When `task_register()` succeeds, the task-management-initial feature shall not create an execution context.
7. If `name == NULL`, then the task-management-initial feature shall reject the registration with a negative error code.
8. If `entry == NULL`, then the task-management-initial feature shall reject the registration with a negative error code.
9. If `stack_base == NULL`, then the task-management-initial feature shall reject the registration with a negative error code.
10. If `stack_size == 0`, then the task-management-initial feature shall reject the registration with a negative error code.
11. If no slot has `state == TASK_STATE_UNUSED`, then the task-management-initial feature shall reject the registration with a negative error code.

### Requirement 5: エラーコード
**Objective:** As a kernel開発者, I want 登録失敗理由を負のエラーコードで判別したい, so that テストとログ確認で失敗原因を切り分けられる

#### Acceptance Criteria
1. The task-management-initial feature shall define `TASK_ERR_FULL` as a negative error code for a full task table.
2. The task-management-initial feature shall define `TASK_ERR_INVAL` as a negative error code for invalid arguments.
3. The task-management-initial feature shall define `TASK_ERR_ID_OVERFLOW` as a negative error code for task id overflow.
4. If `task_register()` receives an invalid argument, then the task-management-initial feature shall return `TASK_ERR_INVAL`.
5. If the task table is full, then the task-management-initial feature shall return `TASK_ERR_FULL`.
6. If the next task id cannot be assigned without overflow, then the task-management-initial feature shall return `TASK_ERR_ID_OVERFLOW`.

### Requirement 6: ID採番
**Objective:** As a kernel開発者, I want タスクIDを単純で再利用しない方式で採番したい, so that 登録済みタスクの識別が配列位置に依存しない

#### Acceptance Criteria
1. The task-management-initial feature shall assign task ids using a monotonically increasing `next_task_id`.
2. The task-management-initial feature shall initialize `next_task_id` to `1`.
3. The task-management-initial feature shall treat task id `0` as invalid.
4. The task-management-initial feature shall keep task ids independent from task table indexes.
5. The task-management-initial feature shall not reuse task ids during a boot session.
6. When a task id is assigned successfully, the task-management-initial feature shall advance `next_task_id`.
7. If advancing or assigning `next_task_id` would overflow, then the task-management-initial feature shall return `TASK_ERR_ID_OVERFLOW`.
8. If task id overflow is detected, then the task-management-initial feature shall not wrap `next_task_id` back to a previous value.

### Requirement 7: task_dumpによる一覧出力
**Objective:** As a kernel開発者, I want 登録済みタスクの一覧をシリアルログで確認したい, so that QEMU上で初期タスク管理状態を検証できる

#### Acceptance Criteria
1. When `task_dump()` is called, the task-management-initial feature shall output only registered task records.
2. When `task_dump()` is called, the task-management-initial feature shall not output slots whose state is `TASK_STATE_UNUSED`.
3. When `task_dump()` outputs a task record, the task-management-initial feature shall include the task `id`.
4. When `task_dump()` outputs a task record, the task-management-initial feature shall include the task `name`.
5. When `task_dump()` outputs a task record, the task-management-initial feature shall include the task `priority`.
6. When `task_dump()` outputs a task record, the task-management-initial feature shall include the task state as a state string.
7. When `task_dump()` outputs a task record, the task-management-initial feature shall include the task `entry` address.
8. When `task_dump()` outputs a task record, the task-management-initial feature shall include the task `stack_base`.
9. When `task_dump()` outputs a task record, the task-management-initial feature shall include the task `stack_size`.
10. When `task_dump()` outputs logs, the task-management-initial feature shall use HAL-provided console output only.

### Requirement 8: 非対象範囲の維持
**Objective:** As a kernel開発者, I want 第7回の対象を登録と一覧確認に限定したい, so that スケジューラや実行機構を混ぜずにタスク管理の土台を確認できる

#### Acceptance Criteria
1. The task-management-initial feature shall not include a scheduler.
2. The task-management-initial feature shall not include context switching.
3. The task-management-initial feature shall not execute registered tasks.
4. The task-management-initial feature shall not call registered entry functions.
5. The task-management-initial feature shall not initialize task stacks.
6. The task-management-initial feature shall not create task execution contexts.
7. The task-management-initial feature shall not use interrupts.
8. The task-management-initial feature shall not use timers.
9. The task-management-initial feature shall not use dynamic memory.

### Requirement 9: 起動時確認と完了条件
**Objective:** As a kernel開発者, I want QEMU起動時に初期タスク管理の動作をログで確認したい, so that 実装完了を最小環境で判断できる

#### Acceptance Criteria
1. When the project is built, the task-management-initial feature shall allow the kernel image to build successfully.
2. When the kernel is started on QEMU, the task-management-initial feature shall allow the kernel to boot successfully.
3. When QEMU is run with `-serial stdio`, the task-management-initial feature shall output task-management logs to the serial stream.
4. When boot-time registration is exercised, the task-management-initial feature shall allow multiple tasks to be registered by `task_register()`.
5. When boot-time dump is exercised, the task-management-initial feature shall allow registered tasks to be observed through `task_dump()`.
6. While boot-time registration and dump are exercised, the task-management-initial feature shall not execute any registered entry function.
7. While boot-time registration and dump are exercised, the task-management-initial feature shall not include scheduler behavior.
8. While boot-time registration and dump are exercised, the task-management-initial feature shall not include context-switch behavior.
