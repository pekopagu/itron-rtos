/*
 * SPDX-License-Identifier: MIT
 */

/**
 * @file dispatch_pending.c
 * @brief kernel-owned dispatch pending状態と第11章11.2の後段消費境界。
 *
 * @details
 * 第11章11.1までは、論理的なpending要求だけを観測していた。第11章11.2では
 * request済みfrom/toをinterrupt exit後段境界で一度だけconsumeし、妥当な場合だけ
 * 既存のdispatcher/task_context switch smokeへ接続する。ただし、これは完全な
 * 割り込み復帰frame切替ではなく、nested interruptやtime sliceも扱わない。
 * 第11章11.3では、同一優先度READYだけの場合にpendingを作らないことを
 * no-pending consumeログで確認する。
 */

#include "dispatch_pending.h"

#include "dispatcher.h"
#include "hal/console.h"
#include "task.h"

#include <stddef.h>

typedef enum {
    DISPATCH_PENDING_SOURCE_NONE = 0,
    DISPATCH_PENDING_SOURCE_IRQ,
    DISPATCH_PENDING_SOURCE_TASK
} dispatch_pending_source_t;

typedef struct {
    bool requested;
    bool request_logged;
    dispatch_pending_source_t source;
    dispatch_pending_reason_t reason;
    const tcb_t *current;
    const tcb_t *candidate;
    int current_id;
    int candidate_id;
} dispatch_pending_state_t;

static dispatch_pending_state_t dispatch_pending_state;

#define DISPATCH_PENDING_CONSUME_NO_PENDING (-1)
#define DISPATCH_PENDING_CONSUME_INVALID    (-2)

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
 * @brief dispatch pending reasonをvalidation log用文字列へ変換する。
 *
 * @details
 * 11.2ではconsume logとrequested logで同じreason表記を使う。
 * enum値を外部へ漏らさず、このmodule内で表示責務を閉じる。
 *
 * @param reason 変換対象のpending reason。
 * @return HAL consoleへ渡す静的文字列。
 */
static const char *dispatch_pending_reason_name(dispatch_pending_reason_t reason)
{
    if (reason == DISPATCH_PENDING_FROM_IRQ) {
        return "higher-priority-ready";
    }

    if (reason == DISPATCH_PENDING_TASK_START) {
        return "task-start";
    }

    /* `wup_tsk()` 起点のREADY復帰はtask-wakeupとして観測する。 */
    if (reason == DISPATCH_PENDING_TASK_WAKEUP) {
        return "task-wakeup";
    }

    return "none";
}

/**
 * @brief pending stateを初期化し、必要に応じてclear理由をlogへ出す。
 *
 * @details
 * pending consumeは一度だけ行うため、valid/invalidどちらでも最終的にclearする。
 * preemption評価開始時の古いpending破棄ではlogを出さず、11.2の後段境界で
 * 消費結果を示す場合だけ理由を出す。
 *
 * @param log_reason NULL以外ならclear理由としてlogへ出す。
 */
static void dispatch_pending_clear_with_reason(const char *log_reason)
{
    dispatch_pending_state.requested = false;
    dispatch_pending_state.request_logged = false;
    dispatch_pending_state.source = DISPATCH_PENDING_SOURCE_NONE;
    dispatch_pending_state.reason = DISPATCH_PENDING_NONE;
    dispatch_pending_state.current = NULL;
    dispatch_pending_state.candidate = NULL;
    dispatch_pending_state.current_id = 0;
    dispatch_pending_state.candidate_id = 0;

    if (log_reason != NULL) {
        hal_console_write("[dispatch-pending] cleared: reason=");
        hal_console_write(log_reason);
        hal_console_write("\n");
    }
}

/**
 * @brief consumeしたpending snapshotをlogへ出力する。
 *
 * @details
 * from/toはrequest時の読み取り専用TCBから得た表示用identityである。
 * 実際にdispatcherへ渡すTCBは、この後にtask idから更新可能pointerを取り直す。
 *
 * @param snapshot consumeしたpending要求。
 */
static void dispatch_pending_log_consumed(
    const dispatch_pending_snapshot_t *snapshot
)
{
    hal_console_write("[dispatch-pending] consumed: reason=");
    hal_console_write(dispatch_pending_reason_name(snapshot->reason));
    hal_console_write(" from id=");
    dispatch_pending_write_int(snapshot->from_task_id);
    if (dispatch_pending_state.current != NULL) {
        hal_console_write(" name=");
        hal_console_write(
            (dispatch_pending_state.current->name != NULL) ?
                dispatch_pending_state.current->name : "(null)"
        );
    }
    hal_console_write(" to id=");
    dispatch_pending_write_int(snapshot->to_task_id);
    if (dispatch_pending_state.candidate != NULL) {
        hal_console_write(" name=");
        hal_console_write(
            (dispatch_pending_state.candidate->name != NULL) ?
                dispatch_pending_state.candidate->name : "(null)"
        );
    }
    hal_console_write("\n");
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
        dispatch_pending_clear_irq_for_evaluation();
        return;
    }

    /*
     * task文脈API由来のpendingは、IRQ評価が開始された事実だけで消費してはいけない。
     * 単一pendingのv1.0モデルでは、未消費のtask pendingがある間はIRQ由来requestで
     * 上書きせず、後段境界に既存requestを渡す。
     */
    if (dispatch_pending_state.requested &&
        dispatch_pending_state.source == DISPATCH_PENDING_SOURCE_TASK) {
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
    dispatch_pending_state.request_logged = false;
    dispatch_pending_state.source = DISPATCH_PENDING_SOURCE_IRQ;
    dispatch_pending_state.reason = DISPATCH_PENDING_FROM_IRQ;
    dispatch_pending_state.current = current;
    dispatch_pending_state.candidate = candidate;
    dispatch_pending_state.current_id = current->id;
    dispatch_pending_state.candidate_id = candidate->id;
}

