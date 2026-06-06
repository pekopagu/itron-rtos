/*
 * SPDX-License-Identifier: MIT
 */

/**
 * @file itron_api.c
 * @brief μITRON風API層の最小実装。
 *
 * @details
 * このファイルは `yield_tsk()` の観測入口を提供する。
 * dispatcherからcurrent taskを読み取り、RUNNING current taskだけをtask管理層へ委譲して
 * READYへ戻す。READY化後はschedulerによる次タスク候補選択を観測し、
 * 第10章10.4では協調API経由でdispatcher/context switch境界へ接続する。
 */

#include <stddef.h>

#include "dispatcher.h"
#include "delay_queue.h"
#include "hal/console.h"
#include "itron_api.h"
#include "scheduler.h"
#include "semaphore.h"
#include "task.h"

/**
 * @brief APIログ用に符号付き整数を10進数で出力する。
 *
 * @details
 * freestanding環境ではprintfを使わないため、HAL console APIだけで
 * task idや戻り値を観測できるようにする。状態遷移やTCB更新は行わない。
 *
 * @param value 出力する整数。
 */
static void itron_api_write_int(int value)
{
    char buffer[20];
    int index = 0;
    unsigned int magnitude;

    if (value < 0) {
        hal_console_putc('-');
        magnitude = (unsigned int)(-value);
    } else {
        magnitude = (unsigned int)value;
    }

    if (magnitude == 0U) {
        hal_console_putc('0');
        return;
    }

    while (magnitude > 0U) {
        buffer[index++] = (char)('0' + (magnitude % 10U));
        magnitude /= 10U;
    }

    while (index > 0) {
        hal_console_putc(buffer[--index]);
    }
}

/**
 * @brief APIログ用に符号なし32bit値を10進数で出力する。
 *
 * @details
 * `dly_tsk()` のdelay tick値は `uint32_t` として受け取る。freestanding環境で
 * printfを使わずHAL consoleだけで観測ログを出すため、表示専用の変換を持つ。
 *
 * @param value 出力する符号なし値。
 */
static void itron_api_write_uint32(uint32_t value)
{
    char buffer[10];
    int index = 0;

    if (value == 0U) {
        hal_console_putc('0');
        return;
    }

    while (value > 0U) {
        buffer[index++] = (char)('0' + (value % 10U));
        value /= 10U;
    }

    while (index > 0) {
        hal_console_putc(buffer[--index]);
    }
}

/**
 * @brief task状態をyield APIログ用の固定文字列へ変換する。
 *
 * @details
 * ログは観測用であり、この関数はTCB状態を変更しない。
 * 未知の値は将来拡張や破損観測を区別できるよう `UNKNOWN` として扱う。
 *
 * @param state 変換対象のtask状態。
 * @return HAL consoleへ渡せる静的文字列。
 */
static const char *itron_api_task_state_name(task_state_t state)
{
    switch (state) {
    case TASK_STATE_UNUSED:
        return "UNUSED";
    case TASK_STATE_DORMANT:
        return "DORMANT";
    case TASK_STATE_READY:
        return "READY";
    case TASK_STATE_RUNNING:
        return "RUNNING";
    case TASK_STATE_WAITING:
        return "WAITING";
    default:
        return "UNKNOWN";
    }
}

/**
 * @brief yield APIログ用にtask識別情報を出力する。
 *
 * @details
 * current taskのid/name/stateを一貫した順序で出力する。
 * nameがNULLの場合でもHAL consoleへNULL pointerを渡さない。
 *
 * @param task 出力対象のtask。NULLは呼び出し側で扱う。
 */
static void itron_api_log_task_identity(const tcb_t *task)
{
    const char *task_name = (task->name != NULL) ? task->name : "(null)";

    hal_console_write(" id=");
    itron_api_write_int(task->id);
    hal_console_write(" name=");
    hal_console_write(task_name);
    hal_console_write(" state=");
    hal_console_write(itron_api_task_state_name(task->state));
}

/**
 * @brief yield APIの状態遷移ログ用にtask id/nameだけを出力する。
 *
 * @details
 * RUNNING->READYの遷移結果を読みやすくするため、状態名は末尾の固定表現へ分離する。
 * このhelperは表示専用であり、TCB状態の変更やdispatcher currentの更新は行わない。
 *
 * @param task 出力対象のtask。呼び出し側でNULLではないことを確認済みである。
 */
static void itron_api_log_task_id_name(const tcb_t *task)
{
    const char *task_name = (task->name != NULL) ? task->name : "(null)";

    hal_console_write(" id=");
    itron_api_write_int(task->id);
    hal_console_write(" name=");
    hal_console_write(task_name);
}

/**
 * @brief yield APIの次task候補ログ用にtask識別情報を出力する。
 *
 * @details
 * 10.3ではREADY化後にschedulerが選んだ候補を観測するだけであり、
 * このhelperも表示専用である。候補taskをRUNNINGへ進めず、dispatcher currentを
 * 更新せず、context switch層へ渡さない。
 *
 * @param task schedulerが選んだREADY候補。呼び出し側でNULLではないことを確認済み。
 */
static void itron_api_log_next_candidate(const tcb_t *task)
{
    const char *task_name = (task->name != NULL) ? task->name : "(null)";

    hal_console_write(" id=");
    itron_api_write_int(task->id);
    hal_console_write(" name=");
    hal_console_write(task_name);
    hal_console_write(" prio=");
    itron_api_write_int(task->priority);
    hal_console_write(" state=");
    hal_console_write(itron_api_task_state_name(task->state));
}

