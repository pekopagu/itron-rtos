/*
 * SPDX-License-Identifier: MIT
 */

/**
 * @file preemption.c
 * @brief IRQ 起点 preemption decision 境界の実装。
 *
 * @details
 * この module は timer IRQ handler から呼ばれる preemption decision の入口を提供する。
 * arch/x86_64 側へ scheduler/dispatcher の内部依存を漏らさないため、IRQ handler は
 * この public API だけを呼ぶ。ここで行うのは current の読み取り、既存 scheduler helper
 * による decision 評価、最小限の validation log だけであり、実際の dispatch や
 * context switch は行わない。
 */

#include "preemption.h"

#include "dispatch_pending.h"
#include "dispatcher.h"
#include "hal/console.h"
#include "scheduler.h"

#include <stddef.h>

/**
 * @brief preemption decision reason を IRQ validation log 用文字列へ変換する。
 *
 * @details
 * scheduler は小さな enum だけを返す。IRQ 起点の観測では、割り込み中ログを最小限に
 * 保つため、task 詳細ではなく reason だけを短い文字列にする。HAL console 依存は
 * scheduler へ入れず、この境界に閉じる。
 *
 * @param decision scheduler が返した読み取り専用 decision。
 * @return serial log 用の静的文字列。
 */
static const char *preemption_irq_reason_to_string(
    scheduler_preempt_decision_t decision
)
{
    if (decision.reason == SCHEDULER_PREEMPT_NEEDED) {
        return "higher-priority-ready";
    }

    if (decision.reason == SCHEDULER_PREEMPT_INVALID_CURRENT) {
        return "invalid-current";
    }

    if (decision.reason == SCHEDULER_PREEMPT_SAME_PRIORITY) {
        return "same-priority-not-timeslice-target";
    }

    if (decision.current == NULL) {
        return "no-current";
    }

    return "no-higher-priority-ready";
}

/**
 * @brief IRQ preemption log用に整数を10進出力する。
 *
 * @details
 * freestanding環境ではprintfを使わないため、11.1のtask id/priority観測に必要な
 * 最小限の変換だけをpreemption境界へ閉じ込める。
 *
 * @param value 出力する符号付き整数。
 * @return なし。
 */
static void preemption_irq_write_int(int value)
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
 * @brief task状態をIRQ preemption log用の固定文字列へ変換する。
 *
 * @details
 * timer IRQ中の観測ログを読みやすくするための表示専用helperである。
 * TCB状態は変更せず、未知の状態値は`UNKNOWN`として扱う。
 *
 * @param state 文字列へ変換するtask状態。
 * @return HAL consoleへ渡す固定文字列。
 */
static const char *preemption_irq_task_state_name(task_state_t state)
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
 * @brief 11.1のpreemption判定で観測するtask identityを出力する。
 *
 * @details
 * id/name/prio/stateだけを読み取り、TCB状態やdispatcher currentは変更しない。
 * timer IRQ中の検出証跡を残すための表示専用helperである。
 *
 * @param prefix ログ行の先頭に出力する分類文字列。
 * @param task 表示対象task。NULLの場合は`none`として出力する。
 * @return なし。
 */
static void preemption_irq_log_task(const char *prefix, const tcb_t *task)
{
    hal_console_write(prefix);
    if (task == NULL) {
        hal_console_write(" none\n");
        return;
    }

    hal_console_write(" id=");
    preemption_irq_write_int(task->id);
    hal_console_write(" name=");
    hal_console_write((task->name != NULL) ? task->name : "(null)");
    hal_console_write(" prio=");
    preemption_irq_write_int(task->priority);
    hal_console_write(" state=");
    hal_console_write(preemption_irq_task_state_name(task->state));
    hal_console_write("\n");
}

/**
 * @brief IRQ 起点 decision の最小限の観測ログを出力する。
 *
 * @details
 * 割り込み中の serial 出力は validation 専用であり、通常 boot log と同じ順序保証を
 * 持たない。そのため task id や名前の詳細は出さず、decision が評価された事実、
 * switch 候補の有無、理由だけを残す。
 *
 * @param decision scheduler が返した読み取り専用 decision。
 */
static void preemption_irq_log_decision(scheduler_preempt_decision_t decision)
{
    const char *result =
        (decision.reason == SCHEDULER_PREEMPT_NEEDED) ? "request-switch" : "no-switch";

    hal_console_write("[preempt-irq] decision evaluated: result=");
    hal_console_write(result);
    hal_console_write(" reason=");
    hal_console_write(preemption_irq_reason_to_string(decision));
    hal_console_write("\n");
}

/**
 * @brief timer IRQ から呼ばれる preemption decision 入口。
 *
 * @details
 * 第8章8.2の到達点を表す関数である。IRQ handler は `timer_tick()` の後に
 * この関数を呼び、既存の scheduler decision を評価した事実だけを観測する。
 * ここでは dispatcher から logical current task を読み取るが、current を
 * 再確定せず、task state も変更しない。decision が switch-target になっても、
 * 実際の dispatcher 呼び出し、context switch、dispatch pending 更新は
 * 第8章8.3以降へ残す。
 *
 * @param なし。
 * @return なし。
 */
/**
 * @brief IRQ由来のpreemption decisionを評価し、dispatch pendingを更新する。
 *
 * @details
 * この関数は第8章8.3の観測境界である。scheduler decisionがswitch-targetを
 * 示した場合だけdispatch pending requestを記録する。それ以外の結果では
 * dispatch pendingをclearしたままにし、IRQ handlerのvalidation logへ渡す
 * 短い理由文字列を返す。
 *
 * @return not-requested時の最小理由文字列。dispatch pending要求時はNULL。
 */
const char *preemption_evaluate_from_irq(void)
{
    const tcb_t *current;
    scheduler_preempt_decision_t decision;
    const char *not_requested_reason;

    /*
     * dispatcher から current を読み取るだけに留める。ここで commit し直したり、
     * task state を補正したりしないため、8.2 の責務は decision 評価に限定される。
     */
    dispatch_pending_clear_for_test_or_later_boundary();
    current = dispatcher_get_current();
    decision = scheduler_select_preemption_candidate(current);

    /*
     * 11.1では「何を基準に高優先度READYを探したか」を先に見せる。
     * currentはdispatcherから読むだけで、ここではRUNNING/READYを補正しない。
     */
    if (current != NULL) {
        preemption_irq_log_task("[preempt-irq] current:", current);
    }

    /*
     * schedulerの判断結果をIRQ向けの観測ログへ分解する。
     * 高優先度READYがある場合だけ候補taskを出し、それ以外は切り替えない理由を出す。
     */
    if (decision.reason == SCHEDULER_PREEMPT_NEEDED) {
        preemption_irq_log_task("[preempt-irq] higher-ready detected:", decision.candidate);
    } else {
        hal_console_write("[preempt-irq] no higher-ready: reason=");
        hal_console_write(preemption_irq_reason_to_string(decision));
        hal_console_write("\n");
    }

    preemption_irq_log_decision(decision);

    if (decision.reason == SCHEDULER_PREEMPT_NEEDED) {
        /*
         * ここで行うのはdispatch pendingの要求記録だけである。
         * timer IRQ中にdispatcher_switch_to()へ進まず、pending消費も行わない。
         */
        dispatch_request_from_irq(current, decision.candidate);
        return NULL;
    }

    /*
     * no-switch系はhandler側のpending観測へ理由だけを返す。
     * requestしていないことを明示するため、dispatch pending stateはclear済みのままにする。
     */
    not_requested_reason = preemption_irq_reason_to_string(decision);
    return not_requested_reason;
}
