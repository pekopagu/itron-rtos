# Implementation Plan

- [x] 1. dispatch pending kernel boundary を追加する
  - `kernel/include/dispatch_pending.h` と `kernel/dispatch_pending.c` に最小限の requested/reason/candidate 状態を追加する。
  - `dispatch_request_from_irq()`、`dispatch_pending_is_requested()`、`dispatch_pending_clear_for_test_or_later_boundary()`、`dispatch_pending_log_state_from_irq()` を Doxygen コメント付きで提供する。
  - 完了状態として、dispatch pending state は kernel 側に閉じ、dispatcher/context switch/task state変更を呼ばずに build 対象へ入る。
  - _Requirements: 1.1, 1.4, 1.5, 1.6, 2.2, 2.3, 4.1, 5.1_
  - _Boundary: DispatchPendingAPI_

- [x] 2. preemption decision を dispatch pending request へ接続する
  - `preemption_evaluate_from_irq()` が scheduler decision の reason を解釈し、switch-target の場合だけ `dispatch_request_from_irq()` を呼ぶ。
  - no-switch、invalid-current、no-current、no-ready、candidate-not-higher では dispatch pending を set しない。
  - 完了状態として、`[preempt-irq]` log と dispatch pending request の責務が分かれ、task state/current/context は変更されない。
  - _Requirements: 1.2, 1.3, 3.2, 4.2_
  - _Boundary: PreemptionIRQAPI_
  - _Depends: 1_

- [x] 3. timer IRQ handler で dispatch pending を観測する
  - `arch_timer_irq_handle()` の流れを `timer_tick()`、preemption decision、dispatch pending observation、EOI に整理する。
  - arch 側は dispatch pending public API だけを呼び、scheduler/dispatcher 内部へ依存しない。
  - 完了状態として、validation run で dispatch pending observation が preemption decision 後かつ EOI 前に出力される。
  - _Requirements: 2.1, 3.1, 3.2, 3.3, 3.4, 5.3_
  - _Boundary: TimerIRQHandler_
  - _Depends: 1, 2_

- [x] 4. README と検証ログを 8.3 に更新する
  - README に8.3の到達点、dispatch pending は観測のみで実切替未接続であること、割り込み中ログの制約を明記する。
  - Zenn Articles 表に必要なら `v8.3-timer-irq-dispatch-pending-observation` を追加する。
  - `docs/logs/qemu-serial.log` を `make run VALIDATE_TIMER_IRQ_ENTRY=1` の結果で更新する。
  - 完了状態として、`make`、`make run`、`make run VALIDATE_TIMER_IRQ_ENTRY=1` の検証結果と spec 3ファイル構成を確認できる。
  - _Requirements: 2.4, 4.3, 4.4, 5.2, 5.4, 5.5_
  - _Boundary: DocumentationEvidence_
  - _Depends: 1, 2, 3_

## Implementation Notes

- validation build の既存 object が古い場合は `make clean VALIDATE_TIMER_IRQ_ENTRY=1` 後に再実行すると `ARCH_TIMER_IRQ_ENTRY_VALIDATE` 経路を新鮮に確認できる。
