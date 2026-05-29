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
#include "hal/console.h"
#include "itron_api.h"
#include "scheduler.h"
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
