# Implementation Plan

- [x] 1. x86_64 PIC EOI boundaryを追加する
  - `arch/x86_64/pic.h` にIRQ EOI用のarch-local APIを追加する。
  - `arch/x86_64/pic.c` にmaster/slave PICへの必要最小限のEOI送信を実装する。
  - 範囲外IRQは無視し、mask mirrorを変更しない。
  - Doxygenで目的、前提、制限、APIC系への将来置換を説明する。
  - Observable completion: valid IRQに対してEOIを送れるAPIがbuild対象になり、invalid IRQで副作用を起こさない実装になっている。
  - _Requirements: 3.1, 3.2, 3.3, 3.4_
  - _Boundary: x86_64 PIC_

- [x] 2. IDT vector 32とtimer IRQ entry stubを追加する
  - `arch/x86_64/interrupt_entry.asm` にvector 32用timer IRQ entry stubを追加する。
  - `arch/x86_64/interrupt.c` でIDT vector 32へstubを登録する。
  - C側timer interrupt handlerを追加し、到達時に最小限のserial logを出してからIRQ0 EOIを送る。
  - handler内で `timer_tick()`、scheduler、dispatcher、context switch、task state変更を呼ばない。
  - Observable completion: vector 32 gateが初期化時に登録され、timer IRQ handlerへの到達経路がlink可能になる。
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 2.1, 2.2, 2.3, 2.4, 3.1, 5.1, 5.4_
  - _Boundary: x86_64 interrupt entry_
  - _Depends: 1_

- [x] 3. 明示validation用HAL入口とbuild flagを追加する
  - `arch/x86_64/interrupt.h` と `arch/x86_64/hal_interrupt.c` にtimer entry validation開始用の委譲を追加する。
  - `kernel/include/hal/interrupt.h` にkernel-facing validation APIを追加する。
  - `kernel/kernel.c` で `ARCH_TIMER_IRQ_ENTRY_VALIDATE` 有効時だけvalidation APIを呼ぶ。
  - `Makefile` に `VALIDATE_TIMER_IRQ_ENTRY=1` を追加し、通常buildでは無効にする。
  - Observable completion: 通常bootではIRQ0 unmaskを呼ばず、validation buildだけIRQ0 unmaskとinterrupt enableを開始する。
  - _Requirements: 4.1, 4.2, 4.3, 4.4, 5.2, 5.3, 6.1, 6.2, 6.3_
  - _Boundary: HAL interrupt, kernel boot integration, build_
  - _Depends: 2_

- [x] 4. READMEとコメントを7.3の範囲に合わせて更新する
  - READMEに7.3の目的、検証方法、通常bootとvalidation bootの違いを追記する。
  - PIT programming、timer_tick接続、preemption、scheduler/dispatcher/context switchが非対象であることを明記する。
  - handler logが7.4前の最小観測であり、本格interrupt-safe loggingではないことを明記する。
  - Observable completion: 読者が7.3を「timer IRQ入口到達構造」と理解でき、timer subsystem完成と誤解しない。
  - _Requirements: 1.4, 2.4, 4.4, 6.4_
  - _Boundary: Documentation_
  - _Depends: 3_

- [x] 5. build、QEMU smoke、境界検証を実行する
  - `make` で通常buildが成功することを確認する。
  - `make run` で既存smoke logが継続し、timer IRQ arrival logが通常bootに出ないことを確認する。
  - `make run VALIDATE_TIMER_IRQ_ENTRY=1` でtimer IRQ handler到達logを確認する。
  - `rg` でkernel commonがarch-local PIC/interrupt headerへ直接依存していないこと、handlerがtimer/scheduler/dispatcher/context switchへ接続していないことを確認する。
  - Observable completion: buildとserial logにより、7.3がentry到達構造だけを追加したことを検証できる。
  - _Requirements: 1.3, 2.2, 2.3, 3.1, 4.1, 4.2, 4.3, 4.4, 5.1, 5.2, 5.4, 6.1, 6.2, 6.3, 6.4_
  - _Boundary: Validation_
  - _Depends: 4_

## Implementation Notes

- `make` と `make run` は成功し、通常bootでは `[timer-irq] entry reached` が出ず既存smoke flowが継続することを確認した。
- `make run VALIDATE_TIMER_IRQ_ENTRY=1` は成功し、`[timer-irq] entry reached: vector=32 irq=0` でvector 32 handler到達を確認した。
- 境界検証ではkernel commonからarch-local PIC/interrupt headerへの直接依存はなく、timer IRQ handlerからtimer/scheduler/dispatcher/context switch/task state変更への実呼び出しもないことを確認した。
