# Requirements Document

## Project Description

学習用 μITRON 風 RTOS を段階的に開発している開発者が、13.2 で導入した sleep/delay queue と既存 semaphore wait queue の分離方針を保ったまま、timeout 付きセマフォ待ちを観測したい。現状は `dly_tsk(delay_ticks)` による delay WAITING と `wai_sem()` による通常 semaphore WAITING を別々に扱えるが、`twai_sem(sem_id, timeout_ticks)` 風 API は存在しない。13.3 では、timeout 付き semaphore 待ち task を semaphore wait queue と delay queue の両方へ登録し、timeout 到達処理はまだ行わずに、待ち対象 semaphore と残 timeout tick をログで確認できるようにする。

## Boundary Context

- **In scope**: `twai_sem()` API 宣言と実装、timeout 付き semaphore 待ち理由の追加、`wait_sem_id` と `timeout_ticks_remaining` 相当の観測、semaphore wait queue と delay queue への二重登録、失敗時の不整合防止、README/Doxygen/spec/log 更新。
- **Out of scope**: tick ごとの timeout decrement、timeout 到達時 READY 復帰、timeout 時の semaphore wait queue 削除、`sig_sem()` 成功時の delay queue 削除、timeout エラーコード本格整理、`pol_sem`、`slp_tsk` / `wup_tsk`、priority 順 wait queue、delta queue、time slice、round-robin、timer IRQ handler からの task API 呼び出し。
- **Adjacent expectations**: 13.2 の delay queue 経路、12.3 の semaphore wait queue、12.4 の wakeup 後 preemption、10.4 の `yield_tsk()`、11.4 の timer IRQ preemption / dispatch pending ログ仕様を維持する。

## Requirements

### Requirement 1: `twai_sem()` API と入力検証

**Objective:** 学習用 RTOS 開発者として、timeout 付き semaphore 待ち API の入口を観測できるようにしたい。

#### Acceptance Criteria

1. When `twai_sem(sem_id, timeout_ticks)` is declared, the system shall expose it through the μITRON-like API header.
2. When `twai_sem()` is called, the system shall log semaphore ID, timeout tick value, current task ID, current task name, and current task state.
3. When `timeout_ticks == 0`, the system shall reject the call as invalid timeout and shall not treat it as polling.
4. When current task is absent or not RUNNING, the system shall reject the call without changing task state or queues.
5. When the semaphore ID is invalid, the system shall reject the call without changing task state or queues.

### Requirement 2: immediate acquisition path

**Objective:** timeout 待ち API でも semaphore count が残っている場合は既存 count 取得モデルを壊さず即時成功させたい。

#### Acceptance Criteria

1. When semaphore count is greater than zero, the system shall decrement the count and return success without switching tasks.
2. When immediate acquisition succeeds, the system shall not enqueue the current task into either semaphore wait queue or delay queue.
3. When immediate acquisition succeeds, the system shall log `action=acquired` and the updated count.

### Requirement 3: timeout 付き semaphore WAITING 化

**Objective:** semaphore count が 0 のとき、timeout 付き semaphore 待ちを通常 semaphore 待ちや delay 待ちと区別して観測したい。

#### Acceptance Criteria

1. When semaphore count is zero and `timeout_ticks > 0`, the system shall move only the RUNNING current task to WAITING.
2. When a task enters timeout semaphore waiting, the system shall set a wait reason distinct from normal semaphore waiting and delay waiting.
3. When a task enters timeout semaphore waiting, the system shall preserve `wait_sem_id` as the waited semaphore ID.
4. When a task enters timeout semaphore waiting, the system shall preserve the requested timeout ticks in a field observable by delay queue dump.
5. When timeout semaphore waiting is logged, the system shall expose task id, task name, wait semaphore ID, timeout tick value, wait reason, and task state.

### Requirement 4: semaphore wait queue と delay queue の両登録

**Objective:** timeout 付き semaphore 待ち task を `sig_sem()` と timeout 観測の両方の対象として管理したい。

#### Acceptance Criteria

1. When timeout semaphore waiting begins, the system shall enqueue the task into the target semaphore wait queue.
2. When timeout semaphore waiting begins, the system shall enqueue the same task into the delay queue.
3. When the delay queue is dumped, the system shall show the timeout semaphore waiter with task id, task name, remaining timeout ticks, wait reason, and task state.
4. When registering the task into both queues, the system shall keep semaphore wait queue ownership separate from delay queue ownership.
5. When registering into the delay queue, the system shall not reuse `wait_sem_id` as delay queue metadata.

### Requirement 5: failure consistency before WAITING

**Objective:** queue 登録に失敗する場合に、不整合な WAITING task や片側 queue 登録だけを残さないようにしたい。

#### Acceptance Criteria

1. When semaphore wait queue cannot accept the task, the system shall reject `twai_sem()` before changing the task to WAITING.
2. When delay queue cannot accept the task, the system shall reject `twai_sem()` before changing the task to WAITING.
3. When either precheck fails, the system shall log an action that identifies the failed queue.
4. When either precheck fails, the system shall not enqueue the task into either queue.

### Requirement 6: 既存経路と未実装範囲の維持

**Objective:** 13.3 の追加で既存の delay、semaphore、yield、timer IRQ 経路を壊さないようにしたい。

#### Acceptance Criteria

1. When normal build is executed, the system shall build successfully.
2. When normal `make run` is executed, the system shall show `twai_sem()` immediate acquisition and timeout wait queue evidence.
3. When `make run VALIDATE_TIMER_IRQ_ENTRY=1` is executed, the system shall preserve timer IRQ preemption / dispatch pending behavior.
4. While timer IRQ handler body runs, the system shall not call `twai_sem()`, `dly_tsk()`, `wai_sem()`, `sig_sem()`, `yield_tsk()`, or `dispatcher_switch_to()` directly.
5. When implementation is complete, README, Doxygen comments, serial log, and spec artifacts shall state that tick decrement, timeout READY return, timeout wait queue removal, and `sig_sem()` success-time delay queue removal remain unimplemented.
