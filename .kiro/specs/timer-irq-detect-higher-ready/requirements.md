# Requirements Document

## Introduction

第11章11.1では、timer IRQ後のpreemption判定で、現在RUNNING中のtaskより高優先度のREADY taskが存在するかを検出する。対象ユーザーは、μITRON風RTOSを段階的に学習実装している開発者である。既存のtimer IRQ pathはtick更新、preemption decision、dispatch pending観測、interrupt exit boundary観測まで到達しているが、今回の到達点は「高優先度READYを検出し、dispatch pending要求として観測する」ことであり、実dispatchやpreemptive context switchはまだ行わない。

## Boundary Context

- **In scope**: timer IRQ後のcurrent RUNNING task取得、高優先度READY task検出、候補taskのid/name/prio/stateログ、dispatch pending request/observe、no-switch/invalid-currentログ、README/Doxygen/spec/log更新。
- **Out of scope**: timer IRQ handlerからの`yield_tsk()`呼び出し、timer IRQ handlerからの`dispatcher_switch_to()`呼び出し、interrupt exit boundaryからの実dispatch接続、dispatch pending消費、preemptive context switch、同一優先度time slice、semaphore wakeup連携、sleep/delay queue、他μITRON風API完成、nested interrupt、APIC/IOAPIC/LAPIC、SMP。
- **Adjacent expectations**: 10.4の`yield_tsk()`協調context switch経路、9.1から9.4のcontext switch smoke、10.1から10.4のyieldログ、8.4のtimer IRQ exit boundary観測を維持する。

## Requirements

### Requirement 1: 高優先度READY検出
**Objective:** As a RTOS学習者, I want timer IRQ後に現在RUNNING中のtaskより高優先度のREADY taskを検出できる, so that preemptive dispatch前段の判断根拠を観測できる

#### Acceptance Criteria
1. When timer IRQ後のpreemption判定が実行される, the RTOS shall read the current task from the dispatcher boundary without changing the current task.
2. When the current task exists and is RUNNING, the RTOS shall log the current task id, name, priority, and state.
3. When a READY task has a numerically smaller priority value than the current RUNNING task, the RTOS shall log that task as a higher-ready candidate with id, name, priority, and state.
4. When a READY task has the same priority value as the current RUNNING task and no higher-priority READY task exists, the RTOS shall not treat that task as a preemption target.
5. If the current task does not exist or is not RUNNING, then the RTOS shall evaluate the result as no-switch with an invalid-current or no-current reason.

### Requirement 2: dispatch pending要求への接続
**Objective:** As a maintainer, I want 高優先度READY検出結果をdispatch pending要求へ接続できる, so that interrupt exit dispatchへ接続する前にpending状態を観測できる

#### Acceptance Criteria
1. When a higher-priority READY task is detected, the RTOS shall request dispatch pending with the reason `higher-priority-ready`.
2. When dispatch pending is requested, the RTOS shall log the source current task and destination candidate task by id and name.
3. When no higher-priority READY task is detected, the RTOS shall not request dispatch pending.
4. When dispatch pending is not requested, the RTOS shall log the decision reason.
5. When the timer IRQ exit boundary observes dispatch pending as requested, the RTOS shall keep the action as `not-dispatched-yet`.

### Requirement 3: 非切替境界の維持
**Objective:** As a RTOS学習者, I want 11.1を検出とpending観測に限定する, so that cooperative switch and future preemptive switch remain clearly separated

#### Acceptance Criteria
1. The timer IRQ handler shall not call `yield_tsk()`.
2. The timer IRQ handler shall not call `dispatcher_switch_to()`.
3. The timer IRQ handler shall not consume dispatch pending.
4. The interrupt exit boundary shall not dispatch a task or change task states.
5. The existing `yield_tsk()` cooperative context switch path shall continue to reach the dispatcher/context switch boundary during normal `make run`.

### Requirement 4: 検証証跡と文書更新
**Objective:** As a maintainer, I want 11.1の到達点を文書とログで確認できる, so that review can distinguish detection from real switching

#### Acceptance Criteria
1. When `make` is executed, the RTOS shall build successfully.
2. When `make run` is executed, the RTOS shall preserve the existing 9.1-9.4 and 10.1-10.4 smoke logs.
3. When `make run VALIDATE_TIMER_IRQ_ENTRY=1` is executed, the RTOS shall enter the timer IRQ path and log higher-priority READY detection and dispatch pending requested when the validation state contains a lower-priority current and a higher-priority READY task.
4. When no higher-priority READY task exists, the RTOS shall log no-switch and dispatch pending not-requested.
5. The README and Doxygen comments shall state that 11.1 detects higher-priority READY after timer IRQ and requests dispatch pending, but does not connect to real dispatch, pending consumption, or preemptive context switch.
6. The `docs/logs/qemu-serial.log` artifact shall be updated with fresh validation evidence.
7. The spec directory for this feature shall contain only `requirements.md`, `design.md`, and `tasks.md` after implementation validation is complete.
