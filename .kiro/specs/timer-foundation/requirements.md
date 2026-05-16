# Requirements Document

## Introduction
第6章 6.2 として、学習目的の μITRON 風 RTOS に timer foundation を追加する。この機能は RTOS 内部で時間経過を扱うための最小単位として system tick を管理し、起動時検証で `timer_tick()` を明示的に呼び出して tick 増加を QEMU serial log で観測できる状態にする。

## Boundary Context
- **In scope**: 起動後 tick の初期化、明示的な tick 増加、現在 tick の取得、serial log による観測、README と検証ログの更新
- **Out of scope**: 実ハードウェアタイマ割り込み、割り込みハンドラ接続、preemption、time slice、`dly_tsk`、timeout、sleep/delay queue、ready queue、round-robin、μITRON 互換 API
- **Adjacent expectations**: 既存の task、scheduler、dispatcher、context switch、semaphore のログ順序と動作は維持され、timer foundation はそれらの責務を引き受けない

## Requirements

### Requirement 1: System Tick Lifecycle
**Objective:** As a RTOS 学習者, I want system tick を初期化・増加・取得できる, so that timer interrupt 前の段階で RTOS 内部時間の土台を検証できる

#### Acceptance Criteria
1. When kernel boot verification initializes timer foundation, the RTOS shall expose the current system tick as 0.
2. When `timer_tick()` is explicitly invoked once after initialization, the RTOS shall advance the system tick by exactly 1.
3. When `timer_tick()` is explicitly invoked multiple times after initialization, the RTOS shall preserve monotonically increasing tick values without skipping explicit invocations.
4. When current tick is requested after explicit tick invocations, the RTOS shall return the latest system tick value.

### Requirement 2: Explicit Timer Foundation Verification
**Objective:** As a RTOS 学習者, I want timer tick 増加を QEMU serial log で観測できる, so that 第6章 6.2 の到達点を実機割り込みなしで確認できる

#### Acceptance Criteria
1. When timer foundation is initialized during boot verification, the RTOS shall emit a serial log line showing timer initialization with tick 0.
2. When `timer_tick()` is explicitly invoked during boot verification, the RTOS shall emit serial log lines showing the incremented tick values.
3. When QEMU smoke verification is executed, the serial log shall show timer initialization before timer tick increments.
4. When QEMU smoke verification is executed, the serial log shall still show existing task, scheduler, dispatcher, context switch, and semaphore verification flow without timer-driven reordering.

### Requirement 3: Non-Preemptive Boundary
**Objective:** As a RTOS 学習者, I want timer foundation を interrupt/preemption から分離して確認できる, so that 次章の timer interrupt と preemption を安全に積み上げられる

#### Acceptance Criteria
1. The RTOS shall not connect timer foundation to PIT, APIC, HPET, or another hardware timer interrupt in this feature.
2. The RTOS shall not perform context switch, scheduler selection, or dispatcher invocation as a consequence of tick advancement in this feature.
3. The RTOS shall not provide completed `dly_tsk`, timeout, time slice, sleep queue, delay queue, ready queue, round-robin, or μITRON-compatible timer API behavior in this feature.
4. Where timer foundation documentation is updated, the documentation shall state that this feature is explicit-call based and not timer interrupt based.

### Requirement 4: Documentation And Evidence
**Objective:** As a RTOS 学習者, I want README と QEMU serial log に timer foundation の範囲と結果が残る, so that 実装済み範囲と次章の未実装範囲を区別できる

#### Acceptance Criteria
1. When README is reviewed after this feature, the documentation shall include a Chapter 6.2 timer item describing timer foundation and its non-preemptive scope.
2. When README is reviewed after this feature, the documentation shall state that preemption, time slice, timer interrupt, `dly_tsk`, and timeout remain future work.
3. When implementation evidence is reviewed after QEMU smoke verification, `docs/logs/qemu-serial.log` shall include timer initialization and tick increment lines.
4. When implementation evidence is reviewed after QEMU smoke verification, `docs/logs/qemu-serial.log` shall include evidence that existing task, scheduler, dispatcher, context switch, and semaphore verification still runs.