/**
 * @brief `sta_tsk()` 起点のdispatch pending要求を記録する。
 *
 * @details
 * DORMANT taskをREADYへ起動した結果、高優先度READY候補が生まれたことを後段境界へ渡す。
 * ここではdispatcherやcontext switchを直接呼ばない。
 *
 * @param current dispatch要求元として観測する現在RUNNING task。
 * @param candidate 起動によりREADY候補になった高優先度task。
 */
void dispatch_request_from_task_start(const tcb_t *current, const tcb_t *candidate)
{
    if (current == NULL || candidate == NULL) {
        /*
         * sta_tsk()後のpreemption比較に必要なfrom/toが欠ける場合は、古いpendingを残さない。
         * dispatcherへfallbackせず、次の観測で誤った切替要求が消費されることを防ぐ。
         */
        dispatch_pending_clear_for_test_or_later_boundary();
        return;
    }

    /*
     * task文脈API由来のpending requestとして保存する。実際のdispatchは既存の後段境界が
     * from RUNNING / to READY を再確認してから扱う。
     */
    dispatch_pending_state.requested = true;
    dispatch_pending_state.request_logged = false;
    dispatch_pending_state.source = DISPATCH_PENDING_SOURCE_TASK;
    dispatch_pending_state.reason = DISPATCH_PENDING_TASK_START;
    dispatch_pending_state.current = current;
    dispatch_pending_state.candidate = candidate;
    dispatch_pending_state.current_id = current->id;
    dispatch_pending_state.candidate_id = candidate->id;
}

/**
 * @brief `wup_tsk()` 起点のdispatch pending要求を記録する。
 *
 * @details
 * sleep待ちtaskをREADYへ復帰した結果、高優先度READY候補が生まれたことを後段境界へ渡す。
 * ここではdispatcherやcontext switchを直接呼ばない。
 *
 * @param current dispatch要求元として観測する現在RUNNING task。
 * @param candidate wakeupによりREADY候補になった高優先度task。
 */
void dispatch_request_from_task_wakeup(const tcb_t *current, const tcb_t *candidate)
{
    if (current == NULL || candidate == NULL) {
        /*
         * wup_tsk()後のpreemption比較に必要なfrom/toが欠ける場合は、古いpendingを残さない。
         * dispatcherへfallbackせず、次の観測で誤った切替要求が消費されることを防ぐ。
         */
        dispatch_pending_clear_for_test_or_later_boundary();
        return;
    }

    /*
     * task文脈API由来のwakeup pending requestとして保存する。
     * 実際のdispatchは既存の後段境界がfrom RUNNING / to READYを再確認してから扱う。
     */
    dispatch_pending_state.requested = true;
    dispatch_pending_state.request_logged = false;
    dispatch_pending_state.source = DISPATCH_PENDING_SOURCE_TASK;
    dispatch_pending_state.reason = DISPATCH_PENDING_TASK_WAKEUP;
    dispatch_pending_state.current = current;
    dispatch_pending_state.candidate = candidate;
    dispatch_pending_state.current_id = current->id;
    dispatch_pending_state.candidate_id = candidate->id;
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
    dispatch_pending_clear_with_reason(NULL);
}

/**
 * @brief IRQ評価開始時にIRQ由来pendingだけを初期化する。
 *
 * @details
 * task文脈API由来のpendingはまだ後段境界で消費されていない可能性があるため、
 * timer IRQのpreemption評価入口では保持する。
 */
void dispatch_pending_clear_irq_for_evaluation(void)
{
    if (dispatch_pending_state.source == DISPATCH_PENDING_SOURCE_IRQ) {
        dispatch_pending_clear_with_reason(NULL);
    }
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
    if (dispatch_pending_state.request_logged) {
        /*
         * 11.4では同じpending requestを複数回観測しても、request event logは
         * 1回だけに固定する。これはログ安定化のための抑止であり、pendingの
         * 消費状態やdispatcherへの委譲可否は変更しない。
         */
        return;
    }

    hal_console_write("[dispatch-pending] requested: reason=");
    hal_console_write(dispatch_pending_reason_name(dispatch_pending_state.reason));
    hal_console_write(" from id=");
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
    dispatch_pending_state.request_logged = true;
}

