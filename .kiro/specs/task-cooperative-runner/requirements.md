# Requirements Document

## Introduction
このfeatureは、第4章4.3「協調的な実行制御」として、複数のREADY taskをboot-time verification model上で順番に実行対象へ進め、entry call、entry body、cooperative return、次task選択の流れをQEMUシリアルログで観測可能にする。

4.1ではcurrent taskのentryを通常のC関数呼び出しとして1回だけ呼び、4.2ではentry returnを正式なtask終了ではなく観測イベントとして扱った。4.3ではこのモデルを拡張し、entry returnをcooperative return eventとして扱い、必要に応じてRUNNING taskをREADYへ戻してschedulerの再選択対象にする。これは第5章のcontext switch based executionへ置き換えるための一時的な検証モデルであり、RUNNINGはまだCPU実行中ではなく、currentとして採用された論理状態である。

## Boundary Context
- **In scope**: boot-time verification modelとしての協調実行、複数READY taskの順番選択、dispatcher commit済みcurrent taskのentry直接呼び出し、cooperative return event、RUNNINGからREADYへの再実行候補化、scheduler再実行、実行回数上限、QEMUシリアルログでの実行順序観測。
- **Out of scope**: 本物のcontext switch、task stack切り替え、CPU register save/restore、assembler実装、interrupt、timer、preemption、μITRON互換APIとしてのyield_tsk、正式なtask終了API、TASK_STATE_EXITED追加、DORMANT遷移、task restart、既存RTOS実装の参照・コピー・流用。
- **Adjacent expectations**: schedulerはREADY task選択だけを維持し、dispatcherはselected taskをcurrentとしてcommitする境界を維持し、task管理はTCB状態の管理に限定される。entry呼び出し対象はselected taskそのものではなく、dispatcherから取得したcurrent taskである。

## Requirements

### Requirement 1: 協調実行モデルの開始条件
**Objective:** As a kernel開発者, I want 複数READY taskを起動時検証の実行対象として順番に進めたい, so that 第5章前に協調的な実行制御の観測モデルを確認できる

#### Acceptance Criteria
1. When cooperative execution verification begins, the task-cooperative-runner feature shall treat READY tasks as candidates for cooperative entry execution.
2. When cooperative execution verification begins, the task-cooperative-runner feature shall not require any task to already be executing on a CPU context.
3. While cooperative execution verification is active, the task-cooperative-runner feature shall treat the model as boot-time verification behavior.
4. While cooperative execution verification is active, the task-cooperative-runner feature shall preserve RUNNING as a logical current-adopted state.
5. The task-cooperative-runner feature shall allow more than one task entry to become observable during one boot-time verification run.

### Requirement 2: selected taskとcurrent taskの関係
**Objective:** As a kernel開発者, I want scheduler選択とdispatcher確定を分けて扱いたい, so that entry呼び出し対象がcurrent taskであることを明確に検証できる

#### Acceptance Criteria
1. When the scheduler selects a READY task, the task-cooperative-runner feature shall treat the selected task as a commit candidate.
2. When a selected task is available, the task-cooperative-runner feature shall require dispatcher commit before that task entry is eligible for cooperative entry execution.
3. When dispatcher commit succeeds, the task-cooperative-runner feature shall use the committed current task as the cooperative entry execution target.
4. When cooperative entry execution is about to call an entry, the task-cooperative-runner feature shall obtain the call target from dispatcher current observation.
5. When cooperative entry execution is about to call an entry, the task-cooperative-runner feature shall not call the selected task directly without confirming it as current.
6. If dispatcher commit fails, then the task-cooperative-runner feature shall not call the selected task entry.

### Requirement 3: current task entryの直接呼び出し
**Objective:** As a kernel開発者, I want current taskのentryを通常のC関数呼び出しとして観測したい, so that context switch導入前にentry実行順序を最小モデルで確認できる

#### Acceptance Criteria
1. When the current task is eligible for cooperative entry execution, the task-cooperative-runner feature shall call the current task entry as a normal C function call.
2. When the current task entry is called, the task-cooperative-runner feature shall call the entry once for that cooperative execution step.
3. When the current task entry body emits its own log, the task-cooperative-runner feature shall allow that entry body log to appear in the QEMU serial stream.
4. If the current task is missing, then the task-cooperative-runner feature shall not call any task entry.
5. If the current task is not in RUNNING state, then the task-cooperative-runner feature shall not call that task entry.
6. If the current task entry is missing, then the task-cooperative-runner feature shall not call that task entry.

