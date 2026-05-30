/*
 * SPDX-License-Identifier: MIT
 */

/**
 * @file dispatch_pending.c
 * @brief 第8章8.3用の最小限のkernel-owned dispatch pending状態。
 *
 * @details
 * この実装は、論理的なpending要求だけを意図的に記録する。dispatcherではなく、
 * current taskのcommit、context switch、task状態変更は行わない。
 * 将来の章で本物のinterrupt-exit dispatch境界を導入する前に、IRQ由来の
 * preemption decision結果を観測できるようにするための状態である。
 */

#include "dispatch_pending.h"

#include "hal/console.h"

#include <stddef.h>

typedef struct {
    bool requested;
    dispatch_pending_reason_t reason;
    const tcb_t *current;
    const tcb_t *candidate;
} dispatch_pending_state_t;

static dispatch_pending_state_t dispatch_pending_state;

/**
 * @brief libcのformat機能を使わず、符号付き整数をHAL consoleへ出力する。
 *
 * @param value 10進数で出力する値。
 */
static void dispatch_pending_write_int(int value)
{
    char buffer[12];
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

    while (magnitude > 0U && index < (int)sizeof(buffer)) {
        buffer[index++] = (char)('0' + (magnitude % 10U));
        magnitude /= 10U;
    }

    while (index > 0) {
        hal_console_putc(buffer[--index]);
    }
}

/**
 * @brief IRQ由来のdispatch pending要求をfrom/to付きで記録する。
 *
 * @details
 * 第11章11.1では、高優先度READY検出の結果をpendingとして観測するだけに留める。
 * currentとcandidateはログ用の読み取り専用参照であり、この関数はdispatcherを呼ばず、
 * task状態、stack、register、pending消費状態を変更しない。
 *
 * @param current dispatch要求元として観測する現在RUNNING task。
 * @param candidate dispatch候補として観測する高優先度READY task。
 * @return なし。
 */
void dispatch_request_from_irq(const tcb_t *current, const tcb_t *candidate)
{
    if (current == NULL || candidate == NULL) {
        /*
         * from/toのどちらかが欠けた要求は11.1の観測対象として不完全である。
         * 古いpendingを残すと次のIRQ観測を誤読するため、ここで明示的に初期化する。
         */
        /*
         * switch-targetとして扱える候補がない場合は、防御的にpendingを残さない。
         * ここでdispatcherへfallbackしたり、task状態を補正したりしない。
         */
        dispatch_pending_clear_for_test_or_later_boundary();
        return;
    }

    /*
     * 第8章8.3では「要求が保留された」事実だけを保存する。
     * candidateはlog用の読み取り専用参照であり、ここでは実行対象へ切り替えない。
     */
    /*
     * 11.1では「高優先度READYを検出したので後で切替が必要」という事実だけを保存する。
     * current/candidateはログ用の読み取り専用参照であり、ここでは実行対象へ切り替えない。
     */
    dispatch_pending_state.requested = true;
    dispatch_pending_state.reason = DISPATCH_PENDING_FROM_IRQ;
    dispatch_pending_state.current = current;
    dispatch_pending_state.candidate = candidate;
}

/**
 * @brief dispatch pending要求が現在保持されているかを返す。
 *
 * @details
 * interrupt exit boundaryがpending状態を観測するための読み取り専用APIである。
 * この関数はpendingを消費せず、dispatcherやtask状態変更にも接続しない。
 *
 * @return pending要求が保持されていればtrue、なければfalse。
 */
bool dispatch_pending_is_requested(void)
{
    return dispatch_pending_state.requested;
}

/**
 * @brief 検証または将来境界用にdispatch pending状態を初期化する。
 *
 * @details
 * 11.1ではpending consumerがまだ存在しないため、preemption評価の入口で古い観測状態を
 * 残さない目的で使う。実dispatch完了を表すconsume処理ではない。
 *
 * @return なし。
 */
void dispatch_pending_clear_for_test_or_later_boundary(void)
{
    /*
     * 11.1ではpending consumerがまだないため、各preemption評価の入口で観測状態を単発化する。
     * これはdispatch完了通知ではなく、古い証跡を消すための初期化である。
     */
    dispatch_pending_state.requested = false;
    dispatch_pending_state.reason = DISPATCH_PENDING_NONE;
    dispatch_pending_state.current = NULL;
    dispatch_pending_state.candidate = NULL;
}

/**
 * @brief timer IRQ経路からdispatch pendingの観測ログを出力する。
 *
 * @details
 * pendingがrequestedの場合はfrom/to taskを出し、not-requestedの場合は理由を出す。
 * 出力後もpendingは消費せず、interrupt exit dispatchやcontext switchへは接続しない。
 *
 * @param not_requested_reason pending未要求時の理由文字列。requested時はNULLでよい。
 * @return なし。
 */
void dispatch_pending_log_state_from_irq(const char *not_requested_reason)
{
    if (!dispatch_pending_state.requested) {
        /*
         * no-switch系のdecisionではpendingをsetしない。
         * 理由をそのまま残すことで、同一優先度除外やinvalid-currentをrequested経路と混同しない。
         */
        /*
         * no-switch系のdecisionではpendingをsetしない。その理由をIRQ中の
         * validation logとして残し、EOI前の観測点に到達したことを確認する。
         */
        hal_console_write("[dispatch-pending] not-requested: reason=");
        if (not_requested_reason == NULL) {
            hal_console_write("none");
        } else {
            hal_console_write(not_requested_reason);
        }
        hal_console_write("\n");
        return;
    }

    /*
     * 11.1では要求元と候補先を観測できるようにする。
     * ここでもdispatcher commitやcontext switchには進めず、pendingを消費しない。
     */
    hal_console_write("[dispatch-pending] requested: reason=higher-priority-ready from id=");
    if (dispatch_pending_state.current == NULL) {
        hal_console_write("none");
    } else {
        dispatch_pending_write_int(dispatch_pending_state.current->id);
        hal_console_write(" name=");
        hal_console_write(
            (dispatch_pending_state.current->name != NULL) ?
                dispatch_pending_state.current->name : "(null)"
        );
    }
    hal_console_write(" to id=");
    if (dispatch_pending_state.candidate == NULL) {
        hal_console_write("none");
    } else {
        dispatch_pending_write_int(dispatch_pending_state.candidate->id);
        hal_console_write(" name=");
        hal_console_write(
            (dispatch_pending_state.candidate->name != NULL) ?
                dispatch_pending_state.candidate->name : "(null)"
        );
    }
    hal_console_write("\n");
}
