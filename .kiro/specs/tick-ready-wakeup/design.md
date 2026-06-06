# Design Document

## Overview

13.4 では delay queue を timeout 観測だけでなく tick 到達時の READY 復帰境界へ拡張する。`timer_tick()` は system tick を増やした後に `delay_queue_tick()` を呼び、delay queue 側が remaining tick の減算、timeout entry の削除、task READY 復帰、timeout semaphore waiter の semaphore wait queue 削除を担当する。

timer IRQ handler 本体は引き続き `timer_tick()`、IRQ preemption 評価、dispatch pending 観測、interrupt exit boundary に留める。READY 復帰後の高優先度検出は既存の `preemption_evaluate_from_irq()` 経路で行い、直接 `dispatcher_switch_to()` は呼ばない。

## Goals

- delay queue entry の remaining tick を tick ごとに 1 減算する。
- remaining tick が 0 になった delay waiter を WAITING から READY へ戻す。
- remaining tick が 0 になった timeout semaphore waiter を semaphore wait queue から削除し READY へ戻す。
- timeout 到達時に wait reason、wait semaphore id、remaining tick を未待ち状態へ整理する。
- READY 復帰後は既存 scheduler / preemption / dispatch pending 経路で候補化を観測する。

## Non-Goals

- `sig_sem()` 成功時に timeout semaphore waiter を delay queue から削除する処理。
- timeout 発生戻り値を task に伝える本格機構。
- timer IRQ handler 本体からの直接 dispatch。
- priority 順 wait queue、delta queue、time slice、round-robin。
- 既存 RTOS 実装の参照、コピー、流用。

## Boundary Commitments

### This Spec Owns

- `delay_queue_tick()` の public API と実装。
- delay queue entry の remaining tick 減算、timeout 検出、entry 削除。
- timeout 到達 task の READY 復帰 helper 接続。
- timeout semaphore waiter の semaphore wait queue からの task 指定削除。
- `timer_tick()` から delay queue tick 処理への接続。
- 13.4 README、Doxygen、serial log、spec 成果物。

### Out of Boundary

- `sig_sem()` wakeup 成功時の delay queue 側削除。
- interrupt exit boundary の本格 context switch 実装変更。
- `dispatch_pending` の理由 enum 拡張を必須にする変更。
- semaphore wait queue の priority ordering。

### Allowed Dependencies

- `timer.c` は `delay_queue_tick()` を呼んでよい。
- `delay_queue.c` は task table の読み取り、READY 復帰 helper、semaphore wait queue task 指定削除を呼んでよい。
- READY 復帰後の preemption 判定は既存の `preemption_evaluate_from_irq()` が scheduler と dispatch pending を使って行う。
- `semaphore.c` は timeout 時削除のため、対象 task id を指定した wait queue removal API を提供してよい。

### Revalidation Triggers

- `task_wait_reason_t`、TCB wait fields、READY 復帰 helper の契約変更。
- `sem_enqueue_waiter()` / `sem_dequeue_waiter()` の queue 所有権変更。
- `timer_tick()` と IRQ handler の呼び出し順序変更。
- dispatch pending または preemption decision の責務変更。

## Architecture

### Tick Wakeup Flow

```text
timer_tick()
  -> system_ticks++
  -> log [timer] tick: count=<n>
  -> delay_queue_tick()
       -> for each entry:
            remaining before/after log
            if after > 0: keep entry
            if after == 0 and reason=delay:
                task_wake_waiting_on_delay_by_id()
                remove delay entry
            if after == 0 and reason=semaphore-timeout:
                sem_remove_waiter(sem_id, task_id)
                task_wake_waiting_on_sem_timeout_by_id()
                remove delay entry
```

`delay_queue_tick()` は queue を前から走査し、削除時は後続 entry を詰める。削除した index では次の entry が同じ index に移動するため、index を進めずに処理を継続する。

### Timeout Semaphore Removal

