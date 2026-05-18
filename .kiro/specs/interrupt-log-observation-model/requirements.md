# Requirements Document

## Introduction

この仕様は、μITRON風RTOSの第7章7.4「割り込み中のログ制約と観測モデル」として、7.3で追加した IRQ0/vector 32 timer interrupt entry の観測方法を整理する。対象ユーザーはRTOS学習者と開発者であり、現在は validation 時に割り込みハンドラ内から通常の serial log を出すため、既存boot logの途中に timer IRQ log が混ざり得る状態である。

今回の変更では、割り込み中のログ出力を通常bootの安定した観測ログとは区別し、handler到達確認のための validation 専用観測ログとして明文化する。これはプリエンプションやtimer subsystemの実装ではなく、割り込み中ログの制約を理解しながら、最小の到達観測を安全に扱うための学習用モデルである。

## Boundary Context

- **In scope**: 割り込み中ログ制約のREADME/spec/comment反映、timer IRQ handler内ログのvalidation専用観測ログ化、通常bootでtimer IRQ観測ログが出ないことの維持、`VALIDATE_TIMER_IRQ_ENTRY=1` 時だけの到達観測、QEMU serial log上で通常ログと観測ログの性質が分かる整理。
- **Out of scope**: PIT programming、hardware timer周期設定、`timer_tick()` 呼び出し、scheduler/dispatcher呼び出し、context switch、preemption完成、`iretq` による通常復帰、nested interrupt、連続割り込み、APIC/IOAPIC/LAPIC、SMP、μITRON API。
- **Adjacent expectations**: 7.3のtimer interrupt entry、IDT vector 32登録、IRQ0 validation flag、PIC EOI境界は維持する。kernel common にはPIC、vector番号、I/O port、entry stubの詳細を漏らさない。

## Requirements

### Requirement 1: 割り込み中ログ制約の明文化

**Objective:** As a RTOS学習者, I want 割り込み中のserial log制約を文書から理解できる, so that validation logを通常boot logと同じ安全性のログとして誤解しない

#### Acceptance Criteria

1. When READMEまたはdocsが確認される, the RTOS documentation shall 割り込み中のserial logが通常ログへ混ざり得る制約を説明する。
2. When timer IRQ handler周辺のコメントが確認される, the RTOS source shall handler内ログをvalidation専用の観測ログとして説明する。
3. The RTOS documentation shall 割り込み中観測ログが本格的なinterrupt-safe logging基盤ではないことを説明する。
4. The RTOS documentation shall nested interrupt、連続割り込み、通常の割り込み復帰をこの章で扱わないことを説明する。

### Requirement 2: 通常ログと割り込み観測ログの区別

**Objective:** As a RTOS開発者, I want 通常boot logと割り込み到達観測logを区別できる, so that QEMU serial log上の混在を検証用途として正しく読める

#### Acceptance Criteria

1. When normal boot runs, the RTOS shall timer IRQ観測ログを出力しない。
2. When `VALIDATE_TIMER_IRQ_ENTRY=1` is enabled, the RTOS shall timer IRQ handler到達を観測できるログを出力する。
3. When validation log is emitted from interrupt context, the RTOS shall そのログを通常boot smoke flowの順序保証対象として扱わない。
4. The RTOS shall use a distinct timer IRQ observation log prefix so that validation-only interrupt observation can be identified in QEMU serial output.

### Requirement 3: 最小のhandler到達観測モデル

**Objective:** As a RTOS学習者, I want timer IRQ handler到達だけを最小に観測できる, so that timer logicやschedulerへ進む前にinterrupt entry境界を検証できる

#### Acceptance Criteria

1. When explicit timer IRQ validation runs, the RTOS shall preserve the IRQ0/vector 32 handler arrival observation established in 7.3.
2. When the timer IRQ handler runs, the RTOS shall not call `timer_tick()`, scheduler, dispatcher, context switch, or task state transition logic.
3. When the timer IRQ handler runs, the RTOS shall keep the observation work minimal and send the existing IRQ0 EOI through the x86_64 PIC boundary.
4. The RTOS shall document that this model is temporary boot-time validation and not real context-switch-based interrupt handling.

### Requirement 4: 境界維持と検証証跡

**Objective:** As a RTOS開発者, I want arch固有の割り込み詳細をkernel commonへ漏らさずに検証できる, so that chapter 7.4が既存のHAL境界と学習用実装方針を壊さない

#### Acceptance Criteria

1. The RTOS shall keep PIC details, vector numbers, I/O ports, and entry stub details inside the x86_64 arch boundary.
2. When `make` is executed, the RTOS shall build successfully.
3. When `make run` is executed, the RTOS shall preserve the existing smoke flow without timer IRQ observation logs.
4. When `make run VALIDATE_TIMER_IRQ_ENTRY=1` is executed, the RTOS shall show validation-only timer IRQ handler arrival evidence.
5. The RTOS shall keep `.kiro/specs/interrupt-log-observation-model/` limited to the approved specification documents required by this feature.
