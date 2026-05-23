# Implementation Plan

- [x] 1. dispatcher層にswitch boundary APIを追加する
  - `dispatcher.h` に `dispatcher_switch_to(tcb_t *from, tcb_t *to)` を追加し、Doxygenコメントで責務と非ゴールを明記する。
  - `dispatcher.c` に入力検証、begin/endログ、`task_context_switch_to_task_pair()` への委譲を追加する。
  - 完了状態として、valid pairではdispatcher boundaryログが出力され、invalid pairでは理由がログに残る。
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 3.2, 3.3, 3.5_
  - _Boundary: DispatcherSwitchBoundary_

- [x] 2. kernel smoke coordinatorをdispatcher境界経由へ変更する
  - `kernel_run_minimal_context_switch_smoke()` の切替開始点を `task_context_switch_to_task_pair()` 直接呼び出しから `dispatcher_switch_to()` へ変更する。
  - 9.1のboot -> first task -> second task -> bootの観測flowを維持する。
  - 完了状態として、上位層から切替開始点がdispatcher境界として見え、既存のtask-to-task smokeログも維持される。
  - _Requirements: 2.1, 2.2, 2.3, 2.4_
  - _Boundary: KernelContextSmokeCoordinator_
  - _Depends: 1_

- [x] 3. README、task_contextコメント、spec証跡を9.2へ更新する
  - READMEに9.2の目的、責務境界、非ゴール、期待ログを追加する。
  - Zenn Articles表とDevelopment Progress表へ `v9.2-dispatcher-switch-boundary` を追加する。
  - `task_context_switch_to_task_pair()` がsmoke補助APIであり、正式dispatcher境界ではないことをコメント上でも明記する。
  - 完了状態として、README/spec/commentから9.2が境界作成のみであることを確認できる。
  - _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5, 4.5_
  - _Boundary: DocumentationEvidence_
  - _Depends: 1, 2_

- [x] 4. build、通常boot、timer IRQ validationを検証しログを更新する
  - `make`、`make run`、`make run VALIDATE_TIMER_IRQ_ENTRY=1` を実行する。
  - `docs/logs/qemu-serial.log` を9.2の通常boot証跡で更新する。
  - `.kiro/specs/dispatcher-switch-boundary/` が3ファイルだけであることを確認する。
  - 完了状態として、dispatcher boundary経由のsmoke flowとtimer IRQ path非接続を報告できる。
  - _Requirements: 4.1, 4.2, 4.3, 4.4_
  - _Boundary: ValidationEvidence_
  - _Depends: 1, 2, 3_
