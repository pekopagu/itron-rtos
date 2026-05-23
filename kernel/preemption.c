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

    if (decision.current == NULL) {
        return "no-current";
    }

    if (decision.candidate == NULL) {
        return "no-ready";
    }

    return "candidate-not-higher";
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
        (decision.reason == SCHEDULER_PREEMPT_NEEDED) ? "switch-target" : "no-switch";

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
void preemption_evaluate_from_irq(void)
{
    const tcb_t *current;
    scheduler_preempt_decision_t decision;

    /*
     * dispatcher から current を読み取るだけに留める。ここで commit し直したり、
     * task state を補正したりしないため、8.2 の責務は decision 評価に限定される。
     */
    current = dispatcher_get_current();
    decision = scheduler_select_preemption_candidate(current);
    preemption_irq_log_decision(decision);
}
