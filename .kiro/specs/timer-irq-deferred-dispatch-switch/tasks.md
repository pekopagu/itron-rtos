# Implementation Plan

- [x] 1. dispatch pending consume API と状態保持を 11.2 に更新する
  - dispatch pending state に from/to task id と reason を保持し、後段で再取得できるようにする。
  - no-pending、invalid-from-or-to、valid dispatch attempt をログで区別する consume API を追加する。
  - 完了状態として、pending request 後に consume すると `[dispatch-pending] consumed: ...` と clear log が出る。
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 2.1, 2.5_
  - _Boundary: DispatchPendingState, DispatchPendingConsumeAPI_

- [x] 2. interrupt exit boundary から deferred dispatch へ接続する
  - timer IRQ exit boundary が requested/no-pending を分類し、requested の場合だけ deferred dispatch API を呼ぶ。
  - timer IRQ handler 本体から `yield_tsk()` と `dispatcher_switch_to()` を直接呼ばない状態を維持する。
  - 完了状態として、validation run で `action=deferred-dispatch` または `action=no-dispatch` が出る。
  - _Requirements: 2.2, 2.3, 2.4, 3.1, 3.2, 3.3, 3.4_
  - _Boundary: TimerIRQExitBoundary_
  - _Depends: 1_

- [x] 3. 既存協調 switch と高優先度 READY 検出ログを維持して統合する
  - 11.1 の `[preempt-irq] current`、`higher-ready detected`、`decision evaluated`、pending requested log を維持する。
  - 同一優先度 READY は引き続き time slice 対象外とし、pending request しない。
  - 10.4 の `yield_tsk()` 協調 context switch 経路と 9.1-9.4 smoke を壊さない。
  - 完了状態として、通常 run と timer IRQ validation run の両方で期待ログが観測できる。
  - _Requirements: 3.5, 4.1, 4.2, 4.3, 4.4, 4.5_
  - _Boundary: PreemptionIRQAPI, DispatcherSwitchBoundary, YieldAPI_
  - _Depends: 1, 2_

- [x] 4. README、Doxygen、serial log、spec 成果物を更新して検証する
  - README に 11.2 の到達点、tag 候補、未実装範囲を追記する。
  - Doxygen コメントに deferred dispatch consume が教育用境界であり、完全な割り込み復帰 frame 切替ではないことを明記する。
  - `make`、`make run`、`make run VALIDATE_TIMER_IRQ_ENTRY=1` を実行し、`docs/logs/qemu-serial.log` を更新する。
  - 最終状態で `.kiro/specs/timer-irq-deferred-dispatch-switch/` を `requirements.md`、`design.md`、`tasks.md` の3ファイルだけにする。
  - _Requirements: 5.1, 5.2, 5.3, 5.4, 5.5_
  - _Boundary: DocumentationEvidence, SpecArtifacts_
  - _Depends: 1, 2, 3_