/**
 * @brief pending状態をsnapshotとして取り出す。
 *
 * @details
 * この関数自体はclearしない。呼び出し側のconsume境界が、no-pending、
 * invalid、dispatch完了のどれで終わったかを決めてからclear理由を出す。
 *
 * @param snapshot 取り出し先。NULLは不正。
 * @return pendingがあればtrue、なければfalse。
 */
static bool dispatch_pending_take_snapshot(
    dispatch_pending_snapshot_t *snapshot
)
{
    if (snapshot == NULL || !dispatch_pending_state.requested) {
        return false;
    }

    snapshot->reason = dispatch_pending_state.reason;
    snapshot->from_task_id = dispatch_pending_state.current_id;
    snapshot->to_task_id = dispatch_pending_state.candidate_id;
    return true;
}

/**
 * @brief 後段dispatch境界でpendingを一度だけ消費し、妥当な場合だけ切替へ接続する。
 *
 * @details
 * 第11章11.2の中核となるconsume処理である。timer IRQ handler本体から直接
 * `dispatcher_switch_to()` を呼ばず、interrupt exit boundaryから委譲された後段処理として
 * pendingを取り出す。request時に保持したfrom/to idを使って更新可能TCBを取り直し、
 * fromがRUNNING、toがREADYである場合だけ既存のdispatcher task-to-task switch境界へ渡す。
 *
 * pendingが存在しない場合はswitchせず、no-pendingとして観測ログを出す。
 * from/toが見つからない、または状態が不正な場合もswitchせず、pendingをinvalidとして
 * clearする。valid dispatch後もpendingをclearし、二重dispatchを防ぐ。
 *
 * この処理は教育用の後段dispatch接続であり、完全な割り込み復帰frame切替、
 * nested interrupt、同一優先度time slice、semaphore wakeup連携はまだ扱わない。
 *
 * @return dispatcherへ進んだ場合は `dispatcher_switch_to()` の戻り値。
 *         pendingなし、または不正pendingの場合は負値。
 */
int dispatch_pending_consume_at_deferred_boundary(void)
{
    dispatch_pending_snapshot_t snapshot;
    tcb_t *from;
    tcb_t *to;
    int result;

    if (!dispatch_pending_take_snapshot(&snapshot)) {
        /*
         * pendingがない場合は正常なno-opとして扱う。
         * ここでdispatcherへ進むと、timer IRQごとに根拠のない切替を起こすため、
         * 観測ログだけを残して後段dispatchを終了する。
         * 11.3の同一優先度READY除外では、この経路が期待される正常なno-opである。
         */
        hal_console_write("[dispatch-pending] consume skipped: reason=no-pending\n");
        return DISPATCH_PENDING_CONSUME_NO_PENDING;
    }

    /*
     * snapshotを取れた時点で、このpendingは今回の後段境界が処理対象として引き受ける。
     * 以降は成功・失敗に関わらずclearし、同じrequestを二重にdispatchしない。
     */
    dispatch_pending_log_consumed(&snapshot);

    /*
     * 11.2では、request時の読み取り専用pointerをそのままdispatcherへ渡さない。
     * from/to idでtask tableから更新可能TCBを取り直し、現時点の状態を確認してから
     * task-to-task switch境界へ接続する。
     */
    from = task_get_mutable_by_id(snapshot.from_task_id);
    to = task_get_mutable_by_id(snapshot.to_task_id);
    if (from == NULL || from->state != TASK_STATE_RUNNING) {
        /*
         * request時点ではfromがRUNNINGでも、後段境界に来るまでに状態が変わる可能性がある。
         * RUNNINGでないfromをdispatcherへ渡すと、割り込み前後の実行主体を誤って扱うため、
         * pendingを破棄して安全側に倒す。
         */
        hal_console_write(
            "[dispatch-pending] consume rejected: reason=invalid-from-or-to detail=invalid-current\n"
        );
        dispatch_pending_clear_with_reason("invalid-pending");
        return DISPATCH_PENDING_CONSUME_INVALID;
    }

    if (to == NULL || to->state != TASK_STATE_READY) {
        /*
         * 切替先はREADY taskだけに限定する。
         * WAITING/DORMANT/RUNNINGへ進めるとschedulerの選択規則やtask lifecycleを壊すため、
         * 11.2では補正せずinvalid pendingとして消費終了する。
         */
        hal_console_write(
            "[dispatch-pending] consume rejected: reason=invalid-from-or-to detail=invalid-target\n"
        );
        dispatch_pending_clear_with_reason("invalid-pending");
        return DISPATCH_PENDING_CONSUME_INVALID;
    }

    /*
     * from/toが現在のtask table上でも妥当な場合だけ、既存のdispatcher境界へ接続する。
     * ここはtimer IRQ handler本体ではなく、interrupt exitから委譲された後段境界である。
     */
    result = dispatcher_switch_to(from, to);
    /*
     * dispatcher_switch_to()の結果に関わらず、pending要求そのものはこの境界で消費済みにする。
     * 失敗時にpendingを残すと、次のIRQや観測境界で同じ要求を再実行してしまうためである。
     */
    dispatch_pending_clear_with_reason("dispatch-completed");
    return result;
}
