/*
 * SPDX-License-Identifier: MIT
 */

/**
 * @file itron_api.h
 * @brief μITRON風API層の公開入口と共通エラーコード定義。
 * @details
 * 第14章14.4では、APIごとにばらついていた戻り値を `ER` と共通の
 * `E_*` エラーコードへ整理する。task、semaphore、delay queue、
 * dispatcher、preemptionの内部状態遷移は既存moduleへ委譲し、この公開
 * API層では呼び出し元へ返す成功・失敗理由を統一する。
 */

#ifndef ITRON_RTOS_ITRON_API_H
#define ITRON_RTOS_ITRON_API_H

#include <stdint.h>

#include "task.h"

/** @brief μITRON風API層の戻り値型。 */
typedef int ER;

/** @brief μITRON風API層で公開するID型。 */
typedef int ID;

/** @brief 正常完了。 */
#define E_OK      (0)
/** @brief 不正ID。task IDまたはsemaphore IDが無効、または存在しない。 */
#define E_ID      (-18)
/** @brief 不正引数。NULL pointer、0 tick timeout、0 tick delayなど。 */
#define E_PAR     (-17)
/** @brief 不正文脈。今回判定できる範囲ではcurrent taskなし、または非RUNNING current。 */
#define E_CTX     (-25)
/** @brief 不正状態。対象objectの状態がAPI要求に合わない。 */
#define E_OBJ     (-41)
/** @brief timeoutまたはpoll失敗。即時取得不能もこの章ではtimeout系として扱う。 */
#define E_TMOUT   (-50)
/** @brief 待ち解除。将来拡張用に公開するが14.4では通常返さない。 */
#define E_RLWAI   (-49)
/** @brief queueまたはcount上限超過。 */
#define E_QOVR    (-43)

/*
 * 旧章で導入したAPI別戻り値名は、14.4以降は共通エラーコードの別名として残す。
 * これにより既存呼び出し側を壊さず、公開APIの意味を `E_*` へ統一する。
 */
#define YIELD_TSK_OK E_OK
#define YIELD_TSK_ERR_INVALID_CURRENT_STATE E_CTX

#define WAI_SEM_OK E_OK
#define WAI_SEM_ERR_INVALID_CURRENT_STATE E_CTX
#define WAI_SEM_ERR_SEMAPHORE E_ID
#define WAI_SEM_ERR_DISPATCH E_OBJ

#define POL_SEM_OK E_OK
#define POL_SEM_ERR_WOULD_BLOCK E_TMOUT
#define POL_SEM_ERR_INVALID_CURRENT_STATE E_CTX
#define POL_SEM_ERR_SEMAPHORE E_ID

#define SIG_SEM_OK E_OK
#define SIG_SEM_ERR_SEMAPHORE E_ID
#define SIG_SEM_ERR_TASK E_OBJ
#define SIG_SEM_ERR_OVERFLOW E_QOVR

#define DLY_TSK_OK E_OK
#define DLY_TSK_ERR_INVALID_DELAY E_PAR
#define DLY_TSK_ERR_INVALID_CURRENT_STATE E_CTX
#define DLY_TSK_ERR_DISPATCH E_OBJ

#define TWAI_SEM_OK E_OK
#define TWAI_SEM_ERR_INVALID_TIMEOUT E_PAR
#define TWAI_SEM_ERR_INVALID_CURRENT_STATE E_CTX
#define TWAI_SEM_ERR_SEMAPHORE E_ID
#define TWAI_SEM_ERR_DELAY_QUEUE E_QOVR
#define TWAI_SEM_ERR_DISPATCH E_OBJ

#define CRE_TSK_OK E_OK
#define CRE_TSK_ERR_INVAL E_PAR
#define CRE_TSK_ERR_TASK E_OBJ

#define STA_TSK_OK E_OK
#define STA_TSK_ERR_INVAL E_ID
#define STA_TSK_ERR_NOT_FOUND E_ID
#define STA_TSK_ERR_BAD_STATE E_OBJ

#define SLP_TSK_OK E_OK
#define SLP_TSK_ERR_INVALID_CURRENT_STATE E_CTX
#define SLP_TSK_ERR_DISPATCH E_OBJ

#define WUP_TSK_OK E_OK
#define WUP_TSK_ERR_INVAL E_ID
#define WUP_TSK_ERR_NOT_FOUND E_ID
#define WUP_TSK_ERR_BAD_STATE E_OBJ

/**
 * @struct itron_task_create_param_t
 * @brief `cre_tsk()` へ渡す学習用task生成属性。
 * @details
 * 第14章14.1ではtask生成と起動を分離するため、生成時はTCBをDORMANTに
 * 登録する。14.4では不正なNULL pointer、entryなし、stackなし、size 0を
 * `E_PAR` として返す。
 */
