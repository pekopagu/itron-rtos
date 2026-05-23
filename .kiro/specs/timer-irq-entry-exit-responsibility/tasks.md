# Implementation Plan

- [x] 1. timer IRQ entry/exit責務境界をコード上で明示する
  - `arch/x86_64/interrupt_entry.asm` の timer IRQ stub コメントを8.4の interrupt entry責務へ更新する。
  - `arch/x86_64/interrupt.c` に interrupt exit boundary を表す小さな観測関数を追加し、dispatch pending を読むだけで消費しないことをDoxygenコメントで明記する。
  - `arch_timer_irq_handle()` の流れを `timer_tick()`、preemption decision、dispatch pending観測、exit boundary観測、EOI送信の順に保つ。
  - 完了状態として、validation logに `[timer-irq] exit boundary: dispatch-pending=... action=...` が出力され、handlerがdispatcher/context switch/task state変更を呼ばない。
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 2.1, 2.2, 2.3, 3.1, 3.2, 3.3, 3.4, 4.1, 4.2, 4.3_
  - _Boundary: TimerIRQEntryStub, TimerIRQHandler, TimerIRQExitBoundary_

- [x] 2. READMEと検証ログを8.4へ更新する
  - READMEに8.4の到達点として、interrupt entry、kernel IRQ handler、interrupt exit boundaryの責務分割を追記する。
  - READMEに、dispatch pending が requested でも8.4では消費せず、dispatcher/context switch/task state変更へ進まないことを明記する。
  - Zenn Articles表に `v8.4-timer-irq-entry-exit-responsibility` のtag候補を追加する。
  - `docs/logs/qemu-serial.log` を `make run VALIDATE_TIMER_IRQ_ENTRY=1` の8.4証跡で更新する。
  - 完了状態として、READMEとログから8.4が責務整理だけで実切替未接続であることを確認できる。
  - _Requirements: 4.4, 5.3, 5.4_
  - _Boundary: DocumentationEvidence_
  - _Depends: 1_

- [x] 3. build、smoke、validationとspec成果物を検証する
  - `make`、`make run`、`make run VALIDATE_TIMER_IRQ_ENTRY=1` を実行して結果を確認する。
  - source reviewで timer IRQ handlerが dispatcher、context switch、task state変更を呼んでいないことを確認する。
  - `.kiro/specs/timer-irq-entry-exit-responsibility/` を `requirements.md`、`design.md`、`tasks.md` の3ファイルだけにする。
  - 完了状態として、全タスクが完了し、検証結果と8.4の非接続範囲を報告できる。
  - _Requirements: 1.5, 5.1, 5.2, 5.5_
  - _Boundary: ValidationEvidence_
  - _Depends: 1, 2_

## Implementation Notes