### Requirement 4: cooperative return event
**Objective:** As a kernel開発者, I want entry returnを正式終了ではなく協調的なreturn eventとして扱いたい, so that 第5章前にyield-likeな観測点だけを追加できる

#### Acceptance Criteria
1. When a cooperative entry call returns, the task-cooperative-runner feature shall treat the return as a cooperative return event.
2. When a cooperative entry call returns, the task-cooperative-runner feature shall not treat the return as formal task termination.
3. When a cooperative entry call returns, the task-cooperative-runner feature shall not require a task termination API.
4. When a cooperative entry call returns, the task-cooperative-runner feature shall not require a TASK_STATE_EXITED state.
5. When a cooperative entry call returns, the task-cooperative-runner feature shall not require a DORMANT transition.
6. When a cooperative return event is observed, the task-cooperative-runner feature shall make the event visible in the QEMU serial stream.

### Requirement 5: RUNNINGからREADYへの再実行候補化
**Objective:** As a kernel開発者, I want cooperative return後のtaskを再びscheduler候補に戻せるようにしたい, so that 複数taskを有限の起動時検証loopで順番に観測できる

#### Acceptance Criteria
1. When a cooperative return event has been observed, the task-cooperative-runner feature shall allow the returned RUNNING task to become READY again.
2. When a returned task becomes READY again, the task-cooperative-runner feature shall treat that transition as cooperative re-candidacy rather than formal task restart.
3. When a returned task becomes READY again, the task-cooperative-runner feature shall allow the scheduler to consider that task in a later selection.
4. If the returned task cannot become READY again, then the task-cooperative-runner feature shall make the failure observable and shall not continue as if re-candidacy succeeded.
5. While cooperative execution verification is active, the task-cooperative-runner feature shall not redefine RUNNING as proof of continuous CPU execution.
6. While cooperative execution verification is active, the task-cooperative-runner feature shall not redefine READY re-candidacy as task restart.

### Requirement 6: scheduler再実行と次task選択
**Objective:** As a kernel開発者, I want cooperative return後にschedulerを再実行したい, so that 次のREADY taskへ進む流れを観測できる

#### Acceptance Criteria
1. When cooperative return handling completes successfully, the task-cooperative-runner feature shall allow scheduler selection to run again.
2. When scheduler selection runs again, the task-cooperative-runner feature shall select from READY tasks.
3. When a next READY task is selected, the task-cooperative-runner feature shall require dispatcher commit before its entry is called.
4. When no READY task is available, the task-cooperative-runner feature shall end cooperative execution verification without calling another task entry.
5. While scheduler selection is reused by cooperative execution verification, the task-cooperative-runner feature shall preserve scheduler behavior as READY task selection.
6. While scheduler selection is reused by cooperative execution verification, the task-cooperative-runner feature shall not require scheduler behavior to call entries, change task states, or emit HAL console output.

### Requirement 7: 実行回数上限と停止条件
**Objective:** As a kernel開発者, I want cooperative entry呼び出し回数に上限を設けたい, so that 起動時検証が無限に続かないことを保証できる

#### Acceptance Criteria
1. The task-cooperative-runner feature shall define a finite upper bound for cooperative entry calls in one boot-time verification run.
2. When the cooperative entry call count reaches the configured upper bound, the task-cooperative-runner feature shall stop calling additional task entries.
3. When the cooperative entry call count reaches the configured upper bound, the task-cooperative-runner feature shall make the limit stop observable in the QEMU serial stream.
4. When cooperative execution verification stops because no READY task exists, the task-cooperative-runner feature shall make the no-ready stop observable in the QEMU serial stream.
5. If cooperative execution cannot safely continue after a failed precondition, then the task-cooperative-runner feature shall stop the verification flow without unbounded retry.
6. While cooperative execution verification is active, the task-cooperative-runner feature shall not depend on timer, interrupt, or preemption to stop the verification flow.

### Requirement 8: QEMUシリアルログでの順序観測
**Objective:** As a kernel開発者, I want 協調実行の順序をQEMUシリアルログで確認したい, so that selected-current-entry-return-next selectionの流れを追跡できる

