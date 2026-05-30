/*
 * SPDX-License-Identifier: MIT
 */

/**
 * @file dispatch_pending.h
 * @brief dispatch pending を観測するための kernel 側境界。
 *
 * @details
 * 第8章8.3では、dispatch pending を「将来のdispatch要求が保留された」
 * という論理状態として導入する。ただし、この状態がtrueになっても
 * dispatcher commit、context switch、stack切り替え、register復元、
 * task状態変更は行わない。
 *
 * 状態管理はkernel common側に閉じ込める。これによりarch/x86_64側は
 * schedulerやdispatcherの内部構造へ依存せず、public APIだけで
 * dispatch pendingの観測点に到達できる。
 */

#ifndef ITRON_RTOS_DISPATCH_PENDING_H
#define ITRON_RTOS_DISPATCH_PENDING_H

#include "task.h"

#include <stdbool.h>

/**
 * @enum dispatch_pending_reason_t
 * @brief dispatch pending が要求された理由。
 */
typedef enum {
    DISPATCH_PENDING_NONE = 0, /**< dispatch要求は保留されていない。 */
    DISPATCH_PENDING_FROM_IRQ  /**< timer IRQ由来のpreemption decisionがdispatchを要求した。 */
} dispatch_pending_reason_t;

/**
 * @brief IRQ由来のdispatch要求を、実dispatchせずに記録する。
 *
 * @details
 * validation logで観測するため、論理的なpending要求、要求元current task、
 * 候補taskを保存する。
 * このAPIはdispatcherを呼ばず、stackを切り替えず、register保存・復元も
 * task状態変更も行わない。
 *
 * @param current dispatch要求元として観測する現在RUNNING task。
 * @param candidate preemption decision が選んだ読み取り専用の候補task。
 */
void dispatch_request_from_irq(const tcb_t *current, const tcb_t *candidate);

/**
 * @brief dispatch要求が現在保留されているかを返す。
 *
 * @return dispatch要求が記録されていればtrue、なければfalse。
 */
bool dispatch_pending_is_requested(void);

/**
 * @brief smoke検証または将来の境界用にdispatch pending状態をclearする。
 *
 * @details
 * 第8章8.3には実際のdispatch consumerがまだ存在しない。このclear APIは、
 * validationで状態を再利用しやすくし、将来のinterrupt-exit境界がpending要求を
 * 消費する位置を先に明示するために置く。
 */
void dispatch_pending_clear_for_test_or_later_boundary(void);

/**
 * @brief timer IRQ経路から現在のdispatch pending状態をlogへ出力する。
 *
 * @details
 * このlogはvalidation専用である。通常boot logへ混ざる可能性があり、
 * interrupt-safe logging基盤の完成を意味しない。要求が保留されていない場合、
 * 呼び出し側は「なぜsetしなかったか」を表すpreemption decision reasonを渡す。
 *
 * @param not_requested_reason not-requested時に表示する最小限の理由文字列。
 */
void dispatch_pending_log_state_from_irq(const char *not_requested_reason);

#endif
