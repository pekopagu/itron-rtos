# Implementation Plan

- [x] 1. 既存boot/arch/kernel/HAL境界を確認する
  - 既存boot path、HAL console、kernel smoke flow、arch/x86_64配置を確認する。
  - `kernel -> HAL -> arch(x86_64)` の依存方向を7.1でも維持する前提を整理する。
  - 完了時には、interrupt foundationを `hal_console_init()` 後、既存smoke flow前に統合する理由を説明できる。
  - _Requirements: 1.4, 3.1, 3.2, 3.4, 5.5_
  - _Boundary: KernelIntegration, HALInterruptAPI_

- [x] 2. x86_64 IDT/GDT前提を設計上確定する
  - 既存long mode bootで用意されるGDT/segment前提を確認する。
  - 7.1ではGDT再構築、TSS/IST、`sti`、PIC/APIC初期化を扱わない方針を確定する。
  - 完了時には、IDT gate selectorが既存kernel code selector前提に限られることを説明できる。
  - _Requirements: 4.1, 4.2, 4.3, 5.1, 5.2_
  - _Boundary: ArchInterruptFoundation_

- [x] 3. `kernel/include/hal/interrupt.h` を追加する
  - kernel共通層向けのHAL interrupt APIを追加する。
  - 通常初期化APIと検証build用APIを公開し、IDT/GDT/lidt/entry stubの詳細を公開しない。
  - 完了時には、kernel共通層がHAL interrupt APIだけをincludeできる。
  - _Requirements: 1.1, 2.3, 3.1, 3.2, 3.4_
  - _Boundary: HALInterruptAPI_

- [x] 4. `arch/x86_64/hal_interrupt.c` を追加し、HAL APIからarch-local APIへ委譲する
  - HAL interrupt APIのx86_64実装を追加する。
  - `hal_interrupt_init()` からarch-local初期化へ委譲し、検証用APIもarch-local検証処理へ委譲する。
  - 完了時には、HAL adapterがIDT tableやentry stubを直接所有せず、委譲だけを担当している。
  - _Requirements: 2.3, 3.2, 3.3_
  - _Boundary: X86HALInterruptAdapter_

- [x] 5. `arch/x86_64/interrupt.h` をarch-local公開APIとして維持する
  - x86_64固有のinterrupt/exception foundation APIをarch-local headerとして維持する。
  - kernel共通層から直接includeしない前提をコメントと設計で明確にする。
  - 完了時には、arch-local APIの利用者がx86_64 HAL adapterに限定される。
  - _Requirements: 3.1, 3.3, 3.4, 6.1_
  - _Boundary: ArchInterruptFoundation, X86HALInterruptAdapter_

- [x] 6. `arch/x86_64/interrupt.c` にIDT entry、IDTR、`lidt`、例外handler登録を実装する
  - IDT entry、IDTR、IDT table、IDT load helperをarch層に実装する。
  - 代表的なCPU例外handlerを登録し、初期化開始、IDT初期化完了、IDT load完了をHAL consoleへ出力する。
  - 完了時には、通常bootのQEMU serial logでIDT初期化完了とIDT load完了を観測できる。
  - _Requirements: 1.1, 1.2, 1.3, 4.1, 4.2, 4.3, 5.1, 5.2_
  - _Boundary: ArchInterruptFoundation_

- [x] 7. `arch/x86_64/interrupt_entry.asm` に例外entry stubを実装する
  - error codeあり/なしのCPU例外entryをC handlerへ渡せる形に整える。
  - vector、error code、RIP、CS、RFLAGSを観測用frameとして渡す。
  - 完了時には、登録済みCPU例外発生時にC側の観測handlerへ到達できる。
  - _Requirements: 2.1, 2.2, 5.3, 5.4_
  - _Boundary: ExceptionEntryStubs_

- [x] 8. `kernel/kernel.c` は `hal/interrupt.h` のみincludeし、`hal_interrupt_init()` を呼ぶ
  - kernel共通層から `arch/x86_64/interrupt.h` の直接includeをなくす。
  - `hal_console_init()` 後、既存smoke flow前に `hal_interrupt_init()` を呼ぶ。
  - 検証buildでのみ `hal_interrupt_trigger_validation_exception()` を呼ぶ。
  - 完了時には、kernel共通層が `arch_interrupt_*` を直接呼ばない。
  - _Requirements: 1.3, 1.4, 2.3, 3.1, 3.2, 3.4, 5.5_
  - _Boundary: KernelIntegration, HALInterruptAPI_
  - _Depends: 3, 4_

