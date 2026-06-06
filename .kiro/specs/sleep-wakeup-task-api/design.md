# Design Document

## Overview

14.2では μITRON 風API層に `slp_tsk()` / `wup_tsk(tskid)` を追加し、sleep待ちだけを扱う最小のtask wait APIを実装する。`slp_tsk()` はdispatcherが保持するcurrent RUNNING taskだけをWAITING(sleep)へ遷移させ、既存schedulerで次READY taskを選択し、既存dispatcher境界へ渡す。`wup_tsk()` は対象taskがWAITINGかつsleep理由の場合だけREADYへ戻し、READY化したtaskがcurrentより高優先度なら既存のdispatch pending境界へ接続する。

## Boundary Commitments

- **Owned by this spec**: `slp_tsk()` / `wup_tsk()` の公開宣言、戻り値定義、APIログ、sleep wait reason、task moduleのRUNNING->WAITING(sleep) helper、WAITING(sleep)->READY helper、wakeup由来のdispatch pending reason、14.2 smokeログ、README/Doxygen/log/spec更新。
- **Not owned by this spec**: timeout付きsleep、wakeup要求カウント蓄積、sleep wait queue、priority順wait queue、round-robin、timer IRQ handlerからのtask API呼び出し、timer IRQ handler本体からの直接dispatch。
- **Allowed dependencies**: `itron_api.c` は `dispatcher_get_current()`、`scheduler_select_next()`、`dispatcher_switch_to()`、task module helper、dispatch pending request helperを使う。task moduleはTCB状態とwait metadataだけを変更する。
- **Revalidation triggers**: task state enum、wait reason enum、dispatcher switch受け入れ状態、dispatch pending reason、timer IRQ entry/exit境界を変更した場合は、yield/semaphore/delay/timer IRQ/cre-sta経路を再検証する。

## Existing Pattern Findings

- `dly_tsk()` と `twai_sem()` はRUNNING currentをWAITINGへ落とした後、`scheduler_select_next()` と `dispatcher_switch_to()` へ進む既存パターンを持つ。
- `sta_tsk()` はREADY化したtask自身をcurrentと比較し、高優先度なら `dispatch_request_from_task_start()` へ接続する。14.2では同じ境界をwakeup用reasonで拡張する。
- `timer_tick()` はdelay queue tick処理までを行い、task APIや `dispatcher_switch_to()` を直接呼ばない。14.2でもこの境界を維持する。

## Components and Interfaces

### μITRON API Header

`kernel/include/itron_api.h` に `SLP_TSK_*` / `WUP_TSK_*` 戻り値と `int slp_tsk(void);` / `int wup_tsk(int tskid);` を追加する。Doxygenには14.2の対象、非対象、timer IRQ handlerから呼ばないことを明記する。

### API Implementation

`kernel/itron_api.c` に `slp_tsk()` と `wup_tsk()` を追加する。

- `slp_tsk()` はcurrent未設定または非RUNNINGを拒否する。
- 成功時は `task_mark_waiting_on_sleep(current->id)` を呼び、schedulerで次READY taskを選ぶ。
- 次taskがあれば既存 `dispatcher_switch_to()` へ進む。timer IRQ handlerとは接続しない。
- `wup_tsk()` は対象IDを検証し、対象がWAITING(sleep)でなければ状態を変更せず失敗する。
- 成功時は `task_wake_waiting_on_sleep_by_id(tskid)` でREADYへ戻し、current RUNNINGと比較して高優先度ならwakeup用dispatch pendingを要求する。

### Task Module

`kernel/include/task.h` / `kernel/task.c` に `TASK_WAIT_REASON_SLEEP`、`task_mark_waiting_on_sleep()`、`task_wake_waiting_on_sleep_by_id()` を追加する。sleep待ちはsemaphore IDを持たず、remaining tickも持たないため、`wait_sem_id=0`、`delay_ticks_remaining=0` に固定する。

### Dispatch Pending

`kernel/include/dispatch_pending.h` / `kernel/dispatch_pending.c` に `DISPATCH_PENDING_TASK_WAKEUP` と `dispatch_request_from_task_wakeup()` を追加する。実dispatchは後段境界に残し、`wup_tsk()` はpendingを記録するだけにする。

### Smoke and Documentation

`kernel/kernel.c` の通常boot smokeへ14.2観測ログを追加し、READMEの到達点とZenn Articles表、`docs/logs/qemu-serial.log` を更新する。`make run VALIDATE_TIMER_IRQ_ENTRY=1` は既存timer IRQ validationを維持する。

## File Structure Plan

- `kernel/include/itron_api.h`: `slp_tsk()` / `wup_tsk()` の公開契約と戻り値。
- `kernel/itron_api.c`: API本体、ログ、scheduler/dispatcher/preemption pending接続。
- `kernel/include/task.h`: sleep wait reasonとtask helper宣言。
- `kernel/task.c`: sleep WAITING遷移とsleep READY復帰。
- `kernel/include/dispatch_pending.h`: wakeup pending reason/API。
- `kernel/dispatch_pending.c`: wakeup pending requestログとsnapshot保存。
- `kernel/kernel.c`: 14.2 smokeシナリオ。
- `README.md`: 14.2到達点とtag候補。
- `docs/logs/qemu-serial.log`: `make run` による最新serialログ。
- `.kiro/specs/sleep-wakeup-task-api/{requirements.md,design.md,tasks.md}`: 14.2 spec成果物。

## Testing Strategy

1. 通常buildでAPI宣言、task helper、dispatch pending拡張がリンクできることを確認する。
2. `make run` で `slp_tsk()` のRUNNING->WAITING reason=sleep、scheduler READY候補除外、`wup_tsk()` のWAITING(sleep)->READY、invalid-state失敗、高優先度wakeup時pendingを確認する。
3. `make run VALIDATE_TIMER_IRQ_ENTRY=1` でtimer IRQ handler本体からAPI/dispatcher直接呼び出しに退行していないことを確認する。
4. 既存ログで `yield_tsk()`、semaphore wakeup preemption、delay queue tick READY復帰、`cre_tsk()` / `sta_tsk()` の観測点が維持されていることを確認する。

## Requirement Mapping

- 1.1-1.5: API header、`slp_tsk()`、task sleep helper。
- 2.1-2.5: scheduler/dispatcher接続、timer IRQ境界確認。
- 3.1-3.5: API header、`wup_tsk()`、task sleep wake helper。
- 4.1-4.5: `wup_tsk()` invalid-state防御とログ。
- 5.1-5.8: dispatch pending接続、build/run/IRQ validation、既存経路、成果物更新。
