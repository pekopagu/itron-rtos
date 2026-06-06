# Requirements Document

## Introduction

μITRON風RTOSを段階的に開発している開発者が、第14章14.3としてsemaphore系APIを整理する。14.1では `cre_tsk()` / `sta_tsk()` によりtask生成と起動を分離し、14.2では `slp_tsk()` / `wup_tsk()` によりsleep待ちとREADY復帰を追加した。今回は既存の `wai_sem()` / `sig_sem()` / `twai_sem()` 経路をμITRON風API層として見直し、`pol_sem()` による非ブロッキング取得、timeout付きsemaphore待ち、`sig_sem()` によるREADY復帰と既存preemption pending接続を観測できるようにする。

## Boundary Context

- **In scope**: `wai_sem(int semid)` / `sig_sem(int semid)` / `pol_sem(int semid)` / `twai_sem(int semid, unsigned int timeout_ticks)` の宣言と実装、semaphore count取得・返却、semaphore wait queue登録・削除、timeout付きsemaphore待ちのdelay queue整合、semaphore待ちtaskのREADY復帰、wakeup後のpreemption pending接続、14.3到達点のREADME・Doxygen・serial log・spec更新。
- **Out of scope**: priority順semaphore wait queue、semaphore属性の本格対応、`cre_sem`、`del_sem`、`ref_sem`、`rel_wai`、`sus_tsk` / `rsm_tsk`、`ter_tsk`、`ext_tsk`、`exd_tsk`、sleep待ちtimeout、wakeup要求カウント本格対応、同一優先度time slice、round-robin、timer IRQ handlerからのtask API呼び出し、timer IRQ handlerからの直接 `dispatcher_switch_to()` 呼び出し、既存RTOSコードの参照・コピー・流用。
- **Adjacent expectations**: 既存の `yield_tsk()`、`dly_tsk()` / delay queue / timer tick、`cre_tsk()` / `sta_tsk()`、`slp_tsk()` / `wup_tsk()`、timer IRQ deferred dispatch、scheduler READY候補選択の観測可能な挙動を維持する。

## Requirements

### Requirement 1: semaphore API入口の整理

**Objective:** 開発者がμITRON風semaphore APIを一貫したAPI層から呼び出し、14.3の対象APIと戻り値を観測できるようにする。

#### Acceptance Criteria

1. When `wai_sem(int semid)` / `sig_sem(int semid)` / `pol_sem(int semid)` / `twai_sem(int semid, unsigned int timeout_ticks)` are declared, the system shall expose them through the μITRON-like API header.
2. When each semaphore API is called, the system shall log the API name, semaphore ID, current task ID, current task name, current task priority when available, and current task state.
3. If current task is absent or not RUNNING for an API that requires task context, then the system shall reject the call without changing task state or queue membership.
4. If the semaphore ID is invalid, then the system shall reject the call without changing semaphore count, task state, or queue membership.
5. The system shall keep semaphore API behavior in the API layer observable as chapter 14.3 rather than only as earlier chapter foundation behavior.

### Requirement 2: `wai_sem()` blocking acquisition

**Objective:** 開発者がcountありの即時取得とcountなしのsemaphore待ち入りを区別して観測できるようにする。

#### Acceptance Criteria

1. When `wai_sem()` is called and semaphore count is greater than zero, the system shall decrement the count and return success without moving the current task to WAITING.
2. When `wai_sem()` acquisition succeeds immediately, the system shall log the count transition and `action=acquired`.
3. When `wai_sem()` is called and semaphore count is zero, the system shall move only the RUNNING current task to WAITING.
4. When `wai_sem()` moves a task to WAITING, the system shall set the wait reason to semaphore and preserve the waited semaphore ID.
5. While a task is WAITING with semaphore reason, the system shall not include the task in scheduler READY candidates.
6. When `wai_sem()` moves a task to WAITING, the system shall log RUNNING to WAITING transition, wait reason, semaphore ID, and `action=wait`.

### Requirement 3: `pol_sem()` non-blocking acquisition

**Objective:** 開発者が待ちに入らないsemaphore取得試行を利用し、失敗時にtask状態が変わらないことを観測できるようにする。

#### Acceptance Criteria

1. When `pol_sem()` is called and semaphore count is greater than zero, the system shall decrement the count and return success without moving the current task to WAITING.
2. When `pol_sem()` acquisition succeeds, the system shall log the count transition and `action=acquired`.
3. When `pol_sem()` is called and semaphore count is zero, the system shall return an immediate error.
4. When `pol_sem()` acquisition would block, the system shall not change task state, wait reason, wait semaphore ID, semaphore wait queue membership, or delay queue membership.
5. When `pol_sem()` acquisition would block, the system shall log `action=would-block` and evidence that WAITING transition did not occur.