- [x] 9. Makefileへ `hal_interrupt.o`、`interrupt.o`、`interrupt_entry.o` を追加する
  - x86_64 HAL interrupt、arch interrupt C実装、ASM entry stubをbuild対象へ追加する。
  - `VALIDATE_EXCEPTION=1` の検証buildを通常buildと分離する。
  - 完了時には、通常buildと検証buildの両方で新規objectがlinkされる。
  - _Requirements: 1.1, 1.2, 2.3, 3.3_
  - _Boundary: BuildAndDocumentationIntegration_
  - _Depends: 3, 4, 6, 7, 8_

- [x] 10. `make all` でbuild検証する
  - 新規HAL interrupt object、arch interrupt object、ASM entry objectを含めてbuildする。
  - link時に未解決symbolがないことを確認する。
  - 完了時には、`make all` が成功し、kernel imageが生成される。
  - _Requirements: 1.1, 3.3_
  - _Boundary: BuildAndDocumentationIntegration_
  - _Depends: 9_

- [x] 11. `make run` でIDT初期化と既存smoke flow継続を確認する
  - 通常bootでIDT初期化開始、IDT初期化完了、IDT load完了をQEMU serial logで確認する。
  - 検証例外を発生させず、既存Chapter 6.3までのsmoke flowが継続することを確認する。
  - 完了時には、例外受信基盤ログと既存smoke flowログを区別できる。
  - _Requirements: 1.1, 1.2, 1.4, 5.5_
  - _Boundary: KernelIntegration, ArchInterruptFoundation_
  - _Depends: 10_

- [x] 12. `make run VALIDATE_EXCEPTION=1` でhandler到達を確認する
  - 明示的な検証buildで検証用CPU例外を発生させる。
  - QEMU serial logでhandler到達、例外番号または例外名を確認する。
  - 完了時には、handler到達ログ後に停止する挙動を教育用観測handlerとして説明できる。
  - _Requirements: 2.1, 2.2, 2.3, 2.4, 6.2_
  - _Boundary: HALInterruptAPI, X86HALInterruptAdapter, ExceptionObservationHandler_
  - _Depends: 10_

- [x] 13. `rg` でkernel共通層のarch-local依存がないことを確認する
  - `kernel/kernel.c` が `arch_interrupt_*` を直接呼んでいないことを確認する。
  - `kernel/kernel.c` が `arch/x86_64/interrupt.h` に依存していないことを確認する。
  - `arch/x86_64/interrupt.c` がscheduler、dispatcher、context switch、preemption、task、semaphore、timerへ依存していないことを確認する。
  - 完了時には、HAL interrupt境界と非対象機能との分離をgrep結果で説明できる。
  - _Requirements: 3.1, 3.2, 3.4, 5.1, 5.2, 5.3, 5.4_
  - _Boundary: HALInterruptAPI, KernelIntegration, ArchInterruptFoundation, ExceptionObservationHandler_
  - _Depends: 8, 12_

- [x] 14. README/Zenn向けに7.1の範囲、HAL境界、非対象を整理する
  - 7.1の到達範囲をIDT初期化、IDT load、CPU例外handler到達観測として整理する。
  - `kernel -> HAL interrupt -> arch/x86_64 interrupt` の境界を説明する。
  - timer interrupt、IRQ routing、PIC/APIC、scheduler、dispatcher、context switch、preemption、復帰可能例外処理は非対象として明記する。
  - 完了時には、読者が7.1をtimer/preemption章と誤解しない説明になっている。
  - _Requirements: 4.1, 5.1, 5.2, 5.3, 5.4, 6.1, 6.2, 6.3_
  - _Boundary: Documentation_
  - _Depends: 11, 12, 13_

- [x] 15. Doxygenとコメントを日本語で更新する
  - HAL interrupt API、arch-local interrupt API、IDT/IDTR、例外entry stub、観測handlerにDoxygenまたは意図コメントを追加する。
  - 処理の意図、HAL境界、GDT前提、`lidt` がtimer/preemption開始ではないことを説明する。
  - 復帰可能例外処理ではなく教育用観測handlerであることを明記する。
  - 完了時には、主要な非自明処理の意図と非対象がソースコメントから追跡できる。
  - _Requirements: 4.1, 4.2, 4.3, 5.1, 5.3, 5.4, 6.1, 6.2, 6.3_
  - _Boundary: HALInterruptAPI, X86HALInterruptAdapter, ArchInterruptFoundation, ExceptionEntryStubs, ExceptionObservationHandler, Documentation_
  - _Depends: 14_
