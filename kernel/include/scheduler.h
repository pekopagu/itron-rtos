/*
 * SPDX-License-Identifier: MIT
 */

/**
 * @file scheduler.h
 * @brief 簡易優先度スケジューラAPI定義（第3章3.2）
 *
 * @details
 * μITRON風RTOSの学習用として、READY状態のタスクから次に実行対象とみなす
 * TCBを1つ選択するためのAPIを定義する。第3章3.2のschedulerは「選択のみ」を担当し、
 * task entry呼び出し、RUNNING遷移、スタック切り替え、コンテキストスイッチ、
 * 割り込み、タイマ、プリエンプションは行わない。
 *
 * schedulerはHAL、arch、serialへ依存しない。選択結果のログ出力は呼び出し側が
 * HAL console経由で行い、依存方向 kernel -> HAL -> arch を維持する。
 */

#ifndef ITRON_RTOS_SCHEDULER_H
#define ITRON_RTOS_SCHEDULER_H

#include "task.h"

/**
 * @enum scheduler_preempt_reason_t
 * @brief タイマ契機プリエンプション判断の結果分類。
 *
 * @details
 * 第6章6.3のpreemption foundationで使う「判断のみ」の値である。
 * この値を返しても、新しいcurrent taskの確定、RUNNING状態の更新、
 * dispatcherやcontext switch層の呼び出しは行わない。
 * 将来の割り込み復帰時dispatcherが同種の判断を消費する可能性はあるが、
 * 現段階では判断結果を観測可能にすることだけを目的とする。
 */
typedef enum {
    SCHEDULER_PREEMPT_NONE = 0,       /**< 現在taskより高優先度のREADY taskが存在しない。 */
    SCHEDULER_PREEMPT_NEEDED,         /**< 高優先度READY taskを切り替え候補として扱える。 */
    SCHEDULER_PREEMPT_SAME_PRIORITY,  /**< 同一優先度READYのみで、11.3ではtime slice対象にしない。 */
    SCHEDULER_PREEMPT_INVALID_CURRENT /**< 渡されたcurrent taskをRUNNING基準として使えない。 */
} scheduler_preempt_reason_t;

/**
 * @struct scheduler_preempt_decision_t
 * @brief schedulerが返す読み取り専用のプリエンプション判断結果。
 *
 * @details
 * schedulerの責務は選択と比較だけである。`current` と `candidate`
 * はTCBを観測するために借りた読み取り専用ポインタとして扱う。
 * current taskの確定はdispatcherの責務に残し、register save/restoreは
 * context switch層の責務に残す。RUNNINGはこの教育用boot-time modelでは
 * 論理状態であり、CPUがそのtask stack上で実行中であることを保証しない。
 */
typedef struct {
    scheduler_preempt_reason_t reason; /**< 判断結果の分類。 */
    const tcb_t *current;              /**< 比較基準にしたcurrent task。存在しない場合はNULL。 */
    const tcb_t *candidate;            /**< 高優先度READY候補。存在しない場合はNULL。 */
} scheduler_preempt_decision_t;

/**
 * @brief 簡易スケジューラを初期化する。
 *
 * @details
 * 第3章3.2ではscheduler自身にREADYキューや現在実行中タスクを持たせないため、
 * この関数は将来拡張用の初期化境界として提供する。動的メモリ確保やHAL出力は行わない。
 *
 * @param なし。
 * @return なし。
 * @note task_start、コンテキストスイッチ、割り込み、タイマは導入しない。
 */
void scheduler_init(void);

/**
 * @brief READY状態の最高優先度タスクを選択する。
 *
 * @details
 * `task_get_count()` と `task_get_by_index()` を使ってtask tableを先頭から走査する。
 * `state == TASK_STATE_READY` のTCBだけを候補にし、priorityの数値が最も小さいタスクを
 * 返す。同一priorityの場合は先に見つかったTCBを維持するため、現在の固定テーブルでは
 * 登録順に近い順序で選択される。READYタスクが存在しない場合はNULLを返す。
 *
 * この関数は第3章3.2の「選択のみ」のAPIであり、TCBやtask_tableを書き換えない。
 * task entryを呼ばず、RUNNING状態へ変更せず、スタック切り替えやコンテキストスイッチも
 * 行わない。μITRON風の優先度規則を確認する足場として設計し、HAL/arch/serialにも依存しない。
 *
 * @param なし。
 * @return 選択されたTCBへの読み取り専用ポインタ。READYタスクがなければNULL。
 */
const tcb_t *scheduler_select_next(void);

/**
 * @brief 指定されたcurrent taskをプリエンプトすべきREADY taskを選択する。
 *
 * @details
 * このhelperは第6章6.3におけるscheduler側の境界である。
 * 論理的なcurrent taskと最高優先度READY taskを比較し、判断結果だけを返す。
 * 意図的にTCB状態を変更せず、dispatcherのcurrent pointerを書き換えず、
 * timer codeを呼ばず、context switchも実行しない。
 *
 * `candidate->priority < current->priority` の場合だけ候補を返す。
 * 第11章11.3では、同一priorityのREADY taskが存在してもtime slice対象とはみなさず、
 * `SCHEDULER_PREEMPT_SAME_PRIORITY` として明示する。これは「候補なし」と曖昧にせず、
 * 将来のround-robin、tick count slice管理、同一優先度順序管理をまだ導入しないことを
 * ログから確認するための教育用境界である。
 *
 * @param current dispatcherが所有するcurrent taskの観測値。NULLの場合は比較対象なし。
 * @return 高優先度READY候補の有無を表す判断結果。
 */
scheduler_preempt_decision_t scheduler_select_preemption_candidate(const tcb_t *current);

#endif