/**
 * @brief μITRON風の自発的yield要求入口。
 *
 * @details
 * dispatcher_get_current()でcurrent taskを読み取り、RUNNINGであれば
 * task管理層の `task_mark_ready_from_running()` を通じてREADYへ戻す。
 * current未設定または非RUNNINGの場合は不正状態としてログを出し、負値を返す。
 *
 * この関数は10.4時点でRUNNING current taskをREADYへ戻した後、
 * `scheduler_select_next()` で次のREADY候補を選び、候補が存在する場合だけ
 * `dispatcher_switch_to()` へ接続する。yield_tskはentry returnではないため、
 * DORMANT taskをREADYへ戻さない。timer IRQ、interrupt exit boundary、
 * dispatch pending、preemptive switchとは接続しない。
 *
 * @return `YIELD_TSK_OK` はRUNNING current taskのREADY化と候補選択境界到達。
 *         `YIELD_TSK_ERR_INVALID_CURRENT_STATE` はcurrent未設定または非RUNNING。
 */
int yield_tsk(void)
{
    const tcb_t *current = dispatcher_get_current();
    const tcb_t *next;
    tcb_t *current_mutable;
    tcb_t *next_mutable;
    int ready_result;
    int switch_result;

    /*
     * dispatcher currentが未確定なら、どのtaskを待ち入りさせるか決められない。
     * そのためtimeout値やsemaphore IDの詳細検証より先に、呼び出し文脈を拒否する。
     */
    if (current == NULL) {
        /*
         * current未設定は、まだdispatcherが実行中taskを確定していない状態である。
         * 10.2でもyield要求をqueue化せず、後続のscheduler/dispatcherへも渡さず、
         * 「不正な呼び出し状況を観測した」ことだけを返す。
         */
        hal_console_write("[yield] called\n");
        hal_console_write("[yield] rejected: reason=invalid-current-state current=none\n");
        return YIELD_TSK_ERR_INVALID_CURRENT_STATE;
    }

    if (current->state != TASK_STATE_RUNNING) {
        /*
         * yield_tskはentry returnではなく、RUNNING taskからの自発的な譲渡要求である。
         * DORMANT/READY/WAITINGなどは実行中taskとは扱わず、状態を補正しない。
         * 特に9.4のentry return -> DORMANT確定をここでREADYへ戻してはならない。
         * 呼び出しログとrejectログの両方に同じcurrent情報を出し、後からログだけで
         * 「どの状態を拒否したか」を追跡できるようにする。
         */
        hal_console_write("[yield] called: current");
        itron_api_log_task_identity(current);
        hal_console_write("\n");
        hal_console_write("[yield] rejected: reason=invalid-current-state current");
        itron_api_log_task_identity(current);
        hal_console_write("\n");
        return YIELD_TSK_ERR_INVALID_CURRENT_STATE;
    }

    /*
     * READY化の前にRUNNING状態をログへ残す。状態変更後に同じTCBを読むとREADYに見えるため、
     * 「RUNNING currentからyieldされた」という入力条件はここで固定して観測する。
     */
    hal_console_write("[yield] called: current");
    itron_api_log_task_identity(current);
    hal_console_write("\n");

    /*
     * 10.4でもREADY化対象はRUNNING current taskだけに限定する。
     * 状態変更の所有権はtask管理層に残し、yield API層はarch/x86_64のcontext switch詳細に触れない。
     */
    ready_result = task_mark_ready_from_running(current->id);
    if (ready_result != 0) {
        /*
         * 直前にRUNNINGを確認していても、task管理層が拒否した場合は成功扱いにしない。
         * 将来current更新順序やTCB所有権が変わったときの不整合をログで検出するためである。
         */
        hal_console_write("[yield] rejected: reason=ready-transition-failed current");
        itron_api_log_task_identity(current);
        hal_console_write(" err=");
        itron_api_write_int(ready_result);
        hal_console_write("\n");
        return YIELD_TSK_ERR_INVALID_CURRENT_STATE;
    }

    /*
     * currentは読み取り専用ポインタのまま使う。状態変更の実体はtask管理層で済んでいるため、
     * ここではid/nameだけを出し、遷移前後の状態名は固定文言として明示する。
     */
    hal_console_write("[yield] state transition: current");
    itron_api_log_task_id_name(current);
    hal_console_write(" RUNNING->READY\n");
    /*
     * READY化後にschedulerへ候補選択だけを依頼する。scheduler_select_next()は
     * READY taskを読むだけで、RUNNING遷移、dispatcher current更新、context switchは行わない。
     */
    next = scheduler_select_next();
    if (next == NULL || next->id == current->id) {
        /*
         * READYへ戻したcurrent自身しか候補がない場合は、10.4では「切替先なし」と扱う。
         * 自分自身へdispatcher_switch_to()を呼ぶと、協調yieldの観測ではなく
         * same-task失敗経路になり、no-next分岐の意図がログから読みにくくなるためである。
         */
        hal_console_write("[yield] no next task: reason=no-ready-task\n");
        hal_console_write("[yield] deferred: reason=no-next-task\n");
        return YIELD_TSK_OK;
    } else {
        hal_console_write("[yield] next selected:");
        itron_api_log_next_candidate(next);
        hal_console_write("\n");
    }

    /*
     * 10.4では協調API経由で初めてdispatcher境界へ接続する。yield_tsk()は
     * x86_64の保存復元詳細を知らず、切替境界と実体はdispatcher/task_context層へ委譲する。
     * currentはREADY化済みなので、dispatcher側は協調yield由来のfromとして扱う。
     */
    /*
     * schedulerとdispatcher_get_current()は読み取り専用TCBを返す。
     * yield API層で直接状態を書き換えない方針を守るため、switch境界へ渡す直前に
     * task管理層のID lookupで更新可能TCBを取り直す。
     */
    current_mutable = task_get_mutable_by_id(current->id);
    next_mutable = task_get_mutable_by_id(next->id);
    if (current_mutable == NULL || next_mutable == NULL) {
        hal_console_write("[yield] deferred: reason=switch-task-not-found\n");
        return YIELD_TSK_ERR_INVALID_CURRENT_STATE;
    }

    hal_console_write("[yield] switch begin: from");
    itron_api_log_task_id_name(current);
    hal_console_write(" to");
    itron_api_log_task_id_name(next);
    hal_console_write("\n");

    /*
     * ここが10.4の接続点である。yield_tsk()は「協調APIからswitchを要求した」
     * ことだけを示し、実際のRUNNING/READY整理、current更新、context switch実体は
     * dispatcher/task_context層へ残す。
     */
    switch_result = dispatcher_switch_to(current_mutable, next_mutable);

    hal_console_write("[yield] switch end: result=");
    itron_api_write_int(switch_result);
    hal_console_write("\n");

    return YIELD_TSK_OK;
}

