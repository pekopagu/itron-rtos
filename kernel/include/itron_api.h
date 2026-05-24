/*
 * SPDX-License-Identifier: MIT
 */

/**
 * @file itron_api.h
 * @brief μITRON風API層の最小公開入口。
 *
 * @details
 * このヘッダは、学習用RTOSのkernel共通部へμITRON風API名を公開する。
 * 現段階の `yield_tsk()` は、実行中タスクからの自発的yield要求を
 * 観測可能にするための入口であり、実際のREADY復帰、次タスク選択、
 * dispatcher接続、CPU context switchはまだ行わない。
 */

#ifndef ITRON_RTOS_ITRON_API_H
#define ITRON_RTOS_ITRON_API_H

/**
 * @brief `yield_tsk()` の観測成功を示す戻り値。
 *
 * @details
 * current taskが存在し、論理状態がRUNNINGであることを確認できた場合に返す。
 * これは「実切替成功」ではなく、「yield要求をAPI層で観測した」ことだけを示す。
 */
#define YIELD_TSK_OK 0

/**
 * @brief `yield_tsk()` のcurrent task不正状態を示す戻り値。
 *
 * @details
 * dispatcherにcurrent taskが未設定、またはcurrent taskがRUNNINGではない場合に返す。
 * 負値を使うことで、観測成功値0と明確に区別する。
 */
#define YIELD_TSK_ERR_INVALID_CURRENT_STATE (-1)

/**
 * @brief μITRON風の自発的yield要求入口。
 *
 * @details
 * dispatcherが保持するcurrent taskを読み取り、HAL consoleへ呼び出し事実と
 * current taskの観測情報を出力する。RUNNING current taskの場合でも、
 * 現段階では `switch-not-connected-yet` として延期理由を記録し、
 * task状態、scheduler選択、dispatcher switch、context switchは実行しない。
 *
 * @return `YIELD_TSK_OK` はRUNNING current taskからの観測成功。
 *         負値はcurrent task未設定または非RUNNINGの不正状態。
 */
int yield_tsk(void);

#endif
