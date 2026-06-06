# Design Document

## Overview

14.1 では既存の task table を基盤に、μITRON 風 API 層として `cre_tsk()` と `sta_tsk()` を追加する。既存 `task_register()` は登録直後 READY という従来検証の契約を維持し、`cre_tsk()` 用には DORMANT 登録を行う task 管理 helper を追加する。

`sta_tsk()` は DORMANT task だけを READY に変える task-context API である。READY 化後は scheduler が READY 候補として選べる状態になり、current より高優先度であれば既存の scheduler preemption decision と dispatch pending request 境界へ接続する。timer IRQ handler 本体から `cre_tsk()` / `sta_tsk()` / `dispatcher_switch_to()` は呼ばない。

## Goals

- `itron_task_create_param_t` によって task 生成属性を API 層で表現する。
- `cre_tsk()` は TCB 登録と DORMANT 初期化だけを担当する。
- `sta_tsk()` は DORMANT から READY への起動だけを担当する。
- READY 化後の高優先度 task は既存の preemption pending 境界へ接続する。
- 既存の `task_register()` と既存 smoke 経路を維持する。

## Non-Goals

- task 削除、強制終了、exit 系 API。
- `act_tsk()` と activation count。
- `stacd` / `tskatr` の本格対応。
- 動的 stack 確保、priority 変更、エラー体系整理。
- 同一優先度 time slice、round-robin。
- timer IRQ handler 本体からの task API 呼び出しや直接 dispatch。

## Boundary Commitments

### This Spec Owns

- `kernel/include/itron_api.h` の `itron_task_create_param_t`, `cre_tsk()`, `sta_tsk()` public API。
- `kernel/itron_api.c` の task-context API ログ、入力検証、DORMANT 登録、READY 起動、preemption pending 接続。
- `kernel/include/task.h` / `kernel/task.c` の DORMANT 登録 helper と DORMANT->READY helper。
- `kernel/kernel.c` の 14.1 boot-time smoke。
- README、Doxygen、`docs/logs/qemu-serial.log`、spec 成果物。

### Out of Boundary

- `task_register()` の既存 READY 登録契約変更。
- scheduler の priority 選択ルール変更。
- dispatcher の switch 実装変更。
- semaphore / delay queue / timer IRQ handler の責務拡張。
- interrupt exit boundary の本格 frame switch 実装。

### Allowed Dependencies

- `itron_api.c` は `task_create_dormant()`, `task_start_dormant()`, `task_get_by_id()`, `dispatcher_get_current()`, `dispatch_request_from_task_start()` を利用してよい。
- `task.c` は TCB 所有者として state / wait metadata / stack metadata を更新してよい。
- `sta_tsk()` の pending request は既存 `dispatch_pending` module へ委譲する。

### Revalidation Triggers

- `tcb_t` の state / wait metadata / stack metadata 契約変更。
- `scheduler_select_next()` または `scheduler_select_preemption_candidate()` の選択条件変更。
- `dispatch_request_from_irq()` の呼び出し契約変更。
- timer IRQ handler の呼び出し順序変更。

## Architecture

### Existing Architecture Analysis

`task_register()` は未使用 TCB を探し、ID を採番し、entry / priority / stack metadata を設定して READY 状態で登録する。scheduler は `TASK_STATE_READY` の task だけを選択対象にする。dispatcher は READY を RUNNING に commit し、既存 API 層は必要に応じて scheduler / dispatcher / pending 境界へ進む。

14.1 ではこの既存経路を変えず、DORMANT 登録用に `task_create_dormant()` を追加する。実装の重複を小さくするため、TCB 初期化処理は task module 内に寄せる。`sta_tsk()` 用には `task_start_dormant()` を追加し、DORMANT 以外を `TASK_ERR_BAD_STATE` として扱う。

### API Flow

```text
cre_tsk(tskid, pk_ctsk)
  -> validate tskid and parameter fields
  -> task_create_dormant(tskid, name, entry, priority, stack_base, stack_size)
  -> log created DORMANT task

sta_tsk(tskid)
  -> log current task and target ID
  -> validate target exists and state is DORMANT
  -> task_start_dormant(tskid)
  -> compare started task with current
  -> if started task has higher priority:
       dispatch_request_from_task_start(current, started_task)
       log pending set reason=task-start
```

`dispatch_request_from_task_start()` は `sta_tsk()` 起点の pending state 保存境界として current/started task を保持するだけで dispatcher を直接呼ばない。14.1 では `reason=task-start` で `sta_tsk()` 起点を観測する。

## File Structure Plan

### Modified Files

