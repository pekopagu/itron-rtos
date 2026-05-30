# Requirements Document

## Introduction

第11章11.4では、11.1から11.3で追加したtimer IRQ後のpreemption関連ログを、教育用RTOSの検証証跡として安定化する。対象ユーザーは、μITRON風RTOSを段階的に学習・保守する開発者である。現在は高優先度READY検出、dispatch pending request、deferred dispatch、pending consume、dispatcher switch、pending clear、同一優先度READY no-dispatchの各ログが観測できるが、11.4では順序、reason文字列、重複抑止、README/spec/Doxygen/logの到達点記録を明確に固定する。

## Boundary Context

- **In scope**: timer IRQ由来preemptionログ順序、preemption decisionのresult/reason表記、dispatch pending request/consume/clear/not-requested/consume skippedの表記、timer IRQ exit boundaryログ、同一優先度READY no-dispatchログ、11.2高優先度READY deferred dispatch経路、10.4 `yield_tsk()`協調switch経路、README・Doxygen・serial log・spec成果物更新。
- **Out of scope**: 同一優先度time slice、round-robin、tick countによるslice管理、semaphore wakeup連携、sleep/delay queue、nested interrupt、完全な割り込み復帰フレーム切替、APIC/IOAPIC/LAPIC、SMP、dispatcher/context switchの根本変更。
- **Adjacent expectations**: priority値が小さいtaskを高優先度として扱う。timer IRQ handler本体は `yield_tsk()` と `dispatcher_switch_to()` を直接呼ばない。既存RTOSのソースコード参照・コピー・流用は行わない。

## Requirements

### Requirement 1: preemption decisionログ順序とreason表記
**Objective:** 開発者として、timer IRQ後のpreemption decisionログを安定した順序と表記で確認したい。これにより、11.1から11.3の挙動差分をserial logから再現性高く検証できる。

#### Acceptance Criteria
1. When timer IRQ由来preemption評価が実行される, the RTOS shall log current task before candidate-specific decision details.
2. When a numerically smaller-priority READY task exists, the RTOS shall log `higher-ready detected` before `decision evaluated`.
3. When no higher-priority READY task exists because only same-priority READY is available, the RTOS shall log `no higher-ready: reason=same-priority-not-timeslice-target`.
4. When decision is evaluated, the RTOS shall use `result=request-switch reason=higher-priority-ready` for higher-priority READY and `result=no-switch reason=same-priority-not-timeslice-target` for same-priority READY.
5. The RTOS shall treat only READY tasks whose priority value is smaller than current as preemption targets.

### Requirement 2: dispatch pendingログの安定化
**Objective:** 保守者として、dispatch pendingのrequest、consume、clear、not-requested、consume skippedを一貫した形式で確認したい。これにより、同じイベントの重複やreason表記ゆれを避けられる。

#### Acceptance Criteria
1. When higher-priority READY requests dispatch pending, the RTOS shall log `[dispatch-pending] requested: reason=higher-priority-ready from id=... name=... to id=... name=...`.
2. When requested pending is consumed, the RTOS shall log `[dispatch-pending] consumed: reason=higher-priority-ready from id=... name=... to id=... name=...`.
3. When deferred dispatch completes, the RTOS shall log `[dispatch-pending] cleared: reason=dispatch-completed`.
4. When no pending is requested, the RTOS shall log `[dispatch-pending] not-requested: reason=<decision-reason>`.
5. When no pending exists at consume boundary, the RTOS shall log `[dispatch-pending] consume skipped: reason=no-pending`.
6. If the same pending request is observed more than once before consume, then the RTOS shall not emit duplicate requested logs for the same event.

### Requirement 3: timer IRQ exit boundaryと既存経路の維持
**Objective:** 開発者として、timer IRQ handler本体の責務を広げず、11.2/11.3/10.4の既存経路を壊さずにログだけを安定化したい。これにより、教育用の境界分離を保ったまま次章へ進められる。

#### Acceptance Criteria
1. When pending is requested at timer IRQ exit boundary, the RTOS shall log `dispatch-pending=requested action=deferred-dispatch`.
2. When no pending exists at timer IRQ exit boundary, the RTOS shall log `dispatch-pending=none action=no-dispatch`.
3. When higher-priority READY pending is valid, the RTOS shall continue deferred dispatch through `dispatcher_switch_to(from, to)`.
4. When normal `make run` is executed, the RTOS shall preserve the 10.4 `yield_tsk()` cooperative context switch path.
5. The timer IRQ handler body shall not call `yield_tsk()` or directly call `dispatcher_switch_to()`.

### Requirement 4: documentation、検証証跡、spec成果物
**Objective:** 保守者として、11.4の到達点と未実装範囲をREADME、Doxygen、spec、serial logに残したい。これにより、後続のtime sliceやwakeup連携と責務を混同せずに進められる。

#### Acceptance Criteria
1. When implementation is complete, README shall describe that 11.4 stabilizes preemption event log order, reason strings, and duplicate suppression.
2. When implementation is complete, README shall state that same-priority time slice, round-robin, semaphore wakeup, nested interrupt, and complete interrupt-return-frame switching remain unimplemented.
3. When implementation is complete, meaningful changed interfaces or helpers shall have Japanese Doxygen comments describing purpose, assumptions, limitations, and non-goals.
4. When validation is complete, `docs/logs/qemu-serial.log` shall contain fresh evidence for same-priority no-dispatch and higher-priority deferred dispatch.
5. When validation is complete, `.kiro/specs/preemption-event-log-stabilization/` shall contain only `requirements.md`, `design.md`, and `tasks.md`.
