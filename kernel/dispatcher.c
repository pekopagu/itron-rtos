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

/**
 * @brief dispatcherのcurrent task保持状態を初期化する。
 *
 * @details
 * 起動直後はまだscheduler選択もcurrent commitも行われていないため、
 * current taskを明示的にNULLへ戻す。TCB状態は変更せず、dispatcherが持つ
 * 観測用のcurrent pointerだけを初期化する。
 *
 * @param なし。
 * @return なし。
 */
void dispatcher_init(void)
{
    /*
     * 起動直後は現在タスクが存在しない状態を明示する。
     * NULLを正式な未設定値にしておくことで、将来の実行開始前チェックでも
     * 「まだcommitされていない」状態を判定しやすくする。
     */
    current_task = NULL;
}

/**
 * @brief schedulerが選択したREADY taskをcurrent taskとして確定する。
 *
 * @details
 * この関数はREADYからRUNNINGへの論理状態遷移とcurrent pointer更新だけを行う。
 * task entry呼び出し、context switch、stack switch、register save/restoreは
 * dispatcherの責務ではないため実行しない。
 *
 * @param selected schedulerが選択したREADY task。
 * @return 成功時はDISPATCHER_OK、失敗時はDISPATCHER_ERR_*。
 */
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
        /* schedulerが返したtaskがtask table側で見つからない場合は境界不整合として扱う。 */
        return DISPATCHER_ERR_NOT_FOUND;
    }

    if (result == TASK_ERR_BAD_STATE) {
        /* READY確認後に状態が変わった場合も、dispatcher側では成功扱いしない。 */
        return DISPATCHER_ERR_BAD_STATE;
    }

    if (result != 0) {
        /* task層のその他errorはdispatcherの入力不正として丸める。 */
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

/**
 * @brief dispatcherが保持しているcurrent taskを読み取り専用で返す。
 *
 * @details
 * current taskがまだcommitされていない場合はNULLを返す。呼び出し側は返された
 * pointerを観測用として扱い、TCB更新はtask moduleの明示的APIへ委譲する。
 *
 * @return current taskへの読み取り専用pointer。未設定時はNULL。
 */
const tcb_t *dispatcher_get_current(void)
{
    /*
     * current_taskはdispatcherだけが更新する。
     * 呼び出し側には読み取り専用pointerだけを返し、TCB変更はtask層APIへ集約する。
     */
    return current_task;
}
