# Requirements Document

## Introduction

この仕様は、μITRON風RTOSの第8章8.3「ディスパッチ保留を観測する」を扱う。対象ユーザーは、RTOSを段階的に学習・実装している開発者である。前回8.2では、IRQ0/vector 32 の timer interrupt handler が `timer_tick()` 後に `preemption_evaluate_from_irq()` を呼び、preemption decision の入口到達を観測できるようになった。

今回の変更では、8.1の timer IRQ handler から `timer_tick()` を呼ぶ構成と、8.2の preemption decision 呼び出しを維持したまま、decision 結果を kernel 側の dispatch pending という論理状態として保留し、QEMU serial log で観測できるようにする。8.3は dispatch pending を「作って観測する」段階であり、dispatcher、context switch、task state変更には接続しない。

## Boundary Context

- **In scope**: kernel 側の dispatch pending 状態、dispatch pending public API、IRQ由来 preemption decision から dispatch pending への接続、dispatch pending 観測ログ、timer IRQ handler の `timer_tick()` -> preemption decision -> dispatch pending observation -> EOI の流れ、README/spec/comment/log 更新。
- **Out of scope**: dispatcher呼び出し、`dispatcher_commit_current()`、context switch、task stack切り替え、register save/restoreの本格利用、task state変更、interrupt return直前の切替、nested interrupt、連続割り込み安定運用、同一優先度タイムスライス、sleep/delay queue、semaphore wakeup連携、APIC/IOAPIC/LAPIC、SMP、μITRON API。
- **Adjacent expectations**: arch/x86_64 側は kernel public API だけを呼び、scheduler/dispatcher 内部へ過度に依存しない。kernel common 側へ PIC、vector番号、I/O port、entry stub の詳細を漏らさない。割り込み中ログは第7章7.4の validation 専用観測として扱う。

## Requirements

### Requirement 1: dispatch pending 状態の保持

**Objective:** As a RTOS学習者, I want preemption decision の switch 要求を dispatch pending として保持できる, so that 実切替前の保留状態を段階的に検証できる

#### Acceptance Criteria

1. When the kernel is initialized for normal boot, the RTOS shall start with dispatch pending not requested.
2. When an IRQ-originated preemption decision indicates a switch target, the RTOS shall record dispatch pending as requested with an IRQ source reason.
3. When an IRQ-originated preemption decision indicates no-switch, invalid-current, or no-current, the RTOS shall keep dispatch pending not requested.
4. The RTOS shall expose a public kernel API to request dispatch pending from IRQ context.
5. The RTOS shall expose a public kernel API to observe whether dispatch pending is requested.
6. The RTOS shall expose a public kernel API to clear dispatch pending for test or later boundary use.

### Requirement 2: dispatch pending の観測ログ

**Objective:** As a maintainer, I want QEMU serial log で dispatch pending の有無と最小限の理由を観測できる, so that 8.3の到達点を実行ログで確認できる

#### Acceptance Criteria

1. When `make run VALIDATE_TIMER_IRQ_ENTRY=1` reaches the timer IRQ handler, the RTOS shall log timer IRQ arrival, IRQ-originated tick advancement, preemption decision evaluation, dispatch pending observation, and IRQ0 EOI.
2. When dispatch pending is not requested after a decision, the RTOS shall log a minimal not-requested message with the decision reason.
3. When dispatch pending is requested after a switch-target decision, the RTOS shall log a minimal requested message with source, reason, and candidate task id.
4. While interrupt-originated validation logging is enabled, the RTOS shall treat dispatch pending logs as validation-only observation logs that may interleave with normal boot logs.

### Requirement 3: timer IRQ handler の責務整理

**Objective:** As a maintainer, I want timer IRQ handler の責務を tick更新、preemption判定、dispatch pending観測、EOI に限定する, so that 8.4の割り込みentry/exit責務整理へ進める

#### Acceptance Criteria

1. When IRQ0/vector 32 timer interrupt handler runs, the RTOS shall call `timer_tick()` before evaluating preemption.
2. When `timer_tick()` returns inside the timer IRQ handler, the RTOS shall evaluate preemption before observing dispatch pending.
3. When dispatch pending observation completes, the RTOS shall send IRQ0 EOI.
4. The timer IRQ handler shall not call dispatcher commit, context switch, task stack switching, register save/restore, or task state transition logic.

### Requirement 4: 非切替境界の維持

**Objective:** As a RTOS学習者, I want dispatch pending が true でも実際の切替が起きない, so that 8.3を観測専用段階として理解できる

#### Acceptance Criteria

1. When dispatch pending is requested, the RTOS shall not dispatch a task as part of this feature.
2. When dispatch pending is requested, the RTOS shall not change the current task or task states as part of this feature.
3. Where documentation is updated for this feature, the documentation shall state that 8.3 observes dispatch pending only and does not connect it to real switching.
4. The RTOS shall preserve the existing normal smoke flow when `make run` is executed without validation flags.

### Requirement 5: 検証成果物の更新

**Objective:** As a maintainer, I want build, run, validation log, and spec artifacts to reflect 8.3, so that the implementation can be reviewed from fresh evidence

#### Acceptance Criteria

1. When `make` is executed, the RTOS shall build successfully.
2. When `make run` is executed, the RTOS shall boot through the existing smoke flow without requiring timer IRQ validation.
3. When `make run VALIDATE_TIMER_IRQ_ENTRY=1` is executed, the RTOS shall produce serial log evidence for dispatch pending observation after the preemption decision and before EOI.
4. The RTOS shall update `docs/logs/qemu-serial.log` with the validation evidence.
5. The spec directory for this feature shall contain only `requirements.md`, `design.md`, and `tasks.md` after implementation validation is complete.

