# Requirements Document

## Introduction

この仕様は、μITRON風RTOSの第9章9.1「起動時の切替smokeをタスク間切替へ拡張」を扱う。対象ユーザーは、RTOSを段階的に学習実装している開発者である。現在は第5章5.3で boot context から1つのtask contextへ切り替え、task entry return後にbootへ戻る最小smokeがあるが、task context同士の切替はまだ観測できない。

今回9.1では、既存の起動時smokeを拡張し、boot context上で準備した複数taskのうち、最初のtask stack上から次のtask contextへ一度だけ切り替える。これは第9章以降の実dispatcher接続へ進むための観測モデルであり、まだ割り込み復帰時切替、yield API、プリエンプション、通常のtask lifecycle完成は行わない。

## Boundary Context

- **In scope**: 起動時context switch smokeの拡張、複数taskの初期stack frame準備、task contextから別task contextへの一度きりの切替観測、ログ/README/specの更新、QEMU serial log更新。
- **Out of scope**: `dispatcher_switch_to()`相当の正式境界、RUNNING/READY遷移の実切替統合、entry return時の最終仕様確定、割り込みexitからのdispatch pending消費、yield API、プリエンプション、タイムスライス、sleep/delay queue、semaphore wakeup連携、μITRON API。
- **Adjacent expectations**: schedulerはREADY task選択、dispatcherはcurrent commit、task context層は準備済みcontextの検証とarch primitive呼び出しに責務を分ける。今回のsmokeは教育用の起動時検証であり、実運用のタスク切替機構ではない。

## Requirements

### Requirement 1: 既存boot-to-task context switch smokeの維持
**Objective:** As a RTOS学習者, I want 既存のboot contextからtask contextへのsmokeを維持したい, so that 第5章までの到達点を壊さず9.1の拡張を確認できる

#### Acceptance Criteria

1. When `make run` is executed, the RTOS shall still prepare an initial task stack frame before entering a task context.
2. When the boot-to-task switch starts, the RTOS shall log the source as boot and the selected task as the destination.
3. When the smoke returns to boot, the RTOS shall log that boot context resumed.

### Requirement 2: task-to-task切替smokeの追加
**Objective:** As a RTOS学習者, I want 起動時smoke内でtask contextから別task contextへ切り替わる様子を観測したい, so that 第9章のtask間context switchの入口を確認できる

#### Acceptance Criteria

1. When at least two runnable sample tasks exist, the RTOS shall prepare initial stack frames for two distinct tasks before the task-to-task switch smoke.
2. When the first task entry returns during the smoke, the RTOS shall switch from the first task context to the second task context instead of immediately returning to boot.
3. When the second task context is restored, the RTOS shall execute the second task entry through the existing trampoline path.
4. When the second task entry returns, the RTOS shall return to boot context and complete the smoke.
5. The RTOS shall log the task-to-task switch begin event with both source and destination task identities.

### Requirement 3: 責務境界と非ゴールの維持
**Objective:** As a maintainer, I want 9.1の切替smokeを正式dispatcherや割り込み切替と混同しないようにしたい, so that 後続章で境界を段階的に置き換えられる

#### Acceptance Criteria

1. The RTOS shall document that 9.1 is a boot-time smoke model and not a complete task switching subsystem.
2. The task-to-task smoke shall not consume dispatch pending state.
3. The task-to-task smoke shall not be invoked from the timer IRQ handler or interrupt exit boundary.
4. The task-to-task smoke shall not add yield API, time slicing, semaphore wakeup dispatch, or interrupt-driven scheduling.
5. Source comments for the new task-to-task boundary shall describe what it intentionally does not implement.

### Requirement 4: 検証成果物の更新
**Objective:** As a maintainer, I want build/run/log/spec evidence to reflect 9.1, so that fresh evidence can verify the task-to-task smoke extension

#### Acceptance Criteria

1. When `make` is executed, the RTOS shall build successfully.
2. When `make run` is executed, the RTOS shall boot through the extended smoke flow.
3. The serial log shall include evidence of the task-to-task switch begin event and the second task entry execution.
4. `docs/logs/qemu-serial.log` shall be updated with 9.1 validation evidence.
5. README shall include a Chapter 9 Section 9.1 summary and a Zenn tag candidate.
