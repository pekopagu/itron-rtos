# Requirements Document

## Introduction

この仕様は、μITRON風RTOSの第8章8.1「ハードウェアタイマから `timer_tick()` を呼ぶ」として、IRQ0/vector 32 の timer interrupt handler から既存の kernel timer tick 更新を呼び出す段階を定義する。対象ユーザーは、学習目的でRTOSを段階的に構築する開発者である。

現在の実装は、第7章7.3で timer IRQ handler への到達と PIC EOI を観測でき、第7章7.4で割り込み中ログは validation 専用の観測ログとして通常 boot log の順序保証対象にしない方針を整理している。一方、kernel timer の `timer_tick()` は boot-time smoke から明示的に呼ばれるだけで、hardware timer interrupt 起点では進まない。今回の変更では、timer IRQ handler の責務を「tick 更新 + EOI」までに限定し、hardware interrupt 起点で tick が進むことを QEMU serial log で観測できるようにする。

## Boundary Context

- **In scope**: IRQ0/vector 32 timer IRQ handler からの `timer_tick()` 呼び出し、割り込み起点の tick 更新ログ、PIC EOI 維持、README と Doxygen コメントによる8.1到達点の明記、validation run の QEMU serial log 更新。
- **Out of scope**: PIT programming の本格化、hardware timer 周期設定、scheduler 呼び出し、dispatcher 呼び出し、preemption 判定、context switch、dispatch pending、task state 変更、sleep/delay queue、timeout 処理、`iretq` による通常の割り込み復帰モデル完成、nested interrupt、連続割り込みの安定運用、APIC/IOAPIC/LAPIC、SMP、μITRON API。
- **Adjacent expectations**: 第7章7.4の割り込み中ログ制約を維持する。timer IRQ validation は通常 boot smoke と同じ順序保証対象ではない。kernel common に PIC、vector番号、I/O port、entry stub の詳細を漏らさない。

## Requirements

### Requirement 1: Timer IRQ 起点の tick 更新

**Objective:** As a RTOS 学習者, I want IRQ0/vector 32 の timer interrupt handler から kernel timer tick が進む, so that hardware timer interrupt と RTOS 内部 tick の最初の接続を観測できる

#### Acceptance Criteria

1. When IRQ0/vector 32 timer interrupt handler is reached during explicit validation, the RTOS shall call the kernel timer tick update exactly once before completing handler-side work.
2. When the timer interrupt handler calls the tick update, the RTOS shall advance the kernel timer tick by one from the current timer state.
3. When the timer interrupt handler completes the tick update, the RTOS shall still send EOI for IRQ0 through the x86_64 PIC boundary.
4. The RTOS shall keep timer IRQ handler responsibility limited to tick update and EOI for this feature.

### Requirement 2: 割り込み起点 tick の観測

**Objective:** As a RTOS 学習者, I want QEMU serial log で interrupt 起点の tick 更新を識別できる, so that boot-time explicit tick と hardware IRQ 起点 tick を区別して検証できる

#### Acceptance Criteria

1. When `make run VALIDATE_TIMER_IRQ_ENTRY=1` is executed, the RTOS shall produce serial log evidence that the timer IRQ handler was reached.
2. When the timer IRQ handler updates the tick, the RTOS shall produce serial log evidence showing that `timer_tick()` advanced the tick from an interrupt-originated path.
3. While interrupt-originated validation logging is enabled, the RTOS shall treat timer IRQ handler logs as validation-only observation logs that may interleave with normal boot logs.
4. When `make run` is executed without validation flags, the RTOS shall preserve the existing smoke flow without timer IRQ handler arrival logs.

### Requirement 3: 非プリエンプティブ境界

**Objective:** As a RTOS 学習者, I want tick 更新が scheduler や context switch に接続されない, so that timer interrupt 接続と preemption 実装を安全に分離できる

#### Acceptance Criteria

1. When `timer_tick()` is called from the timer IRQ handler, the RTOS shall not call scheduler, dispatcher, context switch, preemption, dispatch pending, or task state transition logic as a consequence of that call.
2. The RTOS shall not implement sleep, delay queue, timeout, time slice, round-robin, or μITRON timer API behavior in this feature.
3. Where documentation is updated for this feature, the documentation shall state that 8.1 stops at tick connection and does not implement preemption.
4. Where `task_context_t` is documented, the documentation shall state that it is a future minimal context switch save area and does not save or restore actual interrupt-time CPU register values in this stage.

### Requirement 4: 境界維持と検証証跡

**Objective:** As a RTOS 学習者, I want arch 固有の割り込み詳細と kernel timer 責務が分離されたまま検証証跡を残せる, so that 第8章以降の発展時にも責務境界を追跡できる

#### Acceptance Criteria

1. The RTOS shall keep PIC, vector number, I/O port, and entry stub details inside the x86_64 arch boundary.
2. When arch code calls the timer tick update, the RTOS shall use only the kernel timer public boundary instead of depending on timer internal state.
3. When source comments are reviewed, new or updated public/boundary-facing code shall include Doxygen-style Japanese comments describing purpose, assumptions, limitations, and non-goals.
4. When implementation evidence is reviewed, `docs/logs/qemu-serial.log` shall contain validation evidence for timer IRQ arrival, interrupt-originated tick advancement, and preserved EOI position.