### Requirement 4: `twai_sem()` timeout付きsemaphore待ち

**Objective:** 開発者がtimeout付きsemaphore待ちをsemaphore wait queueとdelay queueの両方で整合して観測できるようにする。

#### Acceptance Criteria

1. When `twai_sem()` is called and semaphore count is greater than zero, the system shall decrement the count and return success without queue registration.
2. When `twai_sem()` acquisition succeeds immediately, the system shall log `action=acquired` and the updated count.
3. If `timeout_ticks == 0`, then the system shall reject `twai_sem()` as invalid timeout and shall not treat it as polling.
4. When `twai_sem()` is called with zero semaphore count and valid timeout, the system shall move only the RUNNING current task to WAITING.
5. When `twai_sem()` moves a task to WAITING, the system shall set the wait reason to semaphore-timeout, preserve the waited semaphore ID, and preserve timeout ticks.
6. When timeout semaphore waiting begins, the system shall register the task in both the target semaphore wait queue and the delay queue.
7. When timeout semaphore waiting begins, the system shall log timeout wait transition, semaphore wait queue registration, delay queue registration, and `action=timed-wait`.
8. If either queue cannot accept the task before WAITING transition, then the system shall reject the call without leaving partial queue registration or an unintended WAITING task.

### Requirement 5: `sig_sem()` READY復帰とcount加算

**Objective:** 開発者がsemaphore返却時に待ちtaskのREADY復帰またはcount加算を観測できるようにする。

#### Acceptance Criteria

1. When `sig_sem()` is called and a semaphore wait queue has a normal semaphore waiter, the system shall move only that semaphore waiter from WAITING to READY.
2. When `sig_sem()` wakes a normal semaphore waiter, the system shall not increment semaphore count.
3. When `sig_sem()` is called and a semaphore wait queue has a timeout semaphore waiter, the system shall remove the waiter from the delay queue before or as part of READY restoration.
4. When `sig_sem()` wakes a timeout semaphore waiter, the system shall move the task from WAITING to READY without leaving stale delay queue membership.
5. When `sig_sem()` is called and no semaphore waiter exists, the system shall increment semaphore count within the semaphore maximum.
6. When `sig_sem()` processes each path, the system shall log wakeup, READY restoration, timeout waiter delay queue removal, count increment, and completed action.
7. When `sig_sem()` targets non-semaphore waiting states indirectly through queues or invalid metadata, the system shall reject the inconsistent wakeup without changing sleep, delay, READY, RUNNING, or DORMANT task state.

### Requirement 6: timeout到達と既存待ち経路の維持

**Objective:** timeout到達、sleep/delay、task生成起動、yield、timer IRQ経路が14.3変更後も壊れていないことを観測できるようにする。

#### Acceptance Criteria

1. When timeout reaches a timeout semaphore waiter, the system shall remove the task from the semaphore wait queue and move it to READY through the existing delay queue tick path.
2. While timer IRQ handler body runs, the system shall not call `wai_sem()`, `sig_sem()`, `pol_sem()`, `twai_sem()`, or `dispatcher_switch_to()` directly.
3. When `sig_sem()` wakes a higher-priority semaphore waiter than current, the system shall connect to the existing preemption pending path rather than directly switching from the API.
4. When `yield_tsk()` path runs after this feature, the system shall preserve the existing cooperative context switch behavior.
5. When `dly_tsk()` / delay queue / timer tick path runs after this feature, the system shall preserve delay timeout READY restoration behavior.
6. When `cre_tsk()` / `sta_tsk()` path runs after this feature, the system shall preserve task creation and task start behavior.
7. When `slp_tsk()` / `wup_tsk()` path runs after this feature, the system shall preserve sleep wait and sleep wakeup behavior.
8. When normal build, `make run`, and `make run VALIDATE_TIMER_IRQ_ENTRY=1` are executed, the system shall complete successfully and provide serial log evidence for the semaphore API paths.
9. When implementation is complete, README, Doxygen comments, serial log, and spec artifacts shall state the 14.3 semaphore API layer behavior.
10. When spec artifacts are finalized, the system shall keep `.kiro/specs/semaphore-api-layer/` to `requirements.md`, `design.md`, and `tasks.md` only.
