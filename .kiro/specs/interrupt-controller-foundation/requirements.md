# Requirements Document

## Introduction

この仕様は、μITRON風RTOSの第7章 7.2「PICまたはAPICの初期化方針」として、x86_64 + QEMU 構成の学習用RTOSに割り込みコントローラ初期化基盤を追加する。現在は第7章7.1で IDT/GDT と最小例外ハンドラ基盤、serial log、cooperative task execution、timer foundation、semaphore foundation、preemption foundation が存在するが、hardware interrupt による task switch は未接続である。

今回の目的は、第7章7.3「タイマ割り込み入口を作る」へ進む前に、まず legacy PIC (8259A) の採用理由と初期化範囲を明確にし、IRQ0 を CPU exception vector と衝突しない vector 32 以降へ remap できる状態を作ることである。APIC / IOAPIC / LAPIC は将来拡張として整理するだけで、実装しない。

## Boundary Context

- **In scope**: legacy PIC 採用理由の整理、PIC 初期化完了の serial log 観測、IRQ0 を vector 32 以降へ移動する方針、初期状態で IRQ を mask する挙動、PIC mask/unmask API の提供、kernel_main からの PIC 初期化呼び出し、README/Zenn記事向けの設計整理。
- **Out of scope**: PIT timer interrupt 発火、timer ISR、`timer_tick()` の ISR 接続、scheduler 起動、dispatcher 呼び出し、preemption 判定、context switch、APIC / IOAPIC / LAPIC 実装、SMP、nested interrupt、μITRON API、既存RTOS実装コードの参照・コピー・流用。
- **Adjacent expectations**: 第7章7.1の IDT/GDT と例外ハンドラ基盤は維持される。既存の timer foundation / semaphore foundation / preemption foundation / cooperative task execution の boot-time smoke は、PIC 初期化後も hardware interrupt へ接続されず、従来どおり明示呼び出しで観測できる。

## Requirements

### Requirement 1: 割り込みコントローラ方針の観測

**Objective:** As a RTOS 学習者, I want 起動時に割り込みコントローラ初期化方針を serial log で確認できる, so that 第7章7.3へ進む前に PIC 採用と非採用範囲を区別できる

#### Acceptance Criteria

1. When kernel boot initializes the interrupt controller foundation, the RTOS shall emit a serial log line showing that PIC initialization has begun.
2. When PIC initialization completes, the RTOS shall emit a serial log line showing that PIC initialization completed with IRQs masked.
3. When normal QEMU smoke verification is executed, the RTOS shall show PIC initialization logs before the existing task, timer, semaphore, preemption-decision, context-switch, and cooperative verification logs.
4. The RTOS shall document that APIC, IOAPIC, and LAPIC are future extensions and are not implemented by this feature.

### Requirement 2: IRQ vector 衝突回避

**Objective:** As a RTOS 開発者, I want IRQ0 が CPU exception vector と衝突しない番号へ移動される, so that 次章で timer interrupt entry を追加しても例外 vector と IRQ vector を混同しない

#### Acceptance Criteria

1. When PIC initialization runs, the RTOS shall make IRQ0 map to vector 32 or later.
2. When PIC initialization runs, the RTOS shall keep CPU exception vectors 0 through 31 reserved for CPU exceptions.
3. The RTOS shall document the selected PIC vector base and explain that IRQ0 is prepared for a later timer interrupt entry.

### Requirement 3: IRQ mask 状態の明示

**Objective:** As a RTOS 開発者, I want PIC 初期化直後に IRQ が mask されていることを確認できる, so that timer ISR 未実装の段階で予期しない hardware interrupt が既存 smoke flow を乱さない

#### Acceptance Criteria

1. When PIC initialization completes, the RTOS shall leave all PIC IRQ lines masked by default.
2. When an IRQ line is unmasked through the PIC API, the RTOS shall update only the requested IRQ line's mask state.
3. When an IRQ line is masked through the PIC API, the RTOS shall update only the requested IRQ line's mask state.
4. If a caller requests an IRQ line outside the PIC IRQ range, then the RTOS shall ignore the request without changing existing PIC mask state.

### Requirement 4: arch と kernel の責務分離

**Objective:** As a RTOS 開発者, I want PIC の I/O port 操作が x86_64 arch 層に閉じている, so that kernel 共通層が割り込みコントローラ固有の port details を所有しない

#### Acceptance Criteria

1. The RTOS shall keep PIC I/O port operations inside the x86_64 arch boundary.
2. When kernel_main initializes the interrupt controller foundation, the RTOS shall call an architecture-facing initialization boundary instead of issuing PIC port operations directly.
3. The RTOS shall provide documented PIC initialization and mask/unmask interfaces for the x86_64 arch boundary.
4. The RTOS shall keep existing HAL console and HAL interrupt responsibilities unchanged unless they are explicitly needed for PIC initialization observation.

### Requirement 5: 非対象範囲の保全

**Objective:** As a RTOS 学習者, I want PIC foundation が timer interrupt や preemption を開始しないことを確認できる, so that 第7章7.2の責務を第7章7.3以降の責務と混同しない

#### Acceptance Criteria

1. The RTOS shall not trigger PIT timer interrupts in this feature.
2. The RTOS shall not add a timer ISR, call `timer_tick()` from an ISR, or connect hardware interrupts to the scheduler in this feature.
3. The RTOS shall not call dispatcher, context switch, or preemption execution from an interrupt handler in this feature.
4. When README is reviewed after this feature, the documentation shall state that this chapter prepares PIC routing only and does not implement interrupt-driven task switching.

### Requirement 6: 検証可能な成果物

**Objective:** As a RTOS 開発者, I want build と QEMU serial log で PIC foundation の導入を確認できる, so that 実装済み範囲と次章への準備状態を客観的に検証できる

#### Acceptance Criteria

1. When `make` is executed, the RTOS shall build successfully with the PIC foundation included.
2. When `make run` is executed, the RTOS shall boot under QEMU and produce `docs/logs/qemu-serial.log`.
3. When `docs/logs/qemu-serial.log` is reviewed after `make run`, the log shall include PIC initialization evidence and existing smoke evidence.
4. When source documentation is reviewed, public or boundary-facing PIC interfaces shall have Doxygen-style comments explaining purpose, assumptions, limitations, and future replacement expectations.
