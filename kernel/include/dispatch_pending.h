/*
 * SPDX-License-Identifier: MIT
 */

/**
 * @file dispatch_pending.h
 * @brief dispatch pending を要求・観測・後段消費するための kernel 側境界。
 *
 * @details
 * 第8章8.3では、dispatch pending を「将来のdispatch要求が保留された」
 * という論理状態として導入する。ただし、この状態がtrueになっても
 * 第11章11.2では、保存済みのfrom/toをinterrupt exit boundary側で一度だけ
 * consumeし、妥当な場合だけdispatcherのtask-to-task switch境界へ接続する。
 * ただし、timer IRQ handler本体から直接dispatcherやyield APIを呼ぶ形にはしない。
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
    DISPATCH_PENDING_FROM_IRQ, /**< timer IRQ由来のpreemption decisionがdispatchを要求した。 */
    DISPATCH_PENDING_TASK_START /**< `sta_tsk()` によるREADY化後のdispatch要求。 */
} dispatch_pending_reason_t;

/**
 * @struct dispatch_pending_snapshot_t
 * @brief 後段dispatch境界で消費するpending要求の最小snapshot。
 *
 * @details
 * pending state本体はdispatch_pending moduleが所有する。consume処理では、
 * request時に保存したreason/from/toの識別情報をこのsnapshotへ写し、
 * switch前にtask管理層から更新可能TCBを取り直す。保存済みpointerをそのまま
 * dispatcherへ渡さないことで、境界間で状態が変化した場合も検証できる。
 */
typedef struct {
    dispatch_pending_reason_t reason; /**< pending要求理由。 */
    int from_task_id;                 /**< 切替元としてrequestされたtask id。 */
    int to_task_id;                   /**< 切替先としてrequestされたtask id。 */
} dispatch_pending_snapshot_t;

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
 * @brief `sta_tsk()` 起点のdispatch pending要求を記録する。
 *
 * @details
 * task文脈APIでDORMANT taskをREADYへ起動した後、高優先度READY taskが見つかった事実を
 * 後段dispatch境界へ渡すためのAPIである。ここではdispatcherを呼ばず、task状態も変更しない。
 *
 * @param current dispatch要求元として観測する現在RUNNING task。
 * @param candidate 起動によりREADY候補になった高優先度task。
 */
void dispatch_request_from_task_start(const tcb_t *current, const tcb_t *candidate);

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

/**
 * @brief interrupt exit後段境界でpendingを一度だけ消費し、妥当ならdispatcherへ接続する。
 *
 * @details
 * 第11章11.2の到達点である。timer IRQ handler本体から直接
 * `dispatcher_switch_to()` を呼ばず、exit boundaryがこのAPIへ委譲する。
 * pendingがない場合は何も切り替えず、from/toが不正な場合も安全側にclearする。
 * 第11章11.3では、同一優先度READYだけでpendingが作られない場合も正当なno-pendingとして扱い、
 * `consume skipped: reason=no-pending` の観測だけで終了する。
 * これは完全な割り込み復帰frame切替ではなく、既存のtask-to-task context switch
 * smokeへ接続する教育用の後段dispatch境界である。
 *
 * @return dispatcher_switch_to()へ進んだ場合はその戻り値。no-pendingやinvalid時は負値。
 */
/**
 * @brief interrupt exit後段境界でpendingを一度だけ消費し、必要ならdispatcherへ接続する。
 * @details
 * 第11章11.4では、同一pending requestのrequestedログを1回だけに固定したうえで、
 * consume、dispatcher switch、clearの順序を安定させる。これはserial logの順序と
 * 重複を安定化するための契約であり、同一優先度time slice、round-robin、
 * semaphore wakeup、nested interrupt、完全な割り込み復帰frame切替はまだ扱わない。
 *
 * @return dispatcher_switch_to()へ進んだ場合はその戻り値。no-pendingやinvalid時は負値。
 */
int dispatch_pending_consume_at_deferred_boundary(void);

#endif
