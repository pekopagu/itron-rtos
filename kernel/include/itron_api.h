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

#include <stdint.h>

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
 * @brief `wai_sem()` の観測成功を示す戻り値。
 *
 * @details
 * count取得成功、またはRUNNING current taskをWAITINGへ落として
 * 次READY taskへのdispatcher境界へ到達した場合に返す。
 */
#define WAI_SEM_OK 0

/**
 * @brief `wai_sem()` のcurrent task不正状態を示す戻り値。
 *
 * @details
 * currentが未確定、またはRUNNINGではない状態から呼ばれた場合に返す。
 * `wai_sem()` はtask文脈APIであり、timer IRQ handlerからは呼ばない。
 */
#define WAI_SEM_ERR_INVALID_CURRENT_STATE (-1)

/**
 * @brief `wai_sem()` のsemaphore不正状態を示す戻り値。
 *
 * @details
 * sem_idが存在しない場合など、semaphore table側で取得対象を解決できない
 * 場合に返す。
 */
#define WAI_SEM_ERR_SEMAPHORE (-2)

/**
 * @brief `wai_sem()` のtask遷移またはswitch失敗を示す戻り値。
 *
 * @details
 * RUNNING->WAITING遷移やdispatcher境界への接続で失敗した場合に返す。
 */
#define WAI_SEM_ERR_DISPATCH (-3)

/**
 * @brief `dly_tsk()` の観測成功を示す戻り値。
 *
 * @details
 * RUNNING current taskをdelay WAITINGへ遷移させ、次READY taskがある場合は
 * dispatcher/context switch境界へ到達したことを示す。delay満了によるREADY復帰は
 * 13.4のdelay queue tick処理が担当する。
 */
#define DLY_TSK_OK 0

/**
 * @brief `dly_tsk()` の不正delay指定を示す戻り値。
 *
 * @details
 * 13.1では `delay_ticks == 0` をno-opではなくエラーとして扱う。エラー時は
 * current taskの状態、待ち理由、delay残tickを変更しない。
 */
#define DLY_TSK_ERR_INVALID_DELAY (-1)

/**
 * @brief `dly_tsk()` のcurrent task不正状態を示す戻り値。
 *
 * @details
 * dispatcher currentが存在しない、またはRUNNINGではない場合に返す。`dly_tsk()` は
 * task文脈APIであり、timer IRQ handler本体から呼ぶAPIではない。
 */
#define DLY_TSK_ERR_INVALID_CURRENT_STATE (-2)

/**
 * @brief `dly_tsk()` の状態遷移またはswitch失敗を示す戻り値。
 *
 * @details
 * RUNNING->WAITING遷移、task再取得、dispatcher switch境界で失敗した場合に返す。
 */
#define DLY_TSK_ERR_DISPATCH (-3)

/**
 * @brief `twai_sem()` の観測成功を示す戻り値。
 *
 * @details
 * semaphore countを即時取得した場合、またはtimeout付きsemaphore待ちとして
 * queue登録と次READY taskへのswitch境界まで到達した場合に返す。
 */
#define TWAI_SEM_OK 0

/**
 * @brief `twai_sem()` の不正timeout指定を示す戻り値。
 *
 * @details
 * 13.3では `timeout_ticks == 0` をpoll相当として扱わず、invalid timeoutとして拒否する。
 */
#define TWAI_SEM_ERR_INVALID_TIMEOUT (-1)

/**
 * @brief `twai_sem()` のcurrent task不正状態を示す戻り値。
 */
#define TWAI_SEM_ERR_INVALID_CURRENT_STATE (-2)

/**
 * @brief `twai_sem()` のsemaphore不正またはsemaphore queue失敗を示す戻り値。
 */
#define TWAI_SEM_ERR_SEMAPHORE (-3)

/**
 * @brief `twai_sem()` のdelay queue登録不可を示す戻り値。
 */
#define TWAI_SEM_ERR_DELAY_QUEUE (-4)

/**
 * @brief `twai_sem()` の状態遷移またはdispatcher接続失敗を示す戻り値。
 */