- `kernel/include/itron_api.h`: `itron_task_create_param_t`、`CRE_TSK_*`、`STA_TSK_*`、`cre_tsk()`、`sta_tsk()` の Doxygen 宣言を追加する。
- `kernel/itron_api.c`: `cre_tsk()` / `sta_tsk()`、14.1 用ログ helper、preemption pending 接続を追加する。
- `kernel/include/task.h`: `task_create_dormant()` と `task_start_dormant()` の宣言を追加する。
- `kernel/task.c`: DORMANT 登録、DORMANT->READY 起動、共有 TCB 初期化処理、ログを追加する。
- `kernel/kernel.c`: 14.1 の boot-time smoke を追加し、DORMANT/READY/invalid/preemption pending を観測する。
- `README.md`: 14.1 到達点、非対応範囲、Zenn Articles tag 候補を追加する。
- `docs/logs/qemu-serial.log`: fresh `make run` 出力へ更新する。
- `.kiro/specs/create-start-task-api/requirements.md`, `design.md`, `tasks.md`: 最終 spec 成果物として保持する。

## Requirements Traceability

| Requirement | Components | Interfaces |
| --- | --- | --- |
| 1.1, 1.2, 1.3, 1.4, 1.5 | ItronTaskAPI, TaskState | `itron_task_create_param_t`, `cre_tsk()` |
| 2.1, 2.2, 2.3, 2.4 | TaskState, Scheduler | `task_create_dormant()`, `scheduler_select_next()` |
| 3.1, 3.2, 3.3, 3.4 | ItronTaskAPI, TaskState, Scheduler | `sta_tsk()`, `task_start_dormant()` |
| 4.1, 4.2, 4.3, 4.4, 4.5 | ItronTaskAPI, TaskState | `sta_tsk()` |
| 5.1, 5.2, 5.3, 5.4, 5.5 | PreemptionIntegration, DispatchPending | `dispatch_request_from_task_start()` |
| 6.1, 6.2, 6.3, 6.4, 6.5, 6.6 | RuntimeSmoke, DocumentationEvidence | `make`, `make run`, `rg` |

## Components and Interfaces

| Component | Domain | Intent | Req Coverage | Contracts |
| --- | --- | --- | --- | --- |
| ItronTaskAPI | API Layer | `cre_tsk()` / `sta_tsk()` の task-context API | 1, 3, 4, 5 | C API |
| TaskState | Task Management | DORMANT 登録と DORMANT->READY 起動 | 1, 2, 3, 4 | State |
| PreemptionIntegration | Scheduling | READY 化後の高優先度判定と pending request | 5 | Service |
| RuntimeSmoke | Validation | boot log で 14.1 到達点を観測 | 6 | Artifact |

### ItronTaskAPI

```c
typedef struct {
    task_entry_t entry;
    int priority;
    void *stack_base;
    unsigned long stack_size;
    const char *name;
} itron_task_create_param_t;

int cre_tsk(int tskid, const itron_task_create_param_t *pk_ctsk);
int sta_tsk(int tskid);
```

- Preconditions: `tskid > 0`, `pk_ctsk != NULL`, `entry != NULL`, `stack_base != NULL`, `stack_size > 0`, `name != NULL`。
- Postconditions: `cre_tsk()` 成功後は対象 task が DORMANT。`sta_tsk()` 成功後は対象 task が READY。
- Invariants: API 層は timer IRQ handler 本体から呼ばれない。`sta_tsk()` は DORMANT 以外を変更しない。

### TaskState

```c
int task_create_dormant(
    int task_id,
    const char *name,
    task_entry_t entry,
    int priority,
    void *stack_base,
    unsigned long stack_size
);

int task_start_dormant(int task_id);
```

`task_create_dormant()` は指定 ID の重複を拒否し、未使用 TCB へ DORMANT として登録する。`task_start_dormant()` は DORMANT だけを READY にし、wait metadata を未待ち状態に整える。

## Testing Strategy

- `make` で通常 build が通ることを確認する。
- `make run` で `cre_tsk()`、DORMANT、`sta_tsk()`、READY 候補、不正状態拒否、高優先度起動時 pending を確認する。
- `make run VALIDATE_TIMER_IRQ_ENTRY=1` で timer IRQ deferred dispatch 経路が壊れていないことを確認する。
- `rg` で timer IRQ handler 本体から `cre_tsk()` / `sta_tsk()` / `dispatcher_switch_to()` を直接呼んでいないことを確認する。
- `.kiro/specs/create-start-task-api/` が最終的に `requirements.md`, `design.md`, `tasks.md` の 3 ファイルだけであることを確認する。