/**
 * @brief μITRON風 `wai_sem()` のtask文脈入口。
 *
 * @details
 * dispatcherが保持するcurrent taskを呼び出し元として扱う。countが残る場合は
 * semaphore moduleにcount減算だけを委譲し、context switchしない。countが0の
 * 場合だけRUNNING current taskをWAITINGへ落とし、schedulerで次READY taskを
 * 選んで既存の `dispatcher_switch_to()` 境界へ接続する。
 *
 * この入口は第12章12.1のboot-time verification modelである。`sig_sem()` による
 * 待ちtask復帰、wait queue、timeout、wakeup後preemption、同一優先度time slice、
 * round-robinはここでは実装しない。またtimer IRQ handler本体から呼ばれるAPIではない。
 *
 * @param sem_id 対象semaphore ID。
 * @return 成功時はWAI_SEM_OK。失敗時はWAI_SEM_ERR_*。
 */
int wai_sem(int sem_id)
{
    const tcb_t *current = dispatcher_get_current();
    const tcb_t *next;
    tcb_t *current_mutable;
    tcb_t *next_mutable;
    const semaphore_t *sem;
    int count_before = 0;
    int count_after = 0;
    int take_result;
    int wait_result;
    int enqueue_result;
    int switch_result;

    if (current == NULL) {
        hal_console_write("[wai-sem] called: sem_id=");
        itron_api_write_int(sem_id);
        hal_console_write(" current=none\n");
        hal_console_write("[wai-sem] rejected: reason=invalid-current-state current=none\n");
        return WAI_SEM_ERR_INVALID_CURRENT_STATE;
    }

    hal_console_write("[wai-sem] called: sem_id=");
    itron_api_write_int(sem_id);
    hal_console_write(" current");
    itron_api_log_task_identity(current);
    hal_console_write("\n");

    if (current->state != TASK_STATE_RUNNING) {
        hal_console_write("[wai-sem] rejected: reason=invalid-current-state current");
        itron_api_log_task_identity(current);
        hal_console_write("\n");
        return WAI_SEM_ERR_INVALID_CURRENT_STATE;
    }

    sem = sem_get_by_id(sem_id);
    if (sem == NULL) {
        hal_console_write("[wai-sem] rejected: reason=invalid-semaphore sem_id=");
        itron_api_write_int(sem_id);
        hal_console_write("\n");
        return WAI_SEM_ERR_SEMAPHORE;
    }

    take_result = sem_take_if_available(sem_id, &count_before, &count_after);
    if (take_result == SEM_OK) {
        hal_console_write("[wai-sem] acquired: sem_id=");
        itron_api_write_int(sem_id);
        hal_console_write(" count ");
        itron_api_write_int(count_before);
        hal_console_write("->");
        itron_api_write_int(count_after);
        hal_console_write("\n");
        hal_console_write("[wai-sem] completed: result=0 action=no-switch\n");
        return WAI_SEM_OK;
    }

    if (take_result != SEM_WAIT_REQUIRED) {
        hal_console_write("[wai-sem] rejected: reason=semaphore-error err=");
        itron_api_write_int(take_result);
        hal_console_write("\n");
        return WAI_SEM_ERR_SEMAPHORE;
    }

    hal_console_write("[wai-sem] wait required: sem_id=");
    itron_api_write_int(sem_id);
    hal_console_write(" count=");
    itron_api_write_int(count_before);
    hal_console_write("\n");

    /*
     * 12.1では待ち入り時だけRUNNING current taskをWAITINGへ落とす。
     * ここではwait queueやtimeoutを作らず、TCBのstate/wait_sem_id更新だけを
     * task moduleへ委譲する。
     */
    wait_result = task_mark_waiting_on_sem(current->id, sem_id);
    if (wait_result != 0) {
        hal_console_write("[wai-sem] rejected: reason=waiting-transition-failed err=");
        itron_api_write_int(wait_result);
        hal_console_write("\n");
        return WAI_SEM_ERR_DISPATCH;
    }

    /*
     * 12.3ではWAITING化したtaskだけを対象semaphoreのFIFO wait queueへ積む。
     * priority順、timeout、wakeup後preemption判定はここでは扱わず、queue登録後も
     * 12.1と同じscheduler/dispatcher境界へ進む。
     */
    enqueue_result = sem_enqueue_waiter(sem_id, current->id);
    if (enqueue_result != SEM_OK) {
        hal_console_write("[wai-sem] rejected: reason=wait-queue-enqueue-failed err=");
        itron_api_write_int(enqueue_result);
        hal_console_write("\n");
        return WAI_SEM_ERR_DISPATCH;
    }

    hal_console_write("[wai-sem] state transition: current");
    itron_api_log_task_id_name(current);
    hal_console_write(" RUNNING->WAITING\n");

    /*
     * WAITING taskはscheduler_select_next()のREADY候補から自然に除外される。
     * 次READY taskがない場合のidle/停止処理はまだ未実装なので、ログだけで止める。
     */
    next = scheduler_select_next();
    if (next == NULL) {
        hal_console_write("[wai-sem] no next task: reason=no-ready-task action=unsupported-stop\n");
        return WAI_SEM_OK;
    }

    hal_console_write("[wai-sem] next selected:");
    itron_api_log_next_candidate(next);
    hal_console_write("\n");

    current_mutable = task_get_mutable_by_id(current->id);
    next_mutable = task_get_mutable_by_id(next->id);
    if (current_mutable == NULL || next_mutable == NULL) {
        hal_console_write("[wai-sem] rejected: reason=switch-task-not-found\n");
        return WAI_SEM_ERR_DISPATCH;
    }

    hal_console_write("[wai-sem] switch begin: from");
    itron_api_log_task_id_name(current);
    hal_console_write(" to");
    itron_api_log_task_id_name(next);
    hal_console_write("\n");

    switch_result = dispatcher_switch_to(current_mutable, next_mutable);

    hal_console_write("[wai-sem] switch end: result=");
    itron_api_write_int(switch_result);
    hal_console_write("\n");

    if (switch_result != DISPATCHER_OK) {
        return WAI_SEM_ERR_DISPATCH;
    }

    return WAI_SEM_OK;
}

