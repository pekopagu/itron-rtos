# Implementation Plan

- [x] 1. API契約とmodule境界を14.3向けに整える
- [x] 1.1 semaphore API宣言と戻り値を整理する
  - `wai_sem` / `sig_sem` / `pol_sem` / `twai_sem` がAPIヘッダから一貫して参照できる状態にする。
  - `pol_sem` の成功、would-block、invalid current、invalid semaphoreを区別できる戻り値を用意する。
  - Doxygenコメントで14.3の対象API、非対象API、timer IRQ handlerから呼ばない制約が読める状態にする。
  - _Requirements: 1.1, 1.5, 3.3, 6.2, 6.9_
  - _Boundary: ITRON API Header_

- [x] 1.2 semaphore moduleをcountとwait queue管理へ限定する
  - `sig_sem` のAPI層実装責務をsemaphore moduleから外し、semaphore moduleはcount増加、取得、wait queue enqueue/dequeue/removeだけを提供する。
  - 待ちtaskがいない場合のcount加算とmax_count超過拒否をmodule helperで観測できる状態にする。
  - semaphore module内でtask READY化やdispatcher switchを行わない状態にする。
  - _Requirements: 1.4, 2.1, 3.1, 4.1, 4.6, 5.2, 5.5_
  - _Boundary: Semaphore Module_

- [x] 1.3 timeout waiterのdelay queue削除境界を追加する
  - timeout付きsemaphore待ちtaskをtask id指定でdelay queueから削除できるようにする。
  - 削除対象がsemaphore-timeout待ちでない場合はqueueを変更せず拒否できるようにする。
  - 削除成功時にtask id、name、reason、queue countをログで確認できる状態にする。
  - _Requirements: 4.6, 5.3, 5.4, 5.7, 6.1_
  - _Boundary: Delay Queue_

- [x] 2. semaphore API層の挙動を実装する
- [x] 2.1 `wai_sem()` の取得成功とWAITING遷移を14.3ログへ整理する
  - countありではcountを減らし、WAITING化せず `action=acquired` を出す。
  - countなしではRUNNING currentだけをWAITING(reason=semaphore)へ遷移させ、semaphore wait queueへ登録する。
  - WAITING化後のtaskがscheduler READY候補から外れ、次READY選択またはno-readyログが観測できる状態にする。
  - _Requirements: 1.2, 1.3, 1.4, 2.1, 2.2, 2.3, 2.4, 2.5, 2.6_
  - _Boundary: ITRON API Layer_
  - _Depends: 1.1, 1.2_

- [x] 2.2 `pol_sem()` の非ブロッキング取得を追加する
  - countありではcountを減らし、WAITING化せず `action=acquired` を出す。
  - countなしでは即時エラーを返し、task状態、wait metadata、semaphore wait queue、delay queueを変更しない。
  - invalid semidやinvalid current/stateをログで確認できる状態にする。
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 3.1, 3.2, 3.3, 3.4, 3.5_
  - _Boundary: ITRON API Layer_
  - _Depends: 1.1, 1.2_

- [x] 2.3 `twai_sem()` のtimeout付き待ちを14.3整合へ整理する
  - countありでは即時取得し、queue登録やWAITING化を行わない。
  - timeout 0はpoll扱いせずinvalid timeoutとして拒否する。
  - countなしではWAITING(reason=semaphore-timeout)へ遷移し、semaphore wait queueとdelay queueの両方へ登録する。
  - queue precheck失敗時にpartial registrationや不整合なWAITING taskを残さない状態にする。
  - _Requirements: 1.2, 1.3, 1.4, 4.1, 4.2, 4.3, 4.4, 4.5, 4.6, 4.7, 4.8_
  - _Boundary: ITRON API Layer_
  - _Depends: 1.1, 1.2, 1.3_

- [x] 2.4 `sig_sem()` をAPI層へ移しREADY復帰とpreemption pendingへ接続する
  - normal semaphore waiterをREADYへ戻し、countを増やさない。
  - timeout semaphore waiterを起こす場合はdelay queue登録を削除してからREADYへ戻す。
  - waiterなしではcountを増やし、max_count超過はエラーとして観測できる状態にする。
  - READY化したtaskがcurrentより高優先度なら既存preemption pendingへつなげ、APIから直接switchしない。
  - _Requirements: 1.2, 1.4, 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 5.7, 6.3_
  - _Boundary: ITRON API Layer, Dispatch Pending_
  - _Depends: 1.1, 1.2, 1.3_

- [x] 3. smoke観測とドキュメントを14.3へ更新する
- [x] 3.1 boot-time smokeでsemaphore API層の主要経路を観測する
  - `wai_sem`、`pol_sem`、`twai_sem`、`sig_sem` の成功・失敗・待ち・READY復帰・count加算をserial logで確認できるようにする。
  - timeout到達によるsemaphore wait queue削除とREADY復帰を既存delay queue tick経路で確認できるようにする。
  - invalid semid、invalid current/state、sleep/delay/DORMANT/READY/RUNNINGを不正に変更しない経路を確認できるようにする。
  - _Requirements: 1.2, 1.3, 1.4, 2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 3.1, 3.2, 3.3, 3.4, 3.5, 4.1, 4.2, 4.3, 4.4, 4.5, 4.6, 4.7, 5.1, 5.3, 5.5, 5.6, 5.7, 6.1, 6.8_
  - _Boundary: Smoke Documentation_
  - _Depends: 2.1, 2.2, 2.3, 2.4_

- [x] 3.2 README、Doxygen、serial log、spec成果物を更新する
  - READMEに14.3到達点と必要なtag候補 `v14.3-semaphore-api-layer` を記載する。
  - `docs/logs/qemu-serial.log` を `make run` の結果で更新する。
  - `.kiro/specs/semaphore-api-layer/` を最終的に `requirements.md`、`design.md`、`tasks.md` の3ファイルだけにする。
  - _Requirements: 1.5, 6.8, 6.9, 6.10_
  - _Boundary: Smoke Documentation_
  - _Depends: 3.1_

- [x] 4. 最終検証を実行する
- [x] 4.1 build、runtime、timer IRQ validationを通す
  - `make`、`make run`、`make run VALIDATE_TIMER_IRQ_ENTRY=1` が成功する。
  - timer IRQ handler本体から `wai_sem` / `sig_sem` / `pol_sem` / `twai_sem` / `dispatcher_switch_to` を直接呼んでいないことを確認する。
  - `yield_tsk`、`dly_tsk` / delay queue tick、`cre_tsk` / `sta_tsk`、`slp_tsk` / `wup_tsk` の既存観測経路が残っていることを確認する。
  - _Requirements: 6.2, 6.4, 6.5, 6.6, 6.7, 6.8, 6.10_
  - _Boundary: Smoke Documentation_
  - _Depends: 3.2_

## Implementation Notes
