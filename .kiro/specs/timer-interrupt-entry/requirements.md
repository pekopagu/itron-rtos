# Requirements Document

## Introduction

この仕様は、μITRON風RTOSの第7章 7.3「タイマ割り込み入口を作る」として、7.1のIDT/例外entry基盤と7.2のlegacy PIC基盤の上に、PICでremap済みのIRQ0/vector 32へ対応するtimer interrupt entryを追加する。対象ユーザーはRTOS学習者と開発者であり、現在の「PICは初期化済みだが全IRQ maskedのまま」という状態から、「vector 32の割り込み入口に到達できる構造がある」状態へ進める。

今回の目的は、timer interruptをRTOSのtickやpreemptionへ接続することではない。x86_64側に閉じたentry stub、IDT vector 32登録、C側handler、最小限のserial log、EOI送信位置の設計を追加し、後続の7.4「割り込み中のログ制約と観測モデル」と第8章のtimer logic接続へ進むための観測可能な土台を作る。

## Boundary Context

- **In scope**: IDT vector 32用entry登録、x86_64 timer IRQ entry stub、C側timer interrupt handler、handler到達時の最小serial log、legacy PICへの必要最小限のEOI送信、IRQ0 unmaskを明示的検証時だけ扱える境界、READMEとDoxygenによる範囲説明。
- **Out of scope**: PIT programming、hardware timer周期設定、`timer_tick()`呼び出し、preemption判定、scheduler/dispatcher呼び出し、context switch、task state変更、interrupt中の本格ログ設計、nested interrupt、APIC/IOAPIC/LAPIC、SMP、μITRON API、既存RTOS実装コードの参照・コピー・流用。
- **Adjacent expectations**: 7.1のIDT/例外entry基盤と7.2のPIC remap/all-mask基盤は維持される。通常bootではIRQ0をunmaskしないため、既存のboot-time smoke flowを乱さない。明示的な検証buildではIRQ0 unmaskとinterrupt enableを使って到達確認してよいが、それでもPIT再設定やtimer_tick接続は行わない。

## Requirements

### Requirement 1: vector 32 timer interrupt entry

**Objective:** As a RTOS開発者, I want IRQ0/vector 32用のentryがIDTに登録される, so that PIC remap後のtimer IRQ入口をCPU例外entryと分けて観測できる

#### Acceptance Criteria

1. When interrupt foundation initialization runs, the RTOS shall register an IDT gate for vector 32.
2. When vector 32 is registered, the RTOS shall keep CPU exception vectors 0 through 31 reserved for CPU exceptions.
3. When normal boot runs, the RTOS shall complete IDT initialization with the vector 32 entry available without invoking the timer interrupt handler.
4. The RTOS shall document that vector 32 corresponds to remapped PIC IRQ0 in the current legacy PIC model.

### Requirement 2: x86_64 timer IRQ entry stub and C handler

**Objective:** As a RTOS学習者, I want timer IRQ entryからC handlerへ到達したことをserial logで確認できる, so that timer_tick接続前に割り込み入口だけを独立して検証できる

#### Acceptance Criteria

1. When vector 32 is delivered to the CPU, the RTOS shall transfer control from the x86_64 entry stub to a C-side timer interrupt handler.
2. When the timer interrupt handler is reached, the RTOS shall emit a minimal serial log line indicating timer IRQ entry arrival.
3. When the timer interrupt handler runs, the RTOS shall not call `timer_tick()`, scheduler, dispatcher, context switch, or task state transition logic.
4. When source documentation is reviewed, the timer IRQ handler shall be described as a temporary entry-arrival observation handler, not as a complete timer subsystem.

### Requirement 3: EOI placement and PIC boundary

**Objective:** As a RTOS開発者, I want IRQ0 handler到達後に必要最小限のEOI位置が明確である, so that 後続章でtimer logicへ接続する前にPIC interrupt completion boundaryを誤解しない

#### Acceptance Criteria

1. When the timer interrupt handler handles vector 32, the RTOS shall send EOI through the x86_64 PIC boundary after minimal observation work.
2. The RTOS shall keep PIC EOI port operations inside the x86_64 arch boundary.
3. The RTOS shall provide a documented arch-local PIC EOI interface for IRQ lines.
4. If a caller requests EOI for an IRQ outside the PIC IRQ range, then the RTOS shall ignore the request without changing unrelated state.

### Requirement 4: IRQ0 unmask and validation control

**Objective:** As a RTOS学習者, I want IRQ0 unmask and interrupt enable to be explicitly controlled, so that timer interrupt entry verification does not accidentally become preemption or continuous timer execution

#### Acceptance Criteria

1. When normal boot runs, the RTOS shall keep IRQ0 masked by default.
2. When explicit timer entry validation is enabled, the RTOS may unmask IRQ0 and enable interrupts only for entry-arrival observation.
3. When explicit timer entry validation is not enabled, the RTOS shall not unmask IRQ0.
4. When explicit timer entry validation is enabled, the RTOS shall still not program PIT frequency or connect timer IRQ to kernel timer logic.

### Requirement 5: HAL/kernel boundary preservation

**Objective:** As a RTOS開発者, I want timer interrupt entry details to remain inside arch/x86_64, so that kernel common code does not learn PIC, vector, or entry-stub details

#### Acceptance Criteria

1. The RTOS shall keep timer IRQ entry stub, vector 32 registration detail, and PIC EOI detail inside `arch/x86_64`.
2. When kernel code requests optional timer entry validation, the RTOS shall use a HAL boundary instead of including x86_64 timer/PIC headers directly.
3. The RTOS shall keep existing HAL console and HAL interrupt responsibilities unchanged except for the minimal validation entry point.
4. The RTOS shall not expose `timer_tick()` or scheduler-facing timer interrupt APIs from this feature.

### Requirement 6: Build, smoke, and documentation evidence

**Objective:** As a RTOS学習者, I want make/QEMU serial log/documentation evidence for the new entry boundary, so that chapter 7.3 can be verified without claiming a complete interrupt-driven scheduler

#### Acceptance Criteria

1. When `make` is executed, the RTOS shall build successfully with the timer interrupt entry included.
2. When `make run` is executed without validation flags, the RTOS shall boot and preserve existing PIC and smoke logs without timer IRQ arrival logs.
3. When the explicit timer entry validation run is executed, the RTOS shall produce serial log evidence that the timer IRQ handler was reached.
4. When source documentation and README are reviewed, public or boundary-facing timer IRQ entry interfaces shall have Doxygen-style comments explaining purpose, assumptions, limitations, and future replacement expectations.