/**
 * @brief 13.3時点のtimeout付きsemaphore待ちAPI。
 *
 * @details
 * `twai_sem()` はtask文脈APIであり、timer IRQ handler本体から呼ばない。
 * semaphore countが残っている場合は即時取得して完了する。countが0の場合は
 * semaphore wait queueとdelay queueの両方へ登録可能かをWAITING化前に確認し、
 * 成功した場合だけtimeout付きsemaphore WAITINGへ遷移させる。
 *
 * 13.3ではtimeout tickのdecrement、timeout到達時READY復帰、timeout時の
 * semaphore wait queue削除、`sig_sem()` 成功時のdelay queue削除は扱わない。
 *
 * @param sem_id 対象semaphore ID。
 * @param timeout_ticks timeout観測用tick数。0はinvalid timeout。
 * @return 成功時はTWAI_SEM_OK。失敗時はTWAI_SEM_ERR_*。
 */
int twai_sem(int sem_id, uint32_t timeout_ticks)
{
    const tcb_t *current = dispatcher_get_current();
    const tcb_t *next;
    tcb_t *current_mutable;
    tcb_t *next_mutable;
    const semaphore_t *sem;
    int count_after = 0;
    int take_result;
    int sem_queue_result;
    int delay_queue_result;
    int wait_result;
    int enqueue_result;
    int switch_result;

    if (current == NULL) {
        hal_console_write("[twai-sem] called: sem id=");
        itron_api_write_int(sem_id);
        hal_console_write(" timeout_ticks=");
        itron_api_write_uint32(timeout_ticks);
        hal_console_write(" current=none\n");
        hal_console_write("[twai-sem] rejected: reason=invalid-current-state current=none\n");
        hal_console_write("[twai-sem] completed: result=");
        itron_api_write_int(TWAI_SEM_ERR_INVALID_CURRENT_STATE);
        hal_console_write(" action=invalid-current-state\n");
        return TWAI_SEM_ERR_INVALID_CURRENT_STATE;
    }

    /*
     * 以降の分岐で同じcurrentを扱うため、最初に呼び出し条件を観測ログへ固定する。
     * この時点ではTCBやsemaphore count、queue状態はまだ変更しない。
     */
    hal_console_write("[twai-sem] called: sem id=");
    itron_api_write_int(sem_id);
    hal_console_write(" timeout_ticks=");
    itron_api_write_uint32(timeout_ticks);
    hal_console_write(" current");
    itron_api_log_task_identity(current);
    hal_console_write("\n");

    if (timeout_ticks == 0U) {
        /*
         * 13.3では0 tick timeoutをpoll相当にせず、明示的な入力エラーとして扱う。
         * current taskやqueueを変更しないため、後続の通常smokeへ影響しない。
         */
        hal_console_write("[twai-sem] invalid timeout: timeout_ticks=0\n");
        hal_console_write("[twai-sem] completed: result=");
        itron_api_write_int(TWAI_SEM_ERR_INVALID_TIMEOUT);
        hal_console_write(" action=invalid-timeout\n");
        return TWAI_SEM_ERR_INVALID_TIMEOUT;
    }

    if (current->state != TASK_STATE_RUNNING) {
        /*
         * twai_sem()はtask文脈APIなので、RUNNING current taskだけを待ち入り対象にする。
         * READY/DORMANT/WAITING taskをここで再分類しない。
         */
        hal_console_write("[twai-sem] rejected: reason=invalid-current-state current");
        itron_api_log_task_identity(current);
        hal_console_write("\n");
        hal_console_write("[twai-sem] completed: result=");
        itron_api_write_int(TWAI_SEM_ERR_INVALID_CURRENT_STATE);
        hal_console_write(" action=invalid-current-state\n");
        return TWAI_SEM_ERR_INVALID_CURRENT_STATE;
    }

    /*
     * semaphore tableの存在確認だけを先に行う。
     * 存在しないsemaphoreではcount取得もqueue登録も行わず、副作用なしで失敗させる。
     */
    sem = sem_get_by_id(sem_id);
    if (sem == NULL) {
        hal_console_write("[twai-sem] rejected: reason=invalid-semaphore sem id=");
        itron_api_write_int(sem_id);
        hal_console_write("\n");
        hal_console_write("[twai-sem] completed: result=");
        itron_api_write_int(TWAI_SEM_ERR_SEMAPHORE);
        hal_console_write(" action=invalid-semaphore\n");
        return TWAI_SEM_ERR_SEMAPHORE;
    }

    /*
     * count取得はsemaphore moduleへ委譲する。
     * 取得できる場合はここで完了し、timeout待ちのTCB状態やqueueには触れない。
     */
    take_result = sem_take_if_available(sem_id, NULL, &count_after);
    if (take_result == SEM_OK) {
        /*
         * countが残っている場合はtimeout待ちには入らない。
         * queue登録もscheduler/dispatcher接続も行わず、通常の取得成功として完了する。
         */
        hal_console_write("[twai-sem] acquired immediately: sem id=");
        itron_api_write_int(sem_id);
        hal_console_write(" count=");
        itron_api_write_int(count_after);
        hal_console_write("\n");
        hal_console_write("[twai-sem] completed: result=0 action=acquired\n");
        return TWAI_SEM_OK;
    }

    /*
     * `SEM_WAIT_REQUIRED` 以外の戻り値はsemaphore層の異常として扱う。
     * ここでもWAITING化やqueue登録へ進まず、current taskをRUNNINGのまま残す。
     */
    if (take_result != SEM_WAIT_REQUIRED) {
        hal_console_write("[twai-sem] rejected: reason=semaphore-error err=");
        itron_api_write_int(take_result);
        hal_console_write("\n");
        hal_console_write("[twai-sem] completed: result=");
        itron_api_write_int(TWAI_SEM_ERR_SEMAPHORE);
        hal_console_write(" action=semaphore-error\n");
        return TWAI_SEM_ERR_SEMAPHORE;
    }

    /*
     * WAITING化前に両queueの受け入れ可否を確認する。
     * ここで失敗した場合はTCBもqueueも変更せず、不整合な片側登録を残さない。
     */
    /*
     * semaphore wait queueを先にprecheckする。
     * ここで満杯ならdelay queue側にも触れず、片側だけの登録状態を作らない。
     */
    sem_queue_result = sem_can_enqueue_waiter(sem_id, current->id);
    if (sem_queue_result != SEM_OK) {
        if (sem_queue_result == SEM_ERR_OVERFLOW) {
            hal_console_write("[sem-wq] enqueue failed: reason=full sem id=");
            itron_api_write_int(sem_id);
            hal_console_write(" task id=");
            itron_api_write_int(current->id);
            hal_console_write(" name=");
            hal_console_write((current->name != NULL) ? current->name : "(null)");
            hal_console_write("\n");
            hal_console_write("[twai-sem] completed: result=");
            itron_api_write_int(TWAI_SEM_ERR_SEMAPHORE);
            hal_console_write(" action=sem-wait-queue-full\n");
        } else {
            hal_console_write("[twai-sem] completed: result=");
            itron_api_write_int(TWAI_SEM_ERR_SEMAPHORE);
            hal_console_write(" action=sem-wait-queue-invalid\n");
        }
        return TWAI_SEM_ERR_SEMAPHORE;
    }

    /*
     * timeout観測用のdelay queueもWAITING化前にprecheckする。
     * semaphore wait queueとdelay queueの両方が受け入れ可能な場合だけ状態遷移へ進む。
     */
    delay_queue_result = delay_queue_can_enqueue(current->id);
    if (delay_queue_result != DELAY_QUEUE_OK) {
        if (delay_queue_result == DELAY_QUEUE_ERR_FULL) {
            hal_console_write("[delay-q] enqueue failed: reason=full task id=");
        } else if (delay_queue_result == DELAY_QUEUE_ERR_DUPLICATE) {
            hal_console_write("[delay-q] enqueue failed: reason=duplicate task id=");
        } else {
            hal_console_write("[delay-q] enqueue failed: reason=invalid task id=");
        }
        itron_api_write_int(current->id);
        hal_console_write(" name=");
        hal_console_write((current->name != NULL) ? current->name : "(null)");
        hal_console_write(" delay_ticks=");
        itron_api_write_uint32(timeout_ticks);
        hal_console_write("\n");
        hal_console_write("[twai-sem] completed: result=");
        itron_api_write_int(TWAI_SEM_ERR_DELAY_QUEUE);
        if (delay_queue_result == DELAY_QUEUE_ERR_FULL) {
            hal_console_write(" action=delay-queue-full\n");
        } else if (delay_queue_result == DELAY_QUEUE_ERR_DUPLICATE) {
            hal_console_write(" action=delay-queue-duplicate\n");
        } else {
            hal_console_write(" action=delay-queue-invalid\n");
        }
        return TWAI_SEM_ERR_DELAY_QUEUE;
    }

    /*
     * 両queueのprecheckが通ったため、timeout付き待ち入りの開始を明示する。
     * このログ以降で初めてTCBをWAITINGへ変更する。
     */
    hal_console_write("[twai-sem] wait begin: sem id=");
    itron_api_write_int(sem_id);
    hal_console_write(" current");
    itron_api_log_task_id_name(current);
    hal_console_write(" timeout_ticks=");
    itron_api_write_uint32(timeout_ticks);
    hal_console_write("\n");

    /*
     * task moduleにTCB状態の所有権を残し、timeout付きsemaphore WAITINGへ遷移させる。
     * API層は直接TCBを書き換えず、状態遷移の成否だけを受け取る。
     */
    wait_result = task_mark_waiting_on_sem_timeout(current->id, sem_id, timeout_ticks);
    if (wait_result != 0) {
        hal_console_write("[twai-sem] rejected: reason=timeout-wait-transition-failed err=");
        itron_api_write_int(wait_result);
        hal_console_write("\n");
        hal_console_write("[twai-sem] completed: result=");
        itron_api_write_int(TWAI_SEM_ERR_DISPATCH);
        hal_console_write(" action=timeout-wait-transition-failed\n");
        return TWAI_SEM_ERR_DISPATCH;
    }

    /*
     * timeout付きsemaphore待ちは `sig_sem()` のwakeup対象でもあるため、
     * 対象semaphoreのwait queueへ登録する。
     */
    enqueue_result = sem_enqueue_waiter(sem_id, current->id);
    if (enqueue_result != SEM_OK) {
        hal_console_write("[twai-sem] completed: result=");
        itron_api_write_int(TWAI_SEM_ERR_SEMAPHORE);
        hal_console_write(" action=sem-wait-queue-enqueue-failed\n");
        return TWAI_SEM_ERR_SEMAPHORE;
    }

    /*
     * timeout観測用に同じtaskをdelay queueへも登録する。
     * wait_sem_idはsemaphore待ち対象として残し、delay queue metadataには使わない。
     */
    delay_queue_result = delay_queue_enqueue(current->id, timeout_ticks);
    if (delay_queue_result != DELAY_QUEUE_OK) {
        hal_console_write("[twai-sem] completed: result=");
        itron_api_write_int(TWAI_SEM_ERR_DELAY_QUEUE);
        hal_console_write(" action=delay-queue-enqueue-failed\n");
        return TWAI_SEM_ERR_DELAY_QUEUE;
    }

    hal_console_write("[twai-sem] state transition: current");
    itron_api_log_task_id_name(current);
    hal_console_write(" RUNNING->WAITING reason=semaphore-timeout\n");

    /*
     * delay queue上のremaining tickとwait reasonを観測する。
     * dumpは表示専用であり、tick減算やREADY復帰は行わない。
     */
    delay_queue_dump();

    /*
     * WAITING化したcurrent taskはREADY候補から外れる。
     * 既存scheduler境界を使い、次に動けるREADY taskだけを選ぶ。
     */
    next = scheduler_select_next();
    if (next == NULL) {
        hal_console_write("[twai-sem] no next task: reason=no-ready-task action=unsupported-stop\n");
        hal_console_write("[twai-sem] completed: result=0 action=no-ready-task\n");
        return TWAI_SEM_OK;
    }

    /*
     * schedulerが選んだ候補をswitch前にログへ固定する。
     * このログは候補観測であり、dispatcher currentの更新はまだ行わない。
     */
    hal_console_write("[twai-sem] next selected:");
    itron_api_log_next_candidate(next);
    hal_console_write("\n");

    /*
     * dispatcher境界へ渡す直前に、読み取り専用TCBではなく更新可能TCBを取り直す。
     * task table所有権を守るため、API層ではID lookupに限定する。
     */
    current_mutable = task_get_mutable_by_id(current->id);
    next_mutable = task_get_mutable_by_id(next->id);
    if (current_mutable == NULL || next_mutable == NULL) {
        hal_console_write("[twai-sem] rejected: reason=switch-task-not-found\n");
        hal_console_write("[twai-sem] completed: result=");
        itron_api_write_int(TWAI_SEM_ERR_DISPATCH);
        hal_console_write(" action=timeout-wait-switch-failed\n");
        return TWAI_SEM_ERR_DISPATCH;
    }

    /*
     * 13.3の実行切替は既存dispatcher switch境界へ委譲する。
     * twai_sem()自身はarch context switchの詳細やstack切替を扱わない。
     */
    hal_console_write("[twai-sem] switch begin: from");
    itron_api_log_task_id_name(current);
    hal_console_write(" to");
    itron_api_log_task_id_name(next);
    hal_console_write("\n");

    /*
     * dispatcher_switch_to()の結果をAPIログへ戻し、timeout待ち入り後の切替証跡にする。
     * 失敗してもtimeout満了処理やqueue巻き戻しは13.3の範囲外である。
     */
    switch_result = dispatcher_switch_to(current_mutable, next_mutable);

    hal_console_write("[twai-sem] switch end: result=");
    itron_api_write_int(switch_result);
    hal_console_write("\n");

    if (switch_result != DISPATCHER_OK) {
        hal_console_write("[twai-sem] completed: result=");
        itron_api_write_int(TWAI_SEM_ERR_DISPATCH);
        hal_console_write(" action=timeout-wait-switch-failed\n");
        return TWAI_SEM_ERR_DISPATCH;
    }

    hal_console_write("[twai-sem] completed: result=0 action=timeout-wait-queued-switch\n");
    return TWAI_SEM_OK;
}

