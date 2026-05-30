# Requirements Document

## Introduction

第11章11.3では、11.1/11.2で扱ってきた「同一優先度READY taskはtimer IRQ由来のtime slice対象外」という仕様を明文化し、実装とログを安定化する。対象ユーザーは、μITRON風RTOSを段階的に学習実装している開発者である。現在はtimer IRQ後に高優先度READYを検出し、dispatch pendingを後段boundaryでconsumeして既存のdispatcher switchへ接続できるが、同一優先度READYの扱いを将来のround-robinやtime sliceと混同しないよう固定したい。今回変更すべきことは、currentと同じpriorityのREADY taskが存在してもdispatch pendingをrequestせず、`same-priority-not-timeslice-target` として観測できるようにすることである。

## Boundary Context

- **In scope**: scheduler/preemption側の同一優先度READY判定、`same-priority-not-timeslice-target` reasonの維持、dispatch pending未requestログ、no-pending consumeログ、11.2高優先度READY deferred dispatch経路の維持、10.4 `yield_tsk()`協調context switch経路の維持、README/Doxygen/spec/log更新。
- **Out of scope**: 同一優先度time slice、round-robin、tick countによるslice管理、同一優先度task順序管理、semaphore wakeup連携、sleep/delay queue、nested interrupt、完全な割り込み復帰frame切替、APIC/IOAPIC/LAPIC、SMP。
- **Adjacent expectations**: priority値は小さいほど高優先度として扱う。timer IRQ handler本体は`yield_tsk()`や`dispatcher_switch_to()`を直接呼ばず、11.2のdispatch pending consume経路を維持する。

## Requirements

### Requirement 1: 同一優先度READYのpreemption対象外化
**Objective:** RTOS学習者として、timer IRQ後の同一優先度READY taskをtime slice対象外として明確に扱いたい。これにより、11.3時点の仕様と将来のround-robin/time slice設計を混同せずに検証できる。

#### Acceptance Criteria
1. When timer IRQ由来のpreemption判断が実行され、current RUNNING taskと同じpriority値のREADY taskだけが存在する場合、RTOS shall not treat that READY task as a preemption target.
2. When 同一優先度READYだけが存在する場合、RTOS shall keep the decision reason as `same-priority-not-timeslice-target`.
3. When scheduler/preemption compares priorities, RTOS shall treat a numerically smaller priority value as higher priority.
4. When a READY task has a numerically smaller priority value than current, RTOS shall continue to treat that task as a higher-priority READY preemption target.

### Requirement 2: dispatch pending未requestとno-dispatch境界
**Objective:** maintainerとして、同一優先度READYだけではdispatch pendingが作られないことをログで確認したい。これにより、time slice未実装の状態を安全に固定できる。

#### Acceptance Criteria
1. When 同一優先度READYだけが存在する場合、RTOS shall not request dispatch pending.
2. When dispatch pending is not requested because of same priority, RTOS shall log `[dispatch-pending] not-requested: reason=same-priority-not-timeslice-target`.
3. When interrupt exit boundary observes no pending after same-priority evaluation, RTOS shall log `dispatch-pending=none action=no-dispatch`.
4. When deferred consume API is called with no pending, RTOS shall log `consume skipped: reason=no-pending`.
5. When no pending exists, RTOS shall not call `dispatcher_switch_to()` from the deferred consume path.

### Requirement 3: 既存11.2/10.4経路の維持
**Objective:** maintainerとして、11.3の固定化が高優先度READYのdeferred dispatchや協調yieldを壊していないことを確認したい。これにより、今回の変更を同一優先度time slice除外に限定できる。

#### Acceptance Criteria
1. When a higher-priority READY task exists after timer IRQ, RTOS shall request dispatch pending with reason `higher-priority-ready`.
2. When requested pending reaches interrupt exit boundary, RTOS shall continue to log `dispatch-pending=requested action=deferred-dispatch`.
3. When requested pending is consumed and from/to states are valid, RTOS shall continue to call `dispatcher_switch_to(from, to)` through the deferred dispatch boundary.
4. When normal `make run` is executed, RTOS shall preserve the 10.4 `yield_tsk()` cooperative context switch path.
5. The timer IRQ handler body shall not call `yield_tsk()` or directly call `dispatcher_switch_to()`.

### Requirement 4: 文書化・検証証跡・spec成果物
**Objective:** maintainerとして、11.3の到達点と未実装範囲をREADME、Doxygen、spec、serial logに残したい。これにより、次章以降のtime sliceやround-robin設計と境界を保てる。

#### Acceptance Criteria
1. When implementation is complete, README shall describe that same-priority READY is explicitly not a time slice target in 11.3.
2. When implementation is complete, README shall state that round-robin, tick-based slice management, semaphore wakeup integration, nested interrupts, and complete interrupt-return-frame switching remain unimplemented.
3. When implementation is complete, meaningful public or internal interfaces shall have Japanese Doxygen comments describing the 11.3 purpose, assumptions, limitations, and non-goals.
4. When validation is complete, `docs/logs/qemu-serial.log` shall contain fresh validation evidence for same-priority no-pending and higher-priority deferred dispatch.
5. When validation is complete, `.kiro/specs/same-priority-not-timeslice-target/` shall contain only `requirements.md`, `design.md`, and `tasks.md`.
