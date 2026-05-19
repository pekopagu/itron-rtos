# Implementation Plan

- [x] 1. timer IRQ handler を kernel timer tick に接続する
  - IRQ0/vector 32 handler が validation 到達ログの後に `timer_tick()` を1回呼ぶようにする。
  - `timer_tick()` 呼び出し後に IRQ0 EOI を送る順序を維持する。
  - handler 内ログは validation 専用観測として必要最小限に留め、EOI完了を確認できる最小ログを追加する。
  - handler から scheduler、dispatcher、context switch、preemption、task state 変更を呼ばない状態を維持する。
  - Observable completion: `make run VALIDATE_TIMER_IRQ_ENTRY=1` の serial log で handler 到達、tick 増加、EOI完了が順に観測できる。
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 2.1, 2.2, 3.1, 4.1, 4.2_
  - _Boundary: TimerIRQHandler, TimerAPI, ArchPIC_

- [x] 2. 8.1の未接続範囲をコメントとREADMEに反映する
  - timer IRQ handler、HAL/arch validation helper、timer public API の Doxygen コメントを8.1の「tick接続」段階に更新する。
  - `task_context_t` が将来の最小context switch用保存領域であり、今回の割り込み段階では実CPU register値を保存・復元しないことを明記する。
  - README に8.1の到達点、preemption未接続、割り込み中ログが通常ログへ混ざり得る制約、未実装範囲を反映する。
  - README の Zenn Articles 表に `v8.1-timer-tick-from-hardware-irq` の tag 候補を追加する。
  - Observable completion: README と Doxygen コメントから、8.1が tick 更新までで止まり scheduler/dispatcher/context switch/preemption に進まないことを確認できる。
  - _Requirements: 2.3, 3.2, 3.3, 3.4, 4.3_
  - _Boundary: DocumentationEvidence, TaskContextDocs_
  - _Depends: 1_

- [x] 3. build と QEMU validation 証跡を更新する
  - `make` で通常 build が成功することを確認する。
  - `make run` で既存 smoke flow が壊れず、通常 boot では timer IRQ handler 到達ログが出ないことを確認する。
  - `make run VALIDATE_TIMER_IRQ_ENTRY=1` で timer IRQ handler 到達、`timer_tick()` 呼び出しによる tick 更新、EOI完了を確認する。
  - `docs/logs/qemu-serial.log` を validation run の証跡で更新する。
  - `rg` で handler が scheduler、dispatcher、context switch、task state 変更を呼んでいないことと、kernel common が arch-local PIC/vector 詳細へ直接依存していないことを確認する。
  - spec ディレクトリを `requirements.md`、`design.md`、`tasks.md` の3ファイルだけにする。
  - Observable completion: build、通常 boot、validation boot、境界 grep、spec ファイル数の確認結果がすべて揃う。
  - _Requirements: 2.4, 4.1, 4.4_
  - _Boundary: Validation_
  - _Depends: 2_
