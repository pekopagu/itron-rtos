/*
 * SPDX-License-Identifier: MIT
 */

/**
 * @file itron_api.c
 * @brief μITRON風API層の最小実装。
 *
 * @details
 * このファイルは `yield_tsk()` の観測入口だけを提供する。
 * dispatcherからcurrent taskを読み取り、HAL consoleへ状態を出力するが、
 * RUNNING taskをREADYへ戻す処理、schedulerによる次タスク選択、
 * dispatcher/context switchへの接続は意図的に行わない。
 */

#include <stddef.h>

#include "dispatcher.h"
#include "hal/console.h"
#include "itron_api.h"
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
 * @brief μITRON風の自発的yield要求入口。
 *
 * @details
 * dispatcher_get_current()でcurrent taskを読み取り、RUNNINGであれば
 * yield要求を観測したことと、まだ実切替へ接続していない延期理由を出力する。
 * current未設定または非RUNNINGの場合は不正状態としてログを出し、負値を返す。
 *
 * この関数はRUNNINGからREADYへの状態遷移、次タスク選択、dispatcher切替、
 * task context切替を呼び出さない。そのためTCB状態、dispatcher current、
 * dispatch pending、CPU contextは変更しない。
 *
 * @return `YIELD_TSK_OK` はRUNNING current taskからの観測成功。
 *         `YIELD_TSK_ERR_INVALID_CURRENT_STATE` はcurrent未設定または非RUNNING。
 */
int yield_tsk(void)
{
    const tcb_t *current = dispatcher_get_current();

    if (current == NULL) {
        /*
         * current未設定は、まだdispatcherが実行中taskを確定していない状態である。
         * 10.1ではyield要求をqueue化せず、後続のscheduler/dispatcherへも渡さず、
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
     * RUNNING currentからのyield要求は受理して観測する。
     * ただし10.1では協調スケジューリング完成前なので、RUNNING->READY遷移、
     * 次task選択、dispatcher_switch_to()、task context switchには接続しない。
     */
    hal_console_write("[yield] called: current");
    itron_api_log_task_identity(current);
    hal_console_write("\n");
    hal_console_write("[yield] deferred: reason=switch-not-connected-yet\n");

    return YIELD_TSK_OK;
}