typedef struct {
    task_entry_t entry;       /**< task入口関数。NULLは `E_PAR`。 */
    int priority;             /**< scheduler比較用優先度。数値が小さいほど高優先度。 */
    void *stack_base;         /**< 呼び出し側が用意したstack基底。NULLは `E_PAR`。 */
    unsigned long stack_size; /**< stack領域サイズ。0は `E_PAR`。 */
    const char *name;         /**< task名。NULLは `E_PAR`。 */
} itron_task_create_param_t;

/**
 * @brief 共通エラーコード名をログ用文字列へ変換する。
 * @param ercd 変換対象の `ER` 値。
 * @return `E_OK`、`E_ID` などの静的文字列。不明値は `E_UNKNOWN`。
 */
const char *itron_error_name(ER ercd);

/**
 * @brief μITRON風の自発的yield要求入口。
 * @return `E_OK` はRUNNING current taskのREADY化と次READY候補への接続成功。
 *         `E_CTX` はcurrent taskなし、またはcurrentがRUNNINGではない場合。
 */
ER yield_tsk(void);

/**
 * @brief μITRON風のtask生成API。
 * @param tskid 生成対象task ID。1未満は `E_ID`。
 * @param pk_ctsk task生成属性。NULLや必須field欠落は `E_PAR`。
 * @return 成功時 `E_OK`。不正IDは `E_ID`、不正引数は `E_PAR`、
 *         登録済みIDなど生成不可状態は `E_OBJ`。
 */
ER cre_tsk(ID tskid, const itron_task_create_param_t *pk_ctsk);

/**
 * @brief μITRON風のtask起動API。
 * @param tskid 起動対象task ID。
 * @return 成功時 `E_OK`。不正または未登録IDは `E_ID`、
 *         DORMANTではない対象taskは `E_OBJ`。
 */
ER sta_tsk(ID tskid);

/**
 * @brief μITRON風のsleep待ちAPI。
 * @return 成功時 `E_OK`。current taskなし、またはcurrentがRUNNINGではない場合は
 *         `E_CTX`。状態遷移またはdispatcher境界の失敗は `E_OBJ`。
 */
ER slp_tsk(void);

/**
 * @brief μITRON風のsleep待ちtask起床API。
 * @param tskid 起床対象task ID。
 * @return 成功時 `E_OK`。不正または未登録IDは `E_ID`、
 *         sleep待ちではない対象taskは `E_OBJ`。
 */
ER wup_tsk(ID tskid);

/**
 * @brief μITRON風のsemaphore待ちAPI。
 * @param sem_id 対象semaphore ID。
 * @return 即時取得またはWAITING遷移成功時 `E_OK`。不正semaphore IDは `E_ID`、
 *         不正文脈は `E_CTX`、状態遷移またはdispatcher境界の失敗は `E_OBJ`。
 */
ER wai_sem(ID sem_id);

/**
 * @brief μITRON風のsemaphore返却API。
 * @param sem_id 対象semaphore ID。
 * @return 成功時 `E_OK`。不正semaphore IDは `E_ID`、
 *         wakeup対象task不整合は `E_OBJ`、count上限超過は `E_QOVR`。
 */
ER sig_sem(ID sem_id);

/**
 * @brief μITRON風のsemaphore非ブロッキング取得API。
 * @param sem_id 対象semaphore ID。
 * @return 即時取得成功時 `E_OK`。不正semaphore IDは `E_ID`、
 *         不正文脈は `E_CTX`、即時取得できない場合はWAITINGへ遷移せず `E_TMOUT`。
 */
ER pol_sem(ID sem_id);

/**
 * @brief μITRON風のdelay待ちAPI。
 * @param delay_ticks delay queueへ登録するtick数。0は `E_PAR`。
 * @return 成功時 `E_OK`。不正引数は `E_PAR`、不正文脈は `E_CTX`、
 *         queueまたはdispatcher境界の失敗は `E_OBJ`。
 */
ER dly_tsk(uint32_t delay_ticks);

/**
 * @brief μITRON風のtimeout付きsemaphore待ちAPI。
 * @param sem_id 対象semaphore ID。
 * @param timeout_ticks timeout観測用tick数。0は `E_PAR`。
 * @return 即時取得またはtimeout待ち登録成功時 `E_OK`。不正semaphore IDは `E_ID`、
 *         不正timeoutは `E_PAR`、不正文脈は `E_CTX`、queue上限超過は `E_QOVR`。
 * @note timeout到達そのものはdelay queue tick処理で `E_TMOUT` としてログ化する。
 */
ER twai_sem(ID sem_id, uint32_t timeout_ticks);

#endif