#### Acceptance Criteria
1. When a task is selected for cooperative execution, the task-cooperative-runner feature shall make the selection observable in the QEMU serial stream.
2. When a task is committed as current, the task-cooperative-runner feature shall make the current commit observable in the QEMU serial stream.
3. When a current task entry is about to be called, the task-cooperative-runner feature shall make the entry call observable in the QEMU serial stream.
4. When a current task entry returns, the task-cooperative-runner feature shall make the cooperative return observable in the QEMU serial stream.
5. When cooperative execution advances to another scheduler selection, the task-cooperative-runner feature shall make the next selection point observable in the QEMU serial stream.
6. When QEMU is run with serial output enabled, the task-cooperative-runner feature shall allow entry call, entry body, cooperative return, and next selection order to be verified from the serial stream.

### Requirement 9: 責務分離の維持
**Objective:** As a kernel開発者, I want scheduler、dispatcher、task管理の既存責務を混ぜずに協調実行を観測したい, so that 第5章で実行制御を置き換えやすい境界を保てる

#### Acceptance Criteria
1. While cooperative execution verification is active, the task-cooperative-runner feature shall preserve scheduler responsibility as READY task selection.
2. While cooperative execution verification is active, the task-cooperative-runner feature shall preserve dispatcher responsibility as current task commit and current observation.
3. While cooperative execution verification is active, the task-cooperative-runner feature shall preserve task management responsibility as TCB and task state management.
4. While cooperative execution verification is active, the task-cooperative-runner feature shall not require scheduler behavior to perform dispatcher commit.
5. While cooperative execution verification is active, the task-cooperative-runner feature shall not require dispatcher behavior to call task entries.
6. While cooperative execution verification is active, the task-cooperative-runner feature shall not require task management behavior to run cooperative execution loops.
7. While cooperative execution verification is active, the task-cooperative-runner feature shall not require a public μITRON-compatible yield API.

### Requirement 10: 第5章への置き換え可能性
**Objective:** As a kernel開発者, I want 4.3の協調実行を一時的な検証モデルとして制約したい, so that 第5章のcontext switch based executionへ自然に置き換えられる

#### Acceptance Criteria
1. The task-cooperative-runner feature shall describe cooperative execution as a temporary boot-time verification model.
2. The task-cooperative-runner feature shall preserve the current task entry as the observable execution target before future context-switch-based execution.
3. The task-cooperative-runner feature shall preserve task stack information as future context setup input without using it for stack switching in 4.3.
4. The task-cooperative-runner feature shall not implement real context switching.
5. The task-cooperative-runner feature shall not switch task stacks.
6. The task-cooperative-runner feature shall not save or restore CPU registers.
7. The task-cooperative-runner feature shall not add assembler implementation for cooperative execution.
8. The task-cooperative-runner feature shall not implement interrupt-driven execution.
9. The task-cooperative-runner feature shall not implement timer-driven execution.
10. The task-cooperative-runner feature shall not implement preemption.

### Requirement 11: 文書化とコメント方針
**Objective:** As a kernel開発者, I want 協調実行モデルの意図と非目標を文書化したい, so that 学習用RTOSとして設計上の境界を後続章へ引き継げる

#### Acceptance Criteria
1. Where cooperative execution behavior is documented, the task-cooperative-runner feature shall state that the model is boot-time verification behavior.
2. Where cooperative entry calling is documented, the task-cooperative-runner feature shall state that entries are called as normal C function calls.
3. Where cooperative return handling is documented, the task-cooperative-runner feature shall state that cooperative return is not formal task termination.
4. Where RUNNING state is documented for 4.3, the task-cooperative-runner feature shall state that RUNNING is logical current adoption rather than CPU execution.
5. Where READY re-candidacy is documented for 4.3, the task-cooperative-runner feature shall state that it is not task restart.
6. Where public or internal helpers are added or changed for cooperative execution, the task-cooperative-runner feature shall provide Doxygen-style comments when meaningful.
7. Where source comments describe cooperative execution, the task-cooperative-runner feature shall identify that context switch, stack switch, register save/restore, interrupt, timer, and preemption are not performed.
8. The task-cooperative-runner feature shall not refer to, copy, adapt, or translate implementation code or structure from existing RTOS implementations.
