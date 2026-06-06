# Implementation Plan

## 1. timeout semaphore wait の状態表現を追加する

- [x] 1.1 wait reason と task 状態遷移 helper を追加する
  - `TASK_WAIT_REASON_SEMAPHORE_TIMEOUT` を追加し、task dump と各 reason 表示で `semaphore-timeout` として観測できるようにする。
  - `task_mark_waiting_on_sem_timeout(task_id, sem_id, timeout_ticks)` が RUNNING task だけを WAITING 化し、`wait_sem_id` と `delay_ticks_remaining` を保持する。
  - timeout semaphore waiting ログに task id/name/sem id/timeout/wait_reason/state が出る。
  - _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5_
  - _Boundary: TaskState_

## 2. queue 側で timeout semaphore waiter を受け入れる

- [x] 2.1 semaphore wait queue の事前確認と timeout waiter 受け入れを追加する
  - `sem_can_enqueue_waiter()` が semaphore ID、task ID、queue capacity を WAITING 化前に検証できる。
  - `sem_enqueue_waiter()` が通常 semaphore waiter と timeout semaphore waiter を受け入れ、delay waiter は拒否する。
  - queue 満杯時は `[sem-wq] enqueue failed: reason=full ...` を出せる。
  - _Requirements: 4.1, 4.4, 5.1, 5.3, 5.4_
  - _Boundary: SemaphoreWaitQueue_

- [x] 2.2 delay queue が timeout semaphore waiter を観測できるようにする
  - `delay_queue_enqueue()` が timeout semaphore waiter を delay queue に登録できる。
  - `delay_queue_dump()` が reason `semaphore-timeout`、remaining、state を表示する。
  - delay queue は `wait_sem_id` を metadata として使わない。
  - _Requirements: 4.2, 4.3, 4.5, 5.2, 5.3, 5.4_
  - _Boundary: DelayQueue_

## 3. `twai_sem()` API を実装する

- [x] 3.1 API 宣言と戻り値を追加する
  - `kernel/include/itron_api.h` に `twai_sem()` と戻り値定義を追加する。
  - `timeout_ticks == 0` は poll 相当ではなく invalid timeout として Doxygen に明記する。
  - _Requirements: 1.1, 1.3, 6.5_
  - _Boundary: TwaiSemAPI_

- [x] 3.2 immediate acquisition 経路を追加する
  - semaphore count > 0 の場合に count を減らし、queue 登録も switch も行わない。
  - 成功ログに `action=acquired` と count を出す。
  - _Requirements: 1.2, 2.1, 2.2, 2.3_
  - _Boundary: TwaiSemAPI_
  - _Depends: 3.1_

- [x] 3.3 timeout wait queueing と switch 経路を追加する
  - count == 0 かつ timeout_ticks > 0 の場合、両 queue precheck 後に WAITING 化し、semaphore wait queue と delay queue の両方へ登録する。
  - precheck 失敗時は task 状態や queue を変更せず、失敗 action を出す。
  - WAITING 化後は scheduler で次 READY task を選び、既存 dispatcher switch 境界へ進む。
  - _Requirements: 1.4, 1.5, 3.1, 4.1, 4.2, 5.1, 5.2, 5.3, 5.4, 6.2_
  - _Boundary: TwaiSemAPI_
  - _Depends: 1.1, 2.1, 2.2, 3.1_

## 4. boot-time smoke とドキュメント証跡を更新する

- [x] 4.1 kernel smoke に 13.3 の観測経路を追加する
  - immediate acquisition と timeout wait の両方を `make run` で観測できる。
  - 既存 `dly_tsk()`、`wai_sem()`、`sig_sem()`、`yield_tsk()` smoke を維持する。
  - _Requirements: 2.1, 2.2, 3.5, 4.1, 4.2, 4.3, 6.2_
  - _Boundary: RuntimeSmoke_
  - _Depends: 3.3_

- [x] 4.2 README、Doxygen、serial log、spec artifact を更新して検証する
  - README に 13.3 到達点、`v13.3-twai-sem-timeout-wait` tag 候補、未実装範囲を追記する。
  - `docs/logs/qemu-serial.log` を fresh `make run` 出力で更新する。
  - `make`、`make run`、`make run VALIDATE_TIMER_IRQ_ENTRY=1`、timer IRQ handler direct-call grep、spec ディレクトリ 3 ファイル確認が通る。
  - _Requirements: 6.1, 6.2, 6.3, 6.4, 6.5_
  - _Boundary: DocumentationEvidence_
  - _Depends: 4.1_

## Implementation Notes

- 13.3 では timeout remaining tick は観測用であり、tick decrement と timeout READY 復帰は行わない。
- timeout semaphore waiter は `sig_sem()` の wakeup 対象として semaphore wait queue に残り、timeout 観測対象として delay queue にも残る。13.3 では片方の完了で他方から削除する処理はまだ実装しない。
