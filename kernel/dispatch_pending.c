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

void dispatch_request_from_irq(const tcb_t *candidate)
{
    if (candidate == NULL) {
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
    dispatch_pending_state.requested = true;
    dispatch_pending_state.reason = DISPATCH_PENDING_FROM_IRQ;
    dispatch_pending_state.candidate = candidate;
}

bool dispatch_pending_is_requested(void)
{
    return dispatch_pending_state.requested;
}

void dispatch_pending_clear_for_test_or_later_boundary(void)
{
    dispatch_pending_state.requested = false;
    dispatch_pending_state.reason = DISPATCH_PENDING_NONE;
    dispatch_pending_state.candidate = NULL;
}

void dispatch_pending_log_state_from_irq(const char *not_requested_reason)
{
    if (!dispatch_pending_state.requested) {
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
     * requestedの場合も、候補task idを観測するだけに留める。
     * ここからdispatcher commitやcontext switchへ進めないことが8.3の境界である。
     */
    hal_console_write("[dispatch-pending] requested: source=");
    if (dispatch_pending_state.reason == DISPATCH_PENDING_FROM_IRQ) {
        hal_console_write("irq");
    } else {
        hal_console_write("unknown");
    }
    hal_console_write(" reason=higher-ready candidate id=");
    if (dispatch_pending_state.candidate == NULL) {
        hal_console_write("none");
    } else {
        dispatch_pending_write_int(dispatch_pending_state.candidate->id);
    }
    hal_console_write("\n");
}
