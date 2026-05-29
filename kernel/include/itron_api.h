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
 * 観測可能にし、RUNNING current taskだけをREADYへ戻した後、
 * schedulerで次READY候補を選び、協調API経由でdispatcher/context switch境界へ進む入口である。
 * timer IRQ、interrupt exit boundary、dispatch pending、preemptive switchへは接続しない。
 */

#ifndef ITRON_RTOS_ITRON_API_H
#define ITRON_RTOS_ITRON_API_H

/**
 * @brief `yield_tsk()` の観測成功を示す戻り値。
 *
 * @details
 * current taskが存在し、論理状態がRUNNINGであり、READYへ戻せた場合に返す。
 * これは「実切替成功」ではなく、「yield要求をREADY候補化し、
 * schedulerの次候補選択境界まで到達した」ことを示す。
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
 * current taskの観測情報を出力する。RUNNING current taskの場合はtask管理層を通じて
 * READYへ戻し、`scheduler_select_next()` で次READY候補を選ぶ。
 * 候補が存在する場合はdispatcher境界へ接続し、task_context層の協調switch smokeへ進む。
 * 候補が存在しない場合はswitchせず、no-nextとして観測を止める。
 *
 * @return `YIELD_TSK_OK` はRUNNING current taskのREADY化と次候補選択境界到達。
 *         負値はcurrent task未設定または非RUNNINGの不正状態。
 */
int yield_tsk(void);

#endif
