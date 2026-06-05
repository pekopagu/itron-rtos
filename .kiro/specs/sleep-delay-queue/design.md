# Design Document

## Overview

13.2では、13.1で導入した `dly_tsk(uint32_t delay_ticks)` のdelay WAITING化に、専用のsleep/delay queueを接続する。queueは固定長配列でtask idとremaining tickを保持し、dump/logでtask id、name、remaining、wait reason、stateを観測できるようにする。

この設計はtick処理を導入しない。delay queue上のremaining値は13.2時点では観測用であり、timer IRQ handlerからdecrement、READY復帰、dequeue、dispatcher直接呼び出しは行わない。

## Goals

- delay queueをsemaphore wait queueとは完全に分離して導入する。
- `dly_tsk(delay_ticks > 0)` でdelay WAITING化するtaskをdelay queueへenqueueする。
- enqueue失敗時にWAITING化済みだがqueue未登録のtaskを残さない。
- delay queue dumpでtask id/name/remaining/state/reasonを確認できる。
- 10.4、11.4、12.4、13.1の既存経路を維持する。

## Non-Goals

- tickごとのdelay decrement
- tick到達時READY復帰
- delay queueからのdequeueによるREADY化
- timeout付き `twai_sem`
- `slp_tsk` / `wup_tsk`
- priority順またはdelta queue最適化
- 同一優先度time slice、round-robin
- timer IRQ handlerからのtask APIまたはdispatcher直接呼び出し

## Boundary Commitments

### This Spec Owns

- `kernel/include/delay_queue.h` / `kernel/delay_queue.c` の固定長delay queue管理。
- `delay_queue_init()`、`delay_queue_can_enqueue()`、`delay_queue_enqueue()`、`delay_queue_dump()`。
- `dly_tsk()` のWAITING化前enqueue可否確認、WAITING化後enqueue、dump、`delay-queued-switch` action。
- README、Doxygen、serial log、spec成果物更新。

### Out of Boundary

- timer IRQ handler内のdelay queue操作。
- delay queue entryのdecrement。
- delay満了時のREADY復帰。
- semaphore wait queueの意味変更。
- schedulerのREADY選択規則変更。

### Allowed Dependencies

- `delay_queue.c` はtask情報観測のため `task_get_by_id()` を利用してよい。
- `itron_api.c` はdelay queue APIを呼び、既存の `task_mark_waiting_on_delay()`、`scheduler_select_next()`、`dispatcher_switch_to()` を維持する。
- `kernel.c` は起動時に `delay_queue_init()` を呼んでよい。

### Revalidation Triggers

- `tcb_t` のwait reasonまたはdelay tickフィールドを変更した場合。
- `task_mark_waiting_on_delay()` の契約を変更した場合。
- `scheduler_select_next()` のREADY候補判定を変更した場合。
- semaphore wait queueのenqueue/dequeue契約を変更した場合。
- timer IRQ handlerがtask APIまたはdispatcherを直接呼ぶようになった場合。

## Architecture

### Delay Queue Model

```text
dly_tsk(delay_ticks)
  -> validate current and delay_ticks
  -> delay_queue_can_enqueue(current_id)
  -> task_mark_waiting_on_delay(current_id, delay_ticks)
  -> delay_queue_enqueue(current_id, delay_ticks)
  -> delay_queue_dump()
  -> scheduler_select_next()
  -> dispatcher_switch_to(from, to)
```

`delay_queue_can_enqueue()` は二重enqueueと満杯をWAITING化前に検出する。`delay_queue_enqueue()` は防御として再確認し、対象taskが `TASK_STATE_WAITING` かつ `TASK_WAIT_REASON_DELAY` の場合だけ受け入れる。

満杯時は `dly_tsk()` が `-1` を返し、`action=delay-queue-full` を出す。13.2では戻り値カテゴリを増やさず、ログactionで理由を観測する。

## File Structure Plan

### New Files

- `kernel/include/delay_queue.h`: delay queue公開API、戻り値、Doxygenコメント。
- `kernel/delay_queue.c`: 固定長queue、初期化、可否確認、enqueue、dump、ログ出力。

### Modified Files

- `kernel/itron_api.c`: `dly_tsk()` にWAITING化前のenqueue可否確認、WAITING化後enqueue、dump、`delay-queued-switch` actionを追加する。
- `kernel/kernel.c`: 起動初期化で `delay_queue_init()` を呼ぶ。
- `Makefile`: `delay_queue.c` をビルド対象へ追加する。
- `README.md`: 13.2到達点、Zenn tag候補、未実装範囲を更新する。
- `docs/logs/qemu-serial.log`: fresh `make run` 出力へ更新する。
- `.kiro/specs/sleep-delay-queue/requirements.md`, `design.md`, `tasks.md`: 最終成果物として3ファイルだけ残す。

## Components and Interfaces

| Component | Domain | Intent | Requirements |
| --- | --- | --- | --- |
| DelayQueue | Wait Management | delay WAITING taskの固定長queue管理と観測 | 1, 2, 3, 4 |
| DelayTaskAPI | API Layer | `dly_tsk()` からdelay queueへ接続し、失敗時整合性を守る | 2, 3, 5 |
| DocumentationEvidence | Docs | 13.2到達点と未実装範囲を記録する | 5 |

### DelayQueue API

- `int delay_queue_init(void);`
- `int delay_queue_can_enqueue(int task_id);`
- `int delay_queue_enqueue(int task_id, uint32_t delay_ticks);`
- `void delay_queue_dump(void);`

戻り値は `DELAY_QUEUE_OK`、`DELAY_QUEUE_ERR_INVAL`、`DELAY_QUEUE_ERR_FULL`、`DELAY_QUEUE_ERR_DUPLICATE`、`DELAY_QUEUE_ERR_TASK_STATE` を使う。

## Requirements Traceability

| Requirement | Components | Interfaces |
| --- | --- | --- |
| 1.1, 1.2, 1.3, 1.4 | DelayQueue | `delay_queue_init()`, `delay_queue_dump()` |
| 2.1, 2.2, 2.3, 2.4, 2.5 | DelayQueue, DelayTaskAPI | `delay_queue_can_enqueue()`, `delay_queue_enqueue()`, `dly_tsk()` |
| 3.1, 3.2, 3.3, 3.4, 3.5 | DelayQueue, DelayTaskAPI | enqueue guards |
| 4.1, 4.2, 4.3, 4.4 | DelayQueue, DelayTaskAPI | wait reason checks |
| 5.1, 5.2, 5.3, 5.4, 5.5 | DocumentationEvidence | build/run/timer validation |

## Testing Strategy

- `make` で通常buildが通ることを確認する。
- `make run` でdelay queue enqueue、dump、`delay-queued-switch`、既存semaphore/yield evidenceを確認する。
- `make run VALIDATE_TIMER_IRQ_ENTRY=1` でtimer IRQ preemption / dispatch pending経路が維持されることを確認する。
- `rg` でtimer IRQ handler本体から `dly_tsk()`、`wai_sem()`、`sig_sem()`、`yield_tsk()`、`dispatcher_switch_to()` を直接呼んでいないことを確認する。
- `.kiro/specs/sleep-delay-queue/` が最終的に `requirements.md`、`design.md`、`tasks.md` の3ファイルだけであることを確認する。
