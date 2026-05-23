# Requirements Document

## Introduction

この仕様は、μITRON風RTOSの第8章8.4「割り込みentry/exitの責務を整理する」を扱う。対象ユーザーは、RTOSを段階的に学習実装している開発者である。前回8.3では、IRQ0/vector 32 の timer IRQ handler から `timer_tick()`、`preemption_evaluate_from_irq()`、`dispatch_pending_log_state_from_irq()`、`arch_pic_send_eoi(IRQ0)` まで接続し、preemption decision の結果を dispatch pending として保持・観測できるようにした。

今回8.4では、その既存経路を維持したまま、割り込み処理を interrupt entry、kernel IRQ handler、interrupt exit の責務に分けて README、spec、コメント、検証ログから理解できる状態にする。ただし、dispatcher、context switch、task state変更、dispatch pending消費、interrupt return直前の実切替には接続しない。

## Boundary Context

- **In scope**: IRQ0/vector 32 の timer IRQ 経路における interrupt entry、kernel IRQ handler、interrupt exit boundary の責務明記。既存の `timer_tick()`、preemption decision、dispatch pending 観測、IRQ0 EOI の順序維持。dispatch pending が要求されても8.4では消費しないことの文書化とログ観測。README、Doxygenコメント、`docs/logs/qemu-serial.log`、spec成果物の更新。
- **Out of scope**: dispatcher呼び出し、`dispatcher_commit_current()`、context switch、task stack切り替え、register save/restoreの本格利用、task state変更、RUNNING/READYの実切替、dispatch pending消費、interrupt return直前の実切替、`iretq` による通常の割り込み復帰モデル完成、nested interrupt、連続割り込みの安定運用、同一優先度タイムスライス、sleep/delay queue、semaphore wakeup連携、APIC/IOAPIC/LAPIC、SMP、μITRON API。
- **Adjacent expectations**: arch/x86_64 側は kernel public API だけを呼び、scheduler/dispatcher内部へ過度に依存しない。kernel common 側へ PIC、vector番号、I/O port、entry stub の詳細を漏らさない。dispatch pending の状態管理は kernel 側に閉じる。割り込み中ログは第7章7.4の方針どおり validation 専用観測として扱う。

## Requirements

### Requirement 1: timer IRQ経路の既存責務維持

**Objective:** As a RTOS学習者, I want 8.1から8.3までの timer IRQ 経路を維持したまま責務境界を整理したい, so that 既存の到達点を壊さず8.4の理解に進める

#### Acceptance Criteria

1. When IRQ0/vector 32 timer interrupt handler runs, the RTOS shall call `timer_tick()` before evaluating preemption.
2. When `timer_tick()` returns inside the timer IRQ handler, the RTOS shall call the IRQ-originated preemption decision API before observing dispatch pending.
3. When preemption decision observation completes, the RTOS shall observe dispatch pending state before sending IRQ0 EOI.
4. When dispatch pending observation completes, the RTOS shall send IRQ0 EOI.
5. The RTOS shall preserve normal boot smoke behavior when `make run` is executed without timer IRQ validation flags.

### Requirement 2: interrupt entry責務の明確化

**Objective:** As a maintainer, I want timer IRQ entry の責務を最小観測入口として明記したい, so that entry stub とC側handlerの境界を後続章で拡張しやすくなる

#### Acceptance Criteria

1. When documentation describes the timer IRQ path, the RTOS shall identify interrupt entry as CPU arrival at IRQ0/vector 32 and transfer from entry stub to the C handler.
2. When source comments describe the timer IRQ stub, the RTOS shall state that full register save/restore and normal `iretq` return are not implemented in 8.4.
3. The interrupt entry responsibility shall not include scheduler selection, dispatcher commit, context switch, task state transition, or dispatch pending consumption.

### Requirement 3: kernel IRQ handler責務の明確化

**Objective:** As a maintainer, I want C側timer IRQ handlerの責務をtick更新、preemption判定、dispatch pending観測、EOIに限定したい, so that handlerがdispatcherやcontext switchへ肥大化しない

#### Acceptance Criteria

1. When the timer IRQ C handler runs, the RTOS shall keep the observable sequence as timer tick, preemption decision, dispatch pending observation, interrupt exit boundary observation, and IRQ0 EOI.
2. The timer IRQ C handler shall not call dispatcher commit, context switch, task stack switching, register save/restore, or task state transition logic.
3. The timer IRQ C handler shall use kernel public APIs for timer, preemption decision, and dispatch pending observation.
4. The timer IRQ C handler shall keep PIC EOI handling in the arch/x86_64 boundary.

### Requirement 4: interrupt exit boundaryの明確化

**Objective:** As a RTOS学習者, I want interrupt exit を将来dispatch pendingを消費する候補境界として観測したい, so that 8.4では実切替しないことを明確に理解できる

#### Acceptance Criteria

1. When dispatch pending is not requested, the RTOS shall log or document the interrupt exit boundary as deferred without dispatch.
2. When dispatch pending is requested, the RTOS shall preserve the pending state and shall not consume it in 8.4.
3. When dispatch pending is requested, the RTOS shall not dispatch a task, change current task, change task states, or perform context switching as part of interrupt exit.
4. Where documentation is updated for 8.4, the documentation shall state that interrupt exit boundary is a future connection point for dispatcher/context switch and not an active dispatcher in this feature.

### Requirement 5: 8.4検証成果物の更新

**Objective:** As a maintainer, I want build、run、validation log、spec artifacts to reflect 8.4, so that fresh evidence can verify the responsibility boundary change

#### Acceptance Criteria

1. When `make` is executed, the RTOS shall build successfully.
2. When `make run` is executed, the RTOS shall boot through the existing smoke flow without requiring timer IRQ validation.
3. When `make run VALIDATE_TIMER_IRQ_ENTRY=1` is executed, the RTOS shall produce serial log evidence for timer IRQ entry, tick update, preemption decision, dispatch pending observation, interrupt exit boundary observation, and EOI.
4. The RTOS shall update `docs/logs/qemu-serial.log` with validation evidence for 8.4.
5. The spec directory for this feature shall contain only `requirements.md`, `design.md`, and `tasks.md` after implementation validation is complete.