`sem_remove_waiter(sem_id, task_id)` を追加する。これは `sig_sem()` の FIFO dequeue とは別に、timeout 到達済み task を task id 指定で wait queue から取り除く。削除後は queue order を保つために既存 queue 内容を一度順序配列として読み出し、対象 task を除いて再構成する。priority 順や timeout policy は扱わない。

### Task State Cleanup

task module に delay timeout と semaphore timeout 用の READY 復帰 helper を追加する。どちらも対象 task が `TASK_STATE_WAITING` かつ期待する `wait_reason` の場合だけ `TASK_STATE_READY` へ戻し、`wait_reason=TASK_WAIT_REASON_NONE`、`wait_sem_id=0`、`delay_ticks_remaining=0` にする。これは task を scheduler 候補へ戻すための状態変更であり、dispatcher switch は行わない。

### Preemption Pending

READY 復帰後、`timer_tick()` は preemption 判定を直接行わない。既存 IRQ handler では `timer_tick()` の直後に `preemption_evaluate_from_irq()` が呼ばれるため、そこで新たに READY になった高優先度 task を検出し、dispatch pending を set する。通常 boot smoke では tick 後に同じ preemption API を明示的に呼び、ログで到達点を観測する。

## File Structure Plan

### Modified Files

- `kernel/include/delay_queue.h`: `delay_queue_tick()` 宣言と 13.4 Doxygen。
- `kernel/delay_queue.c`: tick 減算、timeout 検出、READY 復帰、entry 削除ログ。
- `kernel/include/task.h`: delay timeout / semaphore timeout READY 復帰 helper 宣言。
- `kernel/task.c`: WAITING から READY への timeout 復帰 helper 実装。
- `kernel/include/semaphore.h`: task id 指定の semaphore wait queue removal API 宣言。
- `kernel/semaphore.c`: timeout 到達時の wait queue removal 実装とログ。
- `kernel/timer.c` / `kernel/include/timer.h`: `timer_tick()` から delay queue tick 処理を呼ぶ説明。
- `kernel/kernel.c`: 13.4 boot-time smoke の tick 到達ログ確認を追加。
- `README.md`: 13.4 到達点と tag 候補を追加。
- `docs/logs/qemu-serial.log`: fresh run のログへ更新。
- `.kiro/specs/tick-ready-wakeup/requirements.md`, `design.md`, `tasks.md`: 最終成果物として保持。

## Requirements Traceability

| Requirement | Components | Interfaces |
| --- | --- | --- |
| 1.1, 1.2, 1.3, 1.4, 1.5 | Timer, DelayQueue | `timer_tick()`, `delay_queue_tick()` |
| 2.1, 2.2, 2.3, 2.4, 2.5 | DelayQueue, TaskState | `task_wake_waiting_on_delay_by_id()` |
| 3.1, 3.2, 3.3, 3.4, 3.5 | DelayQueue, SemaphoreWaitQueue, TaskState | `sem_remove_waiter()`, `task_wake_waiting_on_sem_timeout_by_id()` |
| 4.1, 4.2, 4.3, 4.4, 4.5 | Preemption, DispatchPending, TimerIRQ | `preemption_evaluate_from_irq()` |
| 5.1, 5.2, 5.3, 5.4, 5.5 | RuntimeSmoke, DocumentationEvidence | `make`, `make run` |

## Testing Strategy

- `make` で通常 build が通ることを確認する。
- `make run` で delay wakeup、semaphore timeout wakeup、semaphore wait queue removal、preemption pending evidence を確認する。
- `make run VALIDATE_TIMER_IRQ_ENTRY=1` で timer IRQ handler が direct dispatch せず dispatch pending 観測を維持することを確認する。
- `rg` で IRQ handler 本体から `dispatcher_switch_to()`、`twai_sem()`、`dly_tsk()`、`wai_sem()`、`sig_sem()`、`yield_tsk()` を直接呼ばないことを確認する。
- `.kiro/specs/tick-ready-wakeup/` が最終的に `requirements.md`、`design.md`、`tasks.md` の 3 ファイルだけであることを確認する。
