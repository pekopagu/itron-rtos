# Implementation Plan

- [x] 1. dispatcher switch境界へRUNNING/READY状態遷移を追加する
  - `dispatcher_switch_to()`でfrom RUNNING、to READYの事前確認を維持し、状態変更前に失敗できるようにする。
  - from taskのRUNNING->READY、to taskのREADY->RUNNINGをdispatcher境界内で実行する。
  - 状態遷移ログとしてtask id/nameと遷移方向がQEMU serial logへ出る。
  - to taskをdispatcher current taskとして更新し、実切替補助APIへ進む。
  - 実装完了時、9.2のswitch boundary begin/endログと9.1のtask_b->task_c smokeが維持される。
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 2.1, 2.2, 3.1, 3.2_
  - _Boundary: DispatcherSwitchBoundary_

- [x] 2. task_context smoke補助APIからdispatcher状態責務を外す
  - `task_context_switch_to_task_pair()`は9.1 smoke補助APIとして残す。
  - task_context層ではdispatcher currentを更新しないことを維持する。
  - first entry return時にdispatcher境界で済ませたRUNNING/READY遷移を重複させず、entry return最終状態確定は9.4へ残す。
  - 実装完了時、task_context層はstack/register contextに近い処理とarch switch委譲だけを担当していることがsource reviewで確認できる。
  - _Requirements: 2.4, 3.3, 4.1, 4.2_
  - _Boundary: TaskContextSmokeHelper_
  - _Depends: 1_

- [x] 3. READMEとDoxygenコメントへ9.3の到達点を反映する
  - READMEに第9章9.3の概要、到達点、非対象を追加する。
  - Zenn Articles表とDevelopment Progress表に`v9.3-dispatcher-state-transition-switch`を追加する。
  - dispatcher/task_contextのDoxygenコメントに、9.3がRUNNING/READY遷移をdispatcher switch境界へ接続する段階であることを記載する。
  - 実装完了時、entry return最終扱い、dispatch pending消費、interrupt exit接続、timer IRQ実切替が未実装であることをREADMEとコメントから確認できる。
  - _Requirements: 3.4, 3.5, 4.1, 4.2, 4.3_
  - _Boundary: DocumentationEvidence_
  - _Depends: 1, 2_

- [x] 4. build/run/timer validationを実行し、検証証跡を更新する
  - `make`、`make run`、`make run VALIDATE_TIMER_IRQ_ENTRY=1`を実行する。
  - `docs/logs/qemu-serial.log`を9.3の通常run証跡で更新する。
  - source reviewでtimer IRQ handlerから`dispatcher_switch_to()`を呼んでいないこと、dispatch pendingを消費していないこと、arch/x86_64へscheduler/dispatcher内部が漏れていないことを確認する。
  - `.kiro/specs/dispatcher-state-transition-switch/`を`requirements.md`、`design.md`、`tasks.md`の3ファイルだけにする。
  - 実装完了時、通常build、通常run、timer IRQ validation runの結果を報告できる。
  - _Requirements: 4.4, 4.5, 5.1, 5.2, 5.3, 5.4, 5.5_
  - _Boundary: ValidationEvidence_
  - _Depends: 1, 2, 3_