#define TWAI_SEM_ERR_DISPATCH (-5)

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

/**
 * @brief μITRON風のsemaphore待ちAPI入口。
 *
 * @details
 * dispatcherが保持するcurrent taskをtask文脈の呼び出し元として扱う。
 * countが残っていればcountを減らしてswitchしない。countが0なら
 * RUNNING current taskをWAITINGへ落とし、schedulerで次READY taskを選び、
 * READY taskが存在する場合だけ既存の `dispatcher_switch_to()` 境界へ進む。
 *
 * この12.1入口は `sig_sem()`、wait queue、timeout、wakeup後preemption、
 * 同一優先度time slice、round-robinを実装しない。timer IRQ handler本体から
 * 呼ぶAPIでもない。
 *
 * @param sem_id 対象semaphore ID。
 * @return 成功時はWAI_SEM_OK。失敗時はWAI_SEM_ERR_*。
 */
int wai_sem(int sem_id);

/**
 * @brief μITRON風のdelay待ちAPI入口。
 *
 * @details
 * dispatcherが保持するcurrent taskをtask文脈の呼び出し元として扱い、
 * `delay_ticks > 0` の場合だけRUNNING current taskをdelay理由のWAITINGへ遷移させる。
 * その後、既存schedulerで次READY taskを選択し、存在すれば既存
 * `dispatcher_switch_to()` 境界へ進む。
 *
 * tick到達時READY復帰は13.4のdelay queue tick処理が担当する。
 * timer IRQ handlerからの呼び出しは引き続き扱わない。
 *
 * @param delay_ticks delay待ちとして観測するtick数。0は不正。
 * @return 成功時はDLY_TSK_OK、失敗時はDLY_TSK_ERR_*。
 */
/**
 * @brief 13.2時点のdelay queue接続済みdelay待ちAPI。
 *
 * @details
 * `dly_tsk(delay_ticks > 0)` はRUNNING current taskをdelay WAITINGへ落とし、
 * `wait_reason=delay` とremaining tickを保持したうえで専用delay queueへ登録する。
 * queue満杯や二重登録ではWAITING化前に失敗し、不整合なWAITING taskを残さない。
 *
 * tick decrement、tick到達時READY復帰、delay queueからのdequeue wakeup、
 * timeout付きsemaphore待ちのtimeout満了処理は13.4のdelay queue tick処理が担当する。
 * timer IRQ handlerからの呼び出しは引き続き扱わない。
 *
 * @param delay_ticks delay queueへ観測用に登録するtick数。0は不正。
 * @return 成功時は `DLY_TSK_OK`、失敗時は `DLY_TSK_ERR_*`。
 */
int dly_tsk(uint32_t delay_ticks);

/**
 * @brief 13.3時点のtimeout付きsemaphore待ちAPI。
 *
 * @details
 * `timeout_ticks > 0` のtask文脈APIとして扱う。対象semaphoreのcountが残っている場合は
 * countを1つ取得して即時成功し、queue登録やcontext switchは行わない。
 *
 * countが0の場合は、WAITING化前にsemaphore wait queueとdelay queueの両方へ登録可能かを
 * 確認する。両方が登録可能な場合だけcurrent RUNNING taskを
 * `TASK_WAIT_REASON_SEMAPHORE_TIMEOUT` のWAITINGへ遷移させ、semaphore wait queueには
 * `sig_sem()` のwakeup対象として、delay queueにはtimeout tick観測対象として登録する。
 *
 * 13.4ではtickごとのtimeout decrement、timeout到達時READY復帰、timeout時のsemaphore
 * wait queue削除をdelay queue tick処理へ接続する。`sig_sem()` 成功時のdelay queue削除は
 * まだ行わない。timer IRQ handler本体から呼び出すAPIでもない。
 *
 * @param sem_id 対象semaphore ID。
 * @param timeout_ticks timeout観測用tick数。0はinvalid timeout。
 * @return 成功時は `TWAI_SEM_OK`。失敗時は `TWAI_SEM_ERR_*`。
 */
int twai_sem(int sem_id, uint32_t timeout_ticks);

#endif
