/*
 * SPDX-License-Identifier: MIT
 */

/**
 * @file dispatcher.c
 * @brief 現在タスク確定境界の実装。
 *
 * @details
 * dispatcherは論理的な現在タスクを保持し、schedulerが選択したREADYタスクを
 * task module経由で確定する。HAL console、scheduler、arch固有処理には
 * 意図的に依存しない。
 */

#include <stddef.h>

#include "dispatcher.h"
#include "task.h"

/*
 * 現在タスクはdispatcherだけが保持する。
 * schedulerを「選択だけ」に保つことで、将来コンテキストスイッチや実行開始処理を
 * 追加するときも、選択結果を確定する責務をこの境界に集約できる。
 */
static const tcb_t *current_task;

void dispatcher_init(void)
{
    /*
     * 起動直後は現在タスクが存在しない状態を明示する。
     * NULLを正式な未設定値にしておくことで、将来の実行開始前チェックでも
     * 「まだcommitされていない」状態を判定しやすくする。
     */
    current_task = NULL;
}

int dispatcher_commit_current(const tcb_t *selected)
{
    int result;

    /*
     * schedulerが選択対象なしをNULLで返すことは正常系としてあり得る。
     * dispatcherではNULLをcommit対象にできないため、current_taskを変更せず失敗にする。
     */
    if (selected == NULL) {
        return DISPATCHER_ERR_INVAL;
    }

    /*
     * READY以外をcommitしないのは、RUNNINGを「READYから選ばれて確定された状態」に
     * 限定するためである。将来WAITINGやDORMANTからの復帰を扱う場合も、
     * その遷移は別の責務として追加する。
     */
    if (selected->state != TASK_STATE_READY) {
        return DISPATCHER_ERR_BAD_STATE;
    }

    /*
     * READYからRUNNINGへの論理状態遷移だけを確定する。
     * 入口関数呼び出し、スタック切り替え、レジスタ保存、コンテキストスイッチは行わない。
     */
    result = task_mark_running(selected->id);
    if (result == TASK_ERR_NOT_FOUND) {
        return DISPATCHER_ERR_NOT_FOUND;
    }

    if (result == TASK_ERR_BAD_STATE) {
        return DISPATCHER_ERR_BAD_STATE;
    }

    if (result != 0) {
        return DISPATCHER_ERR_INVAL;
    }

    /*
     * task_mark_running()が成功した後だけcurrent_taskを更新する。
     * 状態変更とcurrent更新をこの順序にすることで、失敗時に
     * 「currentだがRUNNINGではない」という中途半端な観測状態を作らない。
     */
    current_task = selected;
    return DISPATCHER_OK;
}

const tcb_t *dispatcher_get_current(void)
{
    return current_task;
}
