# Design Document

## Overview

第9章9.4は、task entry関数がreturnした後のtask lifecycleを確定する小さな拡張である。9.3までに`dispatcher_switch_to(from, to)`はswitch boundaryの中で`from RUNNING->READY`、`to READY->RUNNING`を行うようになっている。9.4ではこのdispatcher責務を維持したまま、`task_context_enter()`のentry return観測点で対象taskを`TASK_STATE_DORMANT`へ遷移させる。

この変更はプリエンプション完成ではない。dispatch pending消費、interrupt exit boundaryからの実dispatch、timer IRQからのcontext switch、μITRON風task lifecycle APIは対象外にする。

## Boundary Commitments

### Owned by This Spec

- `task_context`層でentry returnを観測し、対象taskの起動分完了として`TASK_STATE_DORMANT`へ最終化する。
- entry return時のログを`returned`と`finalized`に分け、状態遷移を明示する。
- task管理層にDORMANT最終化用の小さな状態変更APIを追加する。
- README、Doxygenコメント、QEMU serial log、spec成果物を9.4の到達点へ更新する。

### Out of Boundary

- dispatch pending消費
- interrupt exit boundaryからの`dispatcher_switch_to()`呼び出し
- timer IRQからの実切替
- preemptive context switch
- `yield_tsk`、`sta_tsk`、`ext_tsk`、`exd_tsk`
- task再起動API
- semaphore wakeup連携、sleep/delay queue、time slice、nested interrupt、APIC/IOAPIC/LAPIC、SMP

### Allowed Dependencies

- `task_context.c`はTCBの状態確定のために`task.h`のtask状態APIへ依存してよい。
- `dispatcher.c`は既存どおり`task_context_switch_to_task_pair()`へ実切替smokeを委譲する。
- `arch/x86_64`側はscheduler/dispatcher内部を参照しない。
- kernel common側はPIC、vector番号、I/O port、entry stub詳細を参照しない。

### Revalidation Triggers

- `dispatcher_switch_to()`の状態遷移順序を変えた場合は9.2/9.3を再検証する。
- timer IRQ handler、interrupt exit boundary、dispatch pending消費に触れた場合は8.3/8.4を再検証する。
- schedulerの選択条件をREADY以外へ広げた場合は3.2/6.3を再検証する。

## Architecture

```text
scheduler
  selects READY only
      |
      v
dispatcher_switch_to(from, to)
  owns switch boundary, current commit,
  from RUNNING->READY, to READY->RUNNING
      |
      v
task_context_switch_to_task_pair(first, second)
  owns stack/register context smoke path
      |
      v
task_context_enter(task)
  observes entry return
  finalizes returned task to DORMANT
```

## Components and Interfaces

### task management

`task_mark_dormant_from_entry_return(int task_id)`を追加する。対象taskが`RUNNING`または`READY`の場合だけ`TASK_STATE_DORMANT`へ遷移させる。`READY`を許す理由は、9.3の`dispatcher_switch_to()`がtask-to-task smoke開始前に`from RUNNING->READY`を行うためである。これはREADY復帰を継続する意味ではなく、entry return観測後に最終的にDORMANTへ落とすための9.4限定の受け口である。

### task_context

`task_context_enter()`はentry return直後に次を行う。

1. `[context] task entry returned: task id=... name=...`を出す。
2. 対象taskの現在状態を保存する。
3. task管理APIでDORMANTへ遷移させる。
4. `[context] task entry finalized: task id=... name=... <before>->DORMANT`を出す。
5. 既存9.1 smokeのfirst->second切替を維持する。

task_context層はdispatcher current pointer、scheduler選択、dispatch pending、interrupt exitには触れない。

### dispatcher

`dispatcher_switch_to()`の9.3責務は維持する。コメントだけ9.4の最終DORMANT化がtask_context層で行われることへ更新する。

## File Structure Plan

- `kernel/include/task.h`: DORMANT最終化APIの宣言とDoxygenコメントを追加し、`task_state_t`の説明を9.4へ更新する。
- `kernel/task.c`: `task_mark_dormant_from_entry_return()`を追加する。
- `kernel/task_context.c`: entry return観測点でDORMANT最終化ログと状態遷移を追加し、9.1 smokeの切替を維持する。
- `kernel/dispatcher.c`: 9.3状態遷移コメントを9.4の境界説明へ更新する。
- `README.md`: 第9章9.4の進捗、Zenn tag候補、未実装範囲を更新する。
- `docs/logs/qemu-serial.log`: `make run`結果で更新する。
- `.kiro/specs/task-entry-return-finalization/requirements.md`: 要件。
- `.kiro/specs/task-entry-return-finalization/design.md`: 設計。
- `.kiro/specs/task-entry-return-finalization/tasks.md`: 実装タスク。

## Testing Strategy

- `make`で通常buildが通ることを確認する。
- `make run`で9.1のtask_bからtask_cへのsmoke、9.2のswitch boundary、9.3のRUNNING/READY遷移、9.4のDORMANT最終化ログを確認する。
- `make run VALIDATE_TIMER_IRQ_ENTRY=1`でtimer IRQ validation pathが壊れておらず、timer IRQからdispatcher実切替へ接続されていないことを確認する。
- コード確認でdispatch pending消費、interrupt exitからの実dispatch、μITRON風API完成が追加されていないことを確認する。
