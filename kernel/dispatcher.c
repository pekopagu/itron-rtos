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
#include "hal/console.h"
#include "task.h"
#include "task_context.h"

/*
 * 現在タスクはdispatcherだけが保持する。
 * schedulerを「選択だけ」に保つことで、将来コンテキストスイッチや実行開始処理を
 * 追加するときも、選択結果を確定する責務をこの境界に集約できる。
 */
static const tcb_t *current_task;

/**
 * @brief dispatcher log用に符号付き整数を10進数で出力する。
 *
 * @details
 * freestanding環境ではprintfを使わないため、境界観測に必要な最小限の
 * 数値出力だけをdispatcher内に持つ。状態変更やtask選択は行わない。
 *
 * @param value 出力する整数。
 */
static void dispatcher_write_int(int value)
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

    if (magnitude == 0) {
        hal_console_putc('0');
        return;
    }

    while (magnitude > 0) {
        buffer[index++] = (char)('0' + (magnitude % 10U));
        magnitude /= 10U;
    }

    while (index > 0) {
        hal_console_putc(buffer[--index]);
    }
}

/**
 * @brief dispatcher boundary log用にtask識別情報を出力する。
 *
 * @details
 * NULL taskでもログの形を崩さないようにし、失敗時にfrom/toどちらが
 * 不正だったかを追跡しやすくする。TCBの状態変更は行わない。
 *
 * @param label `from` または `to` などの表示名。
 * @param task 表示対象task。NULLも許容する。
 */
static void dispatcher_log_task(const char *label, const tcb_t *task)
{
    hal_console_write(label);
    if (task == NULL) {
        hal_console_write(" none");
        return;
    }

    hal_console_write(" id=");
    dispatcher_write_int(task->id);
    hal_console_write(" name=");
    hal_console_write((task->name != NULL) ? task->name : "(unnamed)");
}

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

/**
 * @brief dispatcher層からtask context switch smokeへ進む境界を実行する。
 *
 * @details
 * 第9章9.2では、ここをdispatcherのswitch boundaryとして観測可能にする。
 * 実際のboot-time task-to-task smokeはtask_context層の補助APIへ委譲する。
 * この関数はdispatch pendingを消費せず、interrupt exit boundaryやtimer IRQ
 * handlerからも呼ばれない。第10章10.4では `yield_tsk()` がRUNNING currentを
 * READYへ戻した後にこの境界へ入るため、fromがREADY化済みの協調API経路も受け付ける。
 *
 * @param from 切替元task。
 * @param to 切替先task。
 * @return 成功時はDISPATCHER_OK、失敗時は負の値。
 */
int dispatcher_switch_to(tcb_t *from, tcb_t *to)
{
    int result;

    if (from == NULL || to == NULL) {
        hal_console_write("[dispatcher] switch boundary failed: reason=null-task");
        dispatcher_log_task(" from", from);
        dispatcher_log_task(" to", to);
        hal_console_write("\n");
        return DISPATCHER_ERR_INVAL;
    }

    if (from == to) {
        hal_console_write("[dispatcher] switch boundary failed: reason=same-task");
        dispatcher_log_task(" from", from);
        dispatcher_log_task(" to", to);
        hal_console_write("\n");
        return DISPATCHER_ERR_INVAL;
    }

    /*
     * 通常のdispatcher smokeではfromはRUNNINGで入る。
     * 10.4のyield経路だけは、API層が「RUNNING currentをREADYへ戻す」
     * という協調API固有の観測を先に済ませてからここへ来るため、READYも許可する。
     * DORMANT/WAITINGは実行中taskではなく、ここで受け付けると9.4のDORMANT確定や
     * wait状態の意味を壊すため拒否する。
     */
    /*
     * 第12章12.1では `wai_sem()` がRUNNING currentをWAITINGへ落としてから、
     * 次READY taskへの切替開始点としてこの境界へ到達する。WAITING fromは
     * すでに待ち入り済みなので、dispatcherではREADYへ戻さない。
     */
    if (from->state != TASK_STATE_RUNNING &&
        from->state != TASK_STATE_READY &&
        from->state != TASK_STATE_WAITING) {
        hal_console_write("[dispatcher] switch boundary failed: reason=from-not-running");
        dispatcher_log_task(" from", from);
        hal_console_write("\n");
        return DISPATCHER_ERR_BAD_STATE;
    }

    if (to->state != TASK_STATE_READY) {
        hal_console_write("[dispatcher] switch boundary failed: reason=to-not-ready");
        dispatcher_log_task(" to", to);
        hal_console_write("\n");
        return DISPATCHER_ERR_BAD_STATE;
    }

    hal_console_write("[dispatcher] switch boundary begin:");
    dispatcher_log_task(" from", from);
    dispatcher_log_task(" to", to);
    hal_console_write("\n");

    /*
     * 第9章9.3では、dispatcherの実切替境界へRUNNING/READY状態遷移を
     * 接続する。第10章10.4のyield経路では、API層がRUNNING currentをREADYへ
     * 戻してからここへ来るため、fromがすでにREADYの場合は再度READY化しない。
     * DORMANT最終化はtask_context層のlifecycle確定として分離する。
     * dispatch pending消費、interrupt exit接続、timer IRQからの実切替はまだ行わない。
     */
    if (from->state == TASK_STATE_RUNNING) {
        hal_console_write("[dispatcher] state transition:");
        dispatcher_log_task(" from", from);
        hal_console_write(" RUNNING->READY\n");
        from->state = TASK_STATE_READY;
    }

    hal_console_write("[dispatcher] state transition:");
    dispatcher_log_task(" to", to);
    hal_console_write(" READY->RUNNING\n");
    to->state = TASK_STATE_RUNNING;
    current_task = to;

    if (from->state == TASK_STATE_WAITING) {
        /*
         * `wai_sem()` 経路ではfrom taskはすでにWAITINGなので、task entryを
         * 再実行しない。12.1では次READY taskへ進む入口の観測が目的であり、
         * WAITING taskの復帰、wait queue、timeout、wakeup後preemptionはまだ扱わない。
         */
        result = task_context_prepare_initial_frame(to);
        if (result == DISPATCHER_OK) {
            result = task_context_switch_to_task(to);
        }
        hal_console_write("[dispatcher] switch boundary end: result=");
        dispatcher_write_int(result);
        hal_console_write("\n");
        return result;
    }

    /*
     * dispatcherはswitch境界までを担当し、実際のstack frame準備とarch context switchは
     * task_context層へ委譲する。timer IRQやdispatch pendingからここへ来たわけではないため、
     * interrupt-time状態は消費しない。
     */
    result = task_context_switch_to_task_pair(from, to);

    hal_console_write("[dispatcher] switch boundary end: result=");
    dispatcher_write_int(result);
    hal_console_write("\n");

    return result;
}