/**
 * @brief μITRON風 `dly_tsk()` のtask文脈API入口。
 *
 * @details
 * 第13章13.1では、RUNNING current taskをdelay理由のWAITINGへ遷移させる
 * 最小入口だけを実装する。`delay_ticks == 0` はno-opではなくエラーとして扱い、
 * task状態も待ち観測フィールドも変更しない。
 *
 * delay WAITING化後は既存schedulerで次READY taskを選び、存在する場合だけ既存
 * `dispatcher_switch_to()` 境界へ進む。sleep/delay queue、tickごとの減算、
 * tick到達時READY復帰、timer IRQ handlerからの呼び出しはまだ扱わない。
 *
 * @param delay_ticks delay待ちとして観測するtick数。0は不正。
 * @return 成功時はDLY_TSK_OK、失敗時はDLY_TSK_ERR_*。
 */
/**
 * @brief 13.2時点のdelay queue接続済み `dly_tsk()`。
 *
 * @details
 * `delay_ticks > 0` の場合、RUNNING current taskをdelay WAITINGへ落とす前に
 * delay queueへ登録可能かを確認する。登録不能な場合はtask状態を変更せず、
 * `result=-1` とqueue失敗actionをログへ残す。登録可能な場合だけWAITING化し、
 * `wait_reason=delay` のtaskをdelay queueへenqueueしてから既存scheduler/dispatcher
 * 境界へ進む。
 *
 * enqueue可否確認は、queue満杯または二重enqueueのときに不整合なWAITING taskを
 * 残さないための13.2の重要な境界である。enqueue本体でもdelay WAITINGであることを
 * 再確認し、semaphore待ちtaskをdelay queueへ混入させない。
 *
 * 13.2ではdelay queue entryのremaining tickは観測用であり、tick decrement、
 * tick到達時READY復帰、delay queueからのdequeue wakeup、timeout付き `twai_sem`、
 * timer IRQ handlerからのAPI呼び出しはまだ行わない。
 *
 * @param delay_ticks delay queueへ観測用に登録するtick数。0はinvalid-delayとして扱う。
 * @return 成功時は `DLY_TSK_OK`。invalid delay、invalid current、dispatch失敗時は `DLY_TSK_ERR_*`。
 * @note 13.2ではdelay queue登録成功後もtimer連動のwakeupは行わない。
 */
