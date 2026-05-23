# Requirements Document

## Introduction

この仕様は、μITRON風RTOSの第9章9.2「dispatcher_switch_to()相当の境界を作る」を扱う。前回9.1では、起動時context switch smokeとして `boot context -> first task context -> second task context -> boot context` の流れを観測できるようになった。しかし、切替開始点は `task_context_switch_to_task_pair()` というboot-time smoke専用APIへ直接依存しており、dispatcher層の正式な責務境界としては見えていない。

今回の変更では、9.1のtask-to-task smokeを維持しつつ、上位層から見える切替開始点をdispatcher層の `dispatcher_switch_to()` 相当のAPIへ寄せる。これはプリエンプション完成ではなく、dispatcherがcurrent commitだけでなく「切替先へ進める境界」を持つことを明確化するための段階的な実装である。

## Boundary Context

- **In scope**: dispatcher層のswitch boundary API追加、9.1 task-to-task smokeのdispatcher境界経由化、入力検証と観測ログ、README/Doxygen/spec/log更新。
- **Out of scope**: interrupt exit boundaryからの呼び出し、dispatch pending消費、timer IRQからの実切替、preemptive context switch、RUNNING/READY状態遷移の正式完成、task終了状態確定、yield_tsk、semaphore wakeup連携、sleep/delay queue、同一優先度タイムスライス、nested interrupt、iretq通常割り込み復帰、APIC/IOAPIC/LAPIC、SMP、μITRON API完成。
- **Adjacent expectations**: schedulerはREADY taskを選ぶだけに留める。dispatcherはcurrent commitとswitch boundaryを担当する。task_context層は実際のarch_context_switchやstack/register contextに近い処理を担当する。arch/x86_64側へscheduler/dispatcher内部を漏らさない。

## Requirements

### Requirement 1: dispatcher switch boundaryの追加
**Objective:** As a RTOS学習実装者, I want dispatcher層にtask context切替へ進む境界を追加したい, so that 切替開始点をdispatcher責務として観測できる

#### Acceptance Criteria

1. When dispatcher switch boundary is invoked with valid source and destination tasks, the RTOS shall log the boundary begin event with both task identities.
2. When dispatcher switch boundary completes, the RTOS shall log the boundary end event with the result code.
3. If dispatcher switch boundary receives invalid task pointers or an invalid task pair, then the RTOS shall reject the request and log the reason.
4. The dispatcher shall expose a public API equivalent to `dispatcher_switch_to(tcb_t *from, tcb_t *to)`.

### Requirement 2: 9.1 task-to-task smokeの維持
**Objective:** As a maintainer, I want 9.1のtask-to-task smoke flowを壊さずdispatcher境界経由に整理したい, so that 既存の観測結果を保ったまま責務境界を前進できる

#### Acceptance Criteria

1. When `make run` is executed, the RTOS shall still observe the flow from boot context to the first task, from the first task to the second task, and back to boot context.
2. When the first task entry returns during the smoke, the RTOS shall still log the task-to-task switch begin event.
3. When the second task context is restored, the RTOS shall still execute the second task entry through the existing trampoline path.
4. The upper kernel smoke coordinator shall invoke the task-to-task smoke through dispatcher switch boundary rather than directly calling the smoke helper API.

### Requirement 3: 非ゴール境界の明示
**Objective:** As a maintainer, I want dispatcher_switch_to()相当の責務と非ゴールを明記したい, so that 後続章のプリエンプションや割り込みexit接続と混同しない

#### Acceptance Criteria

1. The RTOS shall document that `task_context_switch_to_task_pair()` remains a boot-time smoke helper if it is kept.
2. The RTOS shall document that `dispatcher_switch_to()` does not complete formal RUNNING/READY transitions.
3. The RTOS shall document that `dispatcher_switch_to()` does not consume dispatch pending state.
4. The RTOS shall document that `dispatcher_switch_to()` is not called from the timer IRQ handler or interrupt exit boundary in this section.
5. Source comments for the dispatcher switch boundary shall describe what it intentionally does not implement.

### Requirement 4: 検証成果物の更新
**Objective:** As a maintainer, I want build/run/log/spec evidence to reflect 9.2, so that fresh evidence can verify the dispatcher boundary

#### Acceptance Criteria

1. When `make` is executed, the RTOS shall build successfully.
2. When `make run` is executed, the RTOS shall boot through the dispatcher-boundary smoke flow.
3. When `make run VALIDATE_TIMER_IRQ_ENTRY=1` is executed, the timer IRQ validation path shall still run without connecting interrupt exit to dispatcher switching.
4. `docs/logs/qemu-serial.log` shall be updated with 9.2 validation evidence.
5. README shall include a Chapter 9 Section 9.2 summary and the tag candidate `v9.2-dispatcher-switch-boundary`.
