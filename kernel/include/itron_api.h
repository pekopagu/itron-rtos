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

#include "task.h"

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

/** @brief `cre_tsk()` の成功を示す戻り値。 */
#define CRE_TSK_OK 0

/** @brief `cre_tsk()` の不正引数を示す戻り値。 */
#define CRE_TSK_ERR_INVAL (-1)

/** @brief `cre_tsk()` のtask登録失敗を示す戻り値。 */
#define CRE_TSK_ERR_TASK (-2)

/** @brief `sta_tsk()` の成功を示す戻り値。 */
#define STA_TSK_OK 0

/** @brief `sta_tsk()` の不正task IDを示す戻り値。 */
#define STA_TSK_ERR_INVAL (-1)

/** @brief `sta_tsk()` の対象task未検出を示す戻り値。 */
#define STA_TSK_ERR_NOT_FOUND (-2)

/** @brief `sta_tsk()` の対象task状態不正を示す戻り値。 */
#define STA_TSK_ERR_BAD_STATE (-3)

/** @brief `slp_tsk()` の成功を示す戻り値。 */
#define SLP_TSK_OK 0

/** @brief `slp_tsk()` のcurrent task不正状態を示す戻り値。 */
#define SLP_TSK_ERR_INVALID_CURRENT_STATE (-1)

/** @brief `slp_tsk()` の状態遷移またはswitch失敗を示す戻り値。 */
#define SLP_TSK_ERR_DISPATCH (-2)

/** @brief `wup_tsk()` の成功を示す戻り値。 */
#define WUP_TSK_OK 0

/** @brief `wup_tsk()` の不正task IDを示す戻り値。 */
#define WUP_TSK_ERR_INVAL (-1)

/** @brief `wup_tsk()` の対象task未検出を示す戻り値。 */
#define WUP_TSK_ERR_NOT_FOUND (-2)

/** @brief `wup_tsk()` の対象task状態不正を示す戻り値。 */
#define WUP_TSK_ERR_BAD_STATE (-3)

/**
 * @struct itron_task_create_param_t
 * @brief `cre_tsk()` へ渡す学習用task生成属性。
 *
 * @details
 * 14.1ではtask生成に必要な最小属性だけを扱う。`tskatr` や `stacd`、動的stack確保は
 * 本章の対象外であり、呼び出し側が用意したentry/priority/stack/nameをTCBへ登録する。
 */
typedef struct {
    task_entry_t entry;       /**< task入口関数。 */
    int priority;             /**< scheduler比較用優先度。数値が小さいほど高優先度。 */
    void *stack_base;         /**< 呼び出し側が用意したstack基底アドレス。 */
    unsigned long stack_size; /**< stack領域サイズ。 */
    const char *name;         /**< task名。 */
} itron_task_create_param_t;

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
 * @brief μITRON風のtask生成API。
 *
 * @details
 * 指定IDのtask定義をTCBへ登録し、初期状態をDORMANTにする。生成直後のtaskは
 * scheduler READY候補に入らない。task起動は `sta_tsk()` が担当する。
 *
 * @param tskid 生成対象task ID。0以下は不正。
 * @param pk_ctsk task生成属性。NULLや必須field欠落は不正。
 * @return 成功時はCRE_TSK_OK、失敗時はCRE_TSK_ERR_*。
 */
int cre_tsk(int tskid, const itron_task_create_param_t *pk_ctsk);

/**
 * @brief μITRON風のtask起動API。
 *
 * @details
 * DORMANT taskだけをREADYへ遷移させる。READY化後、現在RUNNING taskより高優先度の
 * READY候補になった場合は既存のdispatch pending境界へ接続する。DORMANT以外の状態は
 * 変更せずエラーにする。
 *
 * @param tskid 起動対象task ID。
 * @return 成功時はSTA_TSK_OK、失敗時はSTA_TSK_ERR_*。
 */
int sta_tsk(int tskid);

/**
 * @brief μITRON風のsleep待ちAPI。
 *
 * @details
 * dispatcherが保持するcurrent taskを読み取り、RUNNING current taskだけをsleep理由の
 * WAITINGへ遷移させる。sleep待ちへ入ったtaskはscheduler READY候補から外れ、
 * 次READY taskが存在する場合は既存の `dispatcher_switch_to()` 境界へ進む。
 *
 * timeout付きsleep、wakeup要求カウント、timer IRQ handlerからの呼び出しは扱わない。
 *
 * @return 成功時はSLP_TSK_OK、失敗時はSLP_TSK_ERR_*。
 */
int slp_tsk(void);

/**
 * @brief μITRON風のsleep待ちtask起床API。
 *
 * @details
 * 指定taskがWAITINGかつsleep理由の場合だけREADYへ戻す。DORMANT、READY、RUNNING、
 * semaphore待ち、delay待ち、timeout付きsemaphore待ちは状態を変更せずエラーにする。
 * READY化したtaskがcurrentより高優先度の場合は、既存のdispatch pending境界へ接続する。
 *
 * wakeup要求蓄積、timeout付きsleep、timer IRQ handlerからの呼び出しは扱わない。
 *
 * @param tskid 起床対象task ID。
 * @return 成功時はWUP_TSK_OK、失敗時はWUP_TSK_ERR_*。
 */
int wup_tsk(int tskid);

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