int dly_tsk(uint32_t delay_ticks)
{
    const tcb_t *current = dispatcher_get_current();
    const tcb_t *next;
    tcb_t *current_mutable;
    tcb_t *next_mutable;
    int wait_result;
    int delay_queue_result;
    int switch_result;

    if (current == NULL) {
        /*
         * dispatcher currentがない状態では「どのtaskをdelay待ちへ落とすか」が
         * 決められない。delay_ticksの妥当性より先に呼び出し文脈の欠落を記録する。
         */
        hal_console_write("[dly-tsk] called: delay_ticks=");
        itron_api_write_uint32(delay_ticks);
        hal_console_write(" current=none\n");
        hal_console_write("[dly-tsk] rejected: reason=invalid-current-state current=none\n");
        hal_console_write("[dly-tsk] completed: result=");
        itron_api_write_int(DLY_TSK_ERR_INVALID_CURRENT_STATE);
        hal_console_write(" action=invalid-current-state\n");
        return DLY_TSK_ERR_INVALID_CURRENT_STATE;
    }

    hal_console_write("[dly-tsk] called: delay_ticks=");
    itron_api_write_uint32(delay_ticks);
    hal_console_write(" current");
    itron_api_log_task_identity(current);
    hal_console_write("\n");

    if (delay_ticks == 0U) {
        /*
         * 13.1では0 tick delayをyield相当やno-opにせず、明確な入力エラーにする。
         * 状態変更前に返すことで、current taskのRUNNING状態と既存待ち情報を保つ。
         */
        hal_console_write("[dly-tsk] invalid delay: delay_ticks=0\n");
        hal_console_write("[dly-tsk] completed: result=");
        itron_api_write_int(DLY_TSK_ERR_INVALID_DELAY);
        hal_console_write(" action=invalid-delay\n");
        return DLY_TSK_ERR_INVALID_DELAY;
    }

    if (current->state != TASK_STATE_RUNNING) {
        /*
         * dly_tsk()はtask文脈APIなので、READY/DORMANT/WAITINGを再分類しない。
         * 特に既存WAITING taskをdelay待ちへ上書きすると、semaphore待ちとの分離が壊れる。
         */
        hal_console_write("[dly-tsk] rejected: reason=invalid-current-state current");
        itron_api_log_task_identity(current);
        hal_console_write("\n");
        hal_console_write("[dly-tsk] completed: result=");
        itron_api_write_int(DLY_TSK_ERR_INVALID_CURRENT_STATE);
        hal_console_write(" action=invalid-current-state\n");
        return DLY_TSK_ERR_INVALID_CURRENT_STATE;
    }

    /*
     * WAITING化前にdelay queueの容量と二重enqueueを確認する。
     * ここで失敗させることで、不整合なWAITING taskを残さない。
     */
    delay_queue_result = delay_queue_can_enqueue(current->id);
    if (delay_queue_result != DELAY_QUEUE_OK) {
        /*
         * queueへ入れられない場合は、task_mark_waiting_on_delay()を呼ばない。
         * これにより、queueに存在しないdelay WAITING taskを作らない。
         */
        if (delay_queue_result == DELAY_QUEUE_ERR_FULL) {
            hal_console_write("[delay-q] enqueue failed: reason=full task id=");
        } else if (delay_queue_result == DELAY_QUEUE_ERR_DUPLICATE) {
            hal_console_write("[delay-q] enqueue failed: reason=duplicate task id=");
        } else {
            hal_console_write("[delay-q] enqueue failed: reason=invalid task id=");
        }
        /*
         * 失敗ログにもtask id/name/delay_ticksを含める。
         * WAITING化前に拒否されたtaskを特定し、queue防御が働いたことを観測できるようにする。
         */
        itron_api_write_int(current->id);
        hal_console_write(" name=");
        hal_console_write((current->name != NULL) ? current->name : "(null)");
        hal_console_write(" delay_ticks=");
        itron_api_write_uint32(delay_ticks);
        hal_console_write("\n");
        hal_console_write("[dly-tsk] completed: result=");
        itron_api_write_int(DLY_TSK_ERR_INVALID_DELAY);
        /*
         * 13.2では戻り値を増やさず、action文字列でqueue失敗理由を区別する。
         * 満杯時は仕様例に合わせて `delay-queue-full` を出す。
         */
        if (delay_queue_result == DELAY_QUEUE_ERR_FULL) {
            hal_console_write(" action=delay-queue-full\n");
        } else if (delay_queue_result == DELAY_QUEUE_ERR_DUPLICATE) {
            hal_console_write(" action=delay-queue-duplicate\n");
        } else {
            hal_console_write(" action=delay-queue-invalid\n");
        }
        return DLY_TSK_ERR_INVALID_DELAY;
    }

    wait_result = task_mark_waiting_on_delay(current->id, delay_ticks);
    if (wait_result != 0) {
        /*
         * currentのRUNNING確認後でも、task table側の状態が変わっていれば失敗させる。
         * API層で補正せず、task moduleの所有する状態遷移契約を優先する。
         */
        hal_console_write("[dly-tsk] rejected: reason=delay-transition-failed err=");
        itron_api_write_int(wait_result);
        hal_console_write("\n");
        hal_console_write("[dly-tsk] completed: result=");
        itron_api_write_int(DLY_TSK_ERR_DISPATCH);
        hal_console_write(" action=delay-transition-failed\n");
        return DLY_TSK_ERR_DISPATCH;
    }

    hal_console_write("[dly-tsk] state transition: current");
    itron_api_log_task_id_name(current);
    hal_console_write(" RUNNING->WAITING reason=delay\n");

    /*
     * WAITING化済みtaskだけをdelay queueへ登録する。
     * delay queue側でもwait_reason=delayを再確認し、semaphore待ちの混入を防ぐ。
     */
    delay_queue_result = delay_queue_enqueue(current->id, delay_ticks);
    if (delay_queue_result != DELAY_QUEUE_OK) {
        /*
         * ここに到達する失敗は、WAITING化後の防御的な再確認で検出された不整合である。
         * 13.2ではdelay READY復帰や状態巻き戻しを実装しないため、dispatch失敗として明示的に止める。
         */
        hal_console_write("[dly-tsk] completed: result=");
        itron_api_write_int(DLY_TSK_ERR_DISPATCH);
        hal_console_write(" action=delay-queue-enqueue-failed\n");
        return DLY_TSK_ERR_DISPATCH;
    }

    /*
     * 13.2ではremaining tickを減らさず、queue上で観測できることだけをdumpで確認する。
     */
    delay_queue_dump();

    next = scheduler_select_next();
    if (next == NULL) {
        /*
         * idle taskはまだ導入していないため、次READYなしはログで止める。
         * delay WAITING化は完了済みだが、無理にswitch先を作らない。
         */
        hal_console_write("[dly-tsk] no next task: reason=no-ready-task action=unsupported-stop\n");
        hal_console_write("[dly-tsk] completed: result=0 action=no-ready-task\n");
        return DLY_TSK_OK;
    }

    /*
     * queue登録後もschedulerの責務はREADY taskだけを選ぶことに限定する。
     * delay WAITINGになったcurrent taskはREADY候補から自然に外れる。
     */
    hal_console_write("[dly-tsk] next selected:");
    itron_api_log_next_candidate(next);
    hal_console_write("\n");

    /*
     * scheduler/dispatcherは読み取り用TCBを返すため、switch境界へ渡す直前に
     * 更新可能TCBを取り直す。見つからない場合は不整合としてswitchしない。
     */
    current_mutable = task_get_mutable_by_id(current->id);
    next_mutable = task_get_mutable_by_id(next->id);
    if (current_mutable == NULL || next_mutable == NULL) {
        hal_console_write("[dly-tsk] rejected: reason=switch-task-not-found\n");
        hal_console_write("[dly-tsk] completed: result=");
        itron_api_write_int(DLY_TSK_ERR_DISPATCH);
        hal_console_write(" action=delay-switch-failed\n");
        return DLY_TSK_ERR_DISPATCH;
    }

    hal_console_write("[dly-tsk] switch begin: from");
    itron_api_log_task_id_name(current);
    hal_console_write(" to");
    itron_api_log_task_id_name(next);
    hal_console_write("\n");

    /*
     * delay WAITING化後の実際の切替は、既存dispatcher境界へ委譲する。
     * dly_tsk()側ではx86_64のstack/register詳細を直接扱わない。
     */
    switch_result = dispatcher_switch_to(current_mutable, next_mutable);

    /*
     * dispatcher境界の結果をAPIログへ戻し、delay APIからswitchへ進んだ証跡を残す。
     */
    hal_console_write("[dly-tsk] switch end: result=");
    itron_api_write_int(switch_result);
    hal_console_write("\n");

    if (switch_result != DISPATCHER_OK) {
        /*
         * delay WAITING化は完了済みでも、dispatcher境界の失敗はAPIのswitch失敗として
         * 観測する。ここでdelay taskをREADYへ戻す復旧処理は13.1の範囲外に置く。
         */
        hal_console_write("[dly-tsk] completed: result=");
        itron_api_write_int(DLY_TSK_ERR_DISPATCH);
        hal_console_write(" action=delay-switch-failed\n");
        return DLY_TSK_ERR_DISPATCH;
    }

    hal_console_write("[dly-tsk] completed: result=0 action=delay-queued-switch\n");
    return DLY_TSK_OK;
}
