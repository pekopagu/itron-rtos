# Implementation Plan

## 1. sleep wait reason と task 状態遷移

- [x] 1.1 task wait reason に sleep を追加し、RUNNING->WAITING(sleep) helperを実装する
  - `_Boundary:_ TaskModule`
  - `TASK_WAIT_REASON_SLEEP` がdump/logで `sleep` と表示される。
  - `task_mark_waiting_on_sleep()` はRUNNING taskだけをWAITINGへ遷移させ、wait metadataをsleep用に揃える。
  - Requirements: 1.3, 1.4, 2.1

- [x] 1.2 sleep待ちtaskをREADYへ戻すhelperを実装する
  - `_Boundary:_ TaskModule`
  - `task_wake_waiting_on_sleep_by_id()` はWAITING(sleep)だけをREADYへ戻し、semaphore/delay/timeout待ちは変更しない。
  - READY復帰ログでWAITING(sleep)->READYを確認できる。
  - Requirements: 3.3, 3.4, 4.1, 4.2, 4.3, 4.4

## 2. slp_tsk / wup_tsk API

- [x] 2.1 `slp_tsk()` の宣言、戻り値、実装を追加する
  - `_Boundary:_ ItronApi`
  - API headerにDoxygen付き宣言があり、current RUNNING taskだけをsleep待ちへ落とす。
  - sleep後に既存scheduler/dispatcher境界で次READY taskへ進むログを確認できる。
  - Requirements: 1.1, 1.2, 1.5, 2.2, 2.3, 2.4

- [x] 2.2 `wup_tsk(tskid)` の宣言、戻り値、実装を追加する
  - `_Boundary:_ ItronApi`
  - WAITING(sleep)だけをREADYへ戻し、sleep待ち以外は状態変更なしでinvalid-stateを返す。
  - READY候補化ログとinvalid-stateログを確認できる。
  - `_Depends:_ 1.2`
  - Requirements: 3.1, 3.2, 3.5, 4.5

## 3. wakeup preemption pending 接続

- [x] 3.1 wakeup由来のdispatch pending reason/APIを追加する
  - `_Boundary:_ DispatchPending`
  - `wup_tsk()` で高優先度taskがREADY化された場合、`reason=task-wakeup` のpendingが記録される。
  - timer IRQ handler本体からは `slp_tsk()` / `wup_tsk()` / `dispatcher_switch_to()` を直接呼ばない。
  - `_Depends:_ 2.2`
  - Requirements: 2.5, 5.1, 5.2, 5.3, 5.6

## 4. smoke と成果物更新

- [x] 4.1 14.2 smokeシナリオとログ観測点を追加する
  - `_Boundary:_ KernelSmoke`
  - `make run` でsleep entry、wakeup success、invalid wakeup failure、READY候補化、高優先度wakeup pendingを確認できる。
  - 既存yield/semaphore/delay/timer/cre-sta smokeの観測点を維持する。
  - `_Depends:_ 3.1`
  - Requirements: 5.4, 5.5, 5.7

- [x] 4.2 README、Doxygen、serial log、specディレクトリを14.2到達点へ更新する
  - `_Boundary:_ DocumentationAndValidation`
  - READMEに14.2と `v14.2-sleep-wakeup-task-api` 候補が記載される。
  - `docs/logs/qemu-serial.log` が更新され、specディレクトリは `requirements.md`, `design.md`, `tasks.md` の3ファイルだけになる。
  - `_Depends:_ 4.1`
  - Requirements: 5.8

## Implementation Notes
