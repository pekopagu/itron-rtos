# Requirements Document

## Introduction

この仕様は、μITRON風RTOSの第8章8.2「割り込みハンドラからプリエンプション判定を呼ぶ」を扱う。対象ユーザーは、RTOSを段階的に学習・実装する開発者である。第8章8.1では IRQ0/vector 32 の timer interrupt handler から `timer_tick()` を呼び、hardware timer interrupt 起点で kernel timer tick が進むことを確認した。

今回の変更では、既存の tick 接続を維持したまま、`timer_tick()` の直後に preemption decision の入口を呼ぶ。目的は「割り込み起点で preemption decision が評価される」ことを観測することであり、dispatcher、context switch、task state変更、dispatch pending の本格実装には進まない。

## Boundary Context

- **In scope**: IRQ0/vector 32 timer IRQ handler からの `timer_tick()` 呼び出し維持、`timer_tick()` 後の preemption decision 入口呼び出し、IRQ起点の preemption decision 到達ログ、EOI維持、README / コメント / spec / QEMU serial log の更新。
- **Out of scope**: dispatcher呼び出し、context switch、task stack切り替え、register save/restoreの本格利用、task state変更、dispatch pendingの本格実装または確定、通常の `iretq` 割り込み復帰モデル完成、nested interrupt、連続割り込みの安定運用、同一優先度タイムスライス、sleep/delay queue、semaphore wakeup連携、APIC / IOAPIC / LAPIC、SMP、μITRON API。
- **Adjacent expectations**: 既存の preemption foundation が提供する scheduler decision API は再利用してよい。ただし arch/x86_64 側は scheduler / dispatcher の内部へ直接依存せず、kernel common 側の public API または薄い境界関数を経由する。

## Requirements

### Requirement 1: Timer IRQ handler からの preemption decision 入口呼び出し

**Objective:** As a RTOS学習者, I want timer IRQ handler から tick 更新後に preemption decision 入口へ到達できる, so that interrupt 起点の preemption 判定導線を実切替前に観測できる

#### Acceptance Criteria

1. When IRQ0/vector 32 timer interrupt handler is reached during explicit validation, the RTOS shall call `timer_tick()` before invoking the preemption decision entry.
2. When `timer_tick()` returns inside the timer IRQ handler, the RTOS shall invoke exactly one kernel-side preemption decision entry before sending IRQ0 EOI.
3. When the preemption decision entry is invoked from the timer IRQ handler, the RTOS shall evaluate the current scheduler preemption decision through an existing public preemption foundation API or a thin kernel boundary.
4. The RTOS shall keep PIC, vector number, I/O port, and entry stub details inside the x86_64 arch boundary.

### Requirement 2: IRQ起点 preemption decision の観測

**Objective:** As a RTOS学習者, I want QEMU serial log で IRQ 起点の preemption decision 到達を確認できる, so that 8.3 の dispatch pending 観測へ進む前に判断入口の接続を検証できる

#### Acceptance Criteria

1. When `make run VALIDATE_TIMER_IRQ_ENTRY=1` is executed, the RTOS shall produce serial log evidence that the timer IRQ handler was reached.
2. When the timer IRQ handler calls `timer_tick()`, the RTOS shall produce serial log evidence that the tick advanced from the IRQ-originated path.
3. When the preemption decision entry runs after the IRQ-originated tick update, the RTOS shall produce minimal serial log evidence showing that an IRQ-originated preemption decision was evaluated.
4. While interrupt-originated validation logging is enabled, the RTOS shall treat timer IRQ and preemption decision logs as validation-only observation logs that may interleave with normal boot logs.

### Requirement 3: 非切替境界の維持

**Objective:** As a maintainer, I want preemption decision を呼んでも実際の dispatch や context switch に進まない, so that 8.2 の責務を判定入口の接続に限定できる

#### Acceptance Criteria

1. When the IRQ-originated preemption decision result indicates a switch candidate, the RTOS shall not call dispatcher, context switch, task stack switching, or register save/restore as part of this feature.
2. When the IRQ-originated preemption decision is evaluated, the RTOS shall not change task states or commit a new current task.
3. The RTOS shall not implement or finalize dispatch pending behavior in this feature.
4. Where documentation is updated for this feature, the documentation shall state that 8.2 reaches only the preemption decision entry and does not perform an actual task switch.

### Requirement 4: 既存 smoke flow と検証証跡の維持

**Objective:** As a maintainer, I want 既存の通常 boot と検証ログを維持できる, so that 段階的なRTOS実装の前提を壊さずに8.2を追加できる

#### Acceptance Criteria

1. When `make` is executed, the RTOS shall build successfully with the IRQ-originated preemption decision entry included.
2. When `make run` is executed without validation flags, the RTOS shall preserve the existing smoke flow without timer IRQ handler arrival logs.
3. When `make run VALIDATE_TIMER_IRQ_ENTRY=1` is executed, the RTOS shall preserve IRQ0 EOI evidence after the tick update and preemption decision entry.
4. The RTOS shall update `docs/logs/qemu-serial.log` with validation evidence for timer IRQ arrival, IRQ-originated tick advancement, IRQ-originated preemption decision evaluation, and EOI.
5. The spec directory for this feature shall contain only `requirements.md`, `design.md`, and `tasks.md` after implementation validation is complete.
