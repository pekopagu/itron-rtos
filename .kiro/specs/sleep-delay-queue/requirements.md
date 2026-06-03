# Requirements Document

## Project Description

μITRON風RTOSを学習目的で段階的に開発する開発者は、13.1で `dly_tsk(delay_ticks)` によるRUNNING current taskのdelay WAITING化を観測できるようになった。しかし現状では、delay待ちtaskをsemaphore wait queueとは独立して管理するsleep/delay queueが存在せず、delay_ticks_remainingをqueue上で観測できない。13.2では、tick decrementやREADY復帰はまだ実装せず、`dly_tsk(delay_ticks > 0)` がWAITING化するtaskを専用delay queueへ登録し、semaphore待ちと混ざらないことを確認できるようにする。

## Boundary Context

- **In scope**: delay queue初期化、enqueue、二重enqueue防御、満杯時の失敗処理、dump/log、`dly_tsk()` からのenqueue接続、README/Doxygen/log/spec更新。
- **Out of scope**: tickごとのdelay decrement、tick到達時READY復帰、delay queueからのdequeueによるREADY化、timeout付き `twai_sem`、`slp_tsk` / `wup_tsk`、priority順delay queue、delta queue、time slice、round-robin、timer IRQ handlerからのtask APIまたはdispatcher直接呼び出し。
- **Adjacent expectations**: 13.1の `dly_tsk()` 入口、12.4のsemaphore wakeup後preemption、10.4の `yield_tsk()`、11.4のtimer IRQ preemption / dispatch pending経路を維持する。

## Requirements

### Requirement 1: delay queue管理の観測

**Objective:** delay WAITING taskをsemaphore wait queueとは独立した固定長queueで観測できるようにする。

#### Acceptance Criteria

1. When kernel initialization runs, the system shall initialize the delay queue to an empty state.
2. When the delay queue is dumped, the system shall log the begin line, current count, each entry, and end line.
3. When a delay queue entry is logged, the system shall expose task id, task name, remaining delay ticks, wait reason, and task state.
4. When no delay waiting task is queued, the system shall keep the delay queue count at zero.

### Requirement 2: `dly_tsk()` からのdelay queue enqueue

**Objective:** `dly_tsk(delay_ticks > 0)` がRUNNING current taskをdelay WAITINGへ落とすとき、そのtaskをdelay queueへ登録する。

#### Acceptance Criteria

1. When `dly_tsk(delay_ticks > 0)` is called with a RUNNING current task, the system shall enqueue that task into the delay queue before completing the delay wait path.
2. When enqueue succeeds, the system shall keep `delay_ticks_remaining` equal to the requested delay tick value.
3. When enqueue succeeds, the system shall log the enqueue with task id, name, delay tick value, and queue count.
4. When enqueue succeeds, the system shall complete `dly_tsk()` with action `delay-queued-switch` if the dispatcher switch succeeds.
5. When `delay_ticks == 0`, the system shall preserve the 13.1 invalid-delay behavior and shall not enqueue anything.

### Requirement 3: delay queueの防御と失敗時整合性

**Objective:** delay queue登録失敗時に不整合なWAITING taskを残さない。

#### Acceptance Criteria

1. When the delay queue is full, the system shall reject enqueue and log `reason=full`.
2. When the same task is already present in the delay queue, the system shall reject duplicate enqueue and log `reason=duplicate`.
3. When enqueue cannot succeed, `dly_tsk()` shall return `-1` before changing the current task to WAITING.
4. When enqueue cannot succeed, `dly_tsk()` shall log completed action `delay-queue-full` or an equivalent enqueue-failure action.
5. When a task is accepted into the delay queue, the system shall accept only a task whose wait reason is delay.

### Requirement 4: semaphore待ちとの分離

**Objective:** delay queueとsemaphore wait queueが相互にtaskを混入させないことを維持する。

#### Acceptance Criteria

1. When `wai_sem()` enqueues a task, the system shall enqueue only semaphore waiting tasks into the semaphore wait queue.
2. When `dly_tsk()` enqueues a task, the system shall enqueue only delay waiting tasks into the delay queue.
3. When `sig_sem()` wakes a semaphore waiter, the system shall not wake any task that is present only in the delay queue.
4. When a delay waiting task is queued, the system shall not use `wait_sem_id` as delay queue metadata.

### Requirement 5: 既存経路と未実装範囲の維持

**Objective:** 13.2のqueue導入で既存のyield、semaphore、timer IRQ経路を壊さない。

#### Acceptance Criteria

1. When normal build is executed, the system shall build successfully.
2. When normal `make run` is executed, the system shall show delay queue enqueue and dump evidence.
3. When `make run VALIDATE_TIMER_IRQ_ENTRY=1` is executed, the system shall preserve timer IRQ preemption / dispatch pending behavior.
4. While timer IRQ handler body runs, the system shall not call `dly_tsk()`, `wai_sem()`, `sig_sem()`, `yield_tsk()`, or `dispatcher_switch_to()` directly.
5. When implementation is complete, README, Doxygen comments, serial log, and spec artifacts shall describe that tick decrement, tick-reached READY return, and timeout `twai_sem` remain unimplemented.
