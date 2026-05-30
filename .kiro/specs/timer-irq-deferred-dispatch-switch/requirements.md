# Requirements Document

## Project Description (Input)

第11章11.2では、11.1で timer IRQ 後に request できるようになった dispatch pending を、安全な後段 dispatch boundary で一度だけ consume し、既存の task-to-task `dispatcher_switch_to(from, to)` 経路へ接続する。対象ユーザーは μITRON 風 RTOS を段階的に学習実装している開発者であり、現在は timer IRQ handler が高優先度 READY を検出して pending を観測できるが、pending を消費して切替境界へ進む処理はまだ存在しない。今回、timer IRQ handler 本体から `yield_tsk()` や `dispatcher_switch_to()` を直接呼ばず、interrupt exit boundary または後段 dispatch boundary で妥当な from/to だけを切替へ渡す。

## Requirements

### Requirement 1: dispatch pending の from/to 保持と consume

**Objective:** RTOS 学習者として、timer IRQ で request された dispatch pending の from/to を後段境界で取り出せるようにしたい。これにより、11.1 の検出結果を実際の dispatcher 境界へ安全に接続できる。

#### Acceptance Criteria

1. When higher-priority READY task is detected from timer IRQ, the RTOS shall keep requested from/to task identity in dispatch pending state.
2. When dispatch pending is requested, the RTOS shall expose a consume API that returns the pending reason and from/to task identity once.
3. When dispatch pending is not requested, the RTOS shall not return a switch target and shall log `consume skipped: reason=no-pending`.
4. If consume processing completes a valid dispatch attempt, then the RTOS shall clear dispatch pending and log `cleared: reason=dispatch-completed`.
5. If consume detects invalid pending data, then the RTOS shall clear dispatch pending and log `cleared: reason=invalid-pending`.

### Requirement 2: 後段 dispatch boundary での妥当性確認

**Objective:** RTOS メンテナとして、interrupt exit boundary で不正な pending を直接切替へ流さないようにしたい。これにより、task 状態の破壊や二重 dispatch を避けられる。

#### Acceptance Criteria

1. When dispatch pending is consumed, the RTOS shall re-acquire mutable TCBs by from/to task id before calling `dispatcher_switch_to()`.
2. When from task is missing or not RUNNING, the RTOS shall reject the pending request and shall not switch.
3. When to task is missing or not READY, the RTOS shall reject the pending request and shall not switch.
4. When from task is RUNNING and to task is READY, the RTOS shall call `dispatcher_switch_to(from, to)` exactly through the deferred boundary.
5. When the pending from/to are invalid, the RTOS shall log `consume rejected: reason=invalid-from-or-to`.

### Requirement 3: timer IRQ handler の責務分離

**Objective:** RTOS 学習者として、timer IRQ handler 本体を tick、preemption decision、pending request/observation、exit boundary 呼び出しに限定したい。これにより、割り込み入口と dispatcher/context switch の責務を混同しない。

#### Acceptance Criteria

1. The timer IRQ handler body shall not call `yield_tsk()`.
2. The timer IRQ handler body shall not directly call `dispatcher_switch_to()`.
3. When timer IRQ exit boundary sees requested pending, the RTOS shall log `dispatch-pending=requested action=deferred-dispatch`.
4. When timer IRQ exit boundary sees no pending, the RTOS shall log `dispatch-pending=none action=no-dispatch`.
5. The existing 10.4 `yield_tsk()` cooperative context switch path shall continue to reach dispatcher/context switch during normal `make run`.

### Requirement 4: 優先度判断と既存ログの維持

**Objective:** RTOS 学習者として、11.1 の高優先度 READY 検出ログを維持したまま 11.2 の後段切替を観測したい。これにより、検出、pending request、pending consume、dispatcher switch の流れをログから追跡できる。

#### Acceptance Criteria

1. When current RUNNING has a lower priority than a READY task, the RTOS shall log `[preempt-irq] higher-ready detected`.
2. When only same-priority READY tasks exist, the RTOS shall not request preemption for time slicing.
3. When a valid pending request is consumed, the RTOS shall log dispatcher state transitions `RUNNING->READY` and `READY->RUNNING`.
4. When `make run VALIDATE_TIMER_IRQ_ENTRY=1` is executed, the RTOS shall show pending request, exit boundary deferred dispatch, consumed pending, dispatcher switch, context switch, cleared pending, and EOI logs.
5. The 9.1-9.4 context switch smoke logs shall remain observable in normal `make run`.

### Requirement 5: 文書、検証証跡、spec 成果物

**Objective:** メンテナとして、11.2 の到達点と未実装範囲を README、Doxygen、serial log、spec に残したい。これにより、次章以降の time slice、semaphore wakeup、nested interrupt、完全な割り込み復帰 frame 切替と混同しない。

#### Acceptance Criteria

1. When implementation is complete, README shall describe that 11.2 consumes dispatch pending at a deferred boundary and connects to dispatcher/context switch.
2. When implementation is complete, README shall state that same-priority time slice, semaphore wakeup, nested interrupt, and full interrupt-return-frame switching are not implemented.
3. When implementation is complete, public or meaningful internal interfaces shall have Japanese Doxygen comments describing purpose, assumptions, limitations, and non-goals.
4. When validation is complete, `docs/logs/qemu-serial.log` shall contain fresh validation evidence.
5. When validation is complete, `.kiro/specs/timer-irq-deferred-dispatch-switch/` shall contain only `requirements.md`, `design.md`, and `tasks.md`.
