# Implementation Plan

- [x] 1. dispatch pending requestログの重複抑止を実装する
  - pending requestに「requestedログ出力済み」状態を持たせ、同一pendingを複数回観測してもrequestedログを一度だけにする。
  - request/consume/clear/not-requested/consume skippedのreason表記を `higher-priority-ready`、`same-priority-not-timeslice-target`、`dispatch-completed`、`no-pending` に固定する。
  - 完了状態として、高優先度READY時のrequestedログがconsume前に一度だけ出る。
  - _Requirements: 2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 3.1, 3.3_
  - _Boundary: DispatchPendingAPI_

- [x] 2. preemption decisionとtimer IRQ exit boundaryのログ順序を11.4として固定する
  - current、higher-readyまたはno higher-ready、decision evaluated、dispatch pending、exit boundary、consume、dispatcher、clear、EOIの順序を維持する。
  - 同一優先度READY時は `same-priority-not-timeslice-target` のままpending requestしない。
  - timer IRQ handler本体が `yield_tsk()` と `dispatcher_switch_to()` を直接呼ばないことを確認する。
  - 完了状態として、期待ログ例と同じ順序で高優先度READYと同一優先度READYのログが観測できる。
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 3.1, 3.2, 3.5_
  - _Boundary: PreemptionIRQAPI, TimerIRQExitBoundary_
  - _Depends: 1_

- [x] 3. README、Doxygen、serial log、spec成果物を11.4として更新する
  - READMEに `v11.4-preemption-event-log-stabilization` と11.4到達点、未実装範囲を追記する。
  - 更新コードのDoxygenコメントに、ログ安定化の目的、制約、非目標を日本語で記載する。
  - `docs/logs/qemu-serial.log` をfresh validation evidenceで更新する。
  - `.kiro/specs/preemption-event-log-stabilization/` を最終的に `requirements.md`、`design.md`、`tasks.md` の3ファイルだけにする。
  - 完了状態として、build/run/validation結果とspec成果物制約を確認できる。
  - _Requirements: 3.4, 4.1, 4.2, 4.3, 4.4, 4.5_
  - _Boundary: DocumentationEvidence, SpecArtifacts, ValidationEvidence_
  - _Depends: 1, 2_
