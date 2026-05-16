/*
 * SPDX-License-Identifier: MIT
 */

/**
 * @file semaphore.h
 * @brief 学習用μITRON風セマフォ管理API（第6章6.1）。
 *
 * @details
 * このAPIはセマフォ同期機構の土台を観測するための最小契約である。
 * 静的semaphore tableにid、name、count、max_countを保持し、起動時の
 * QEMU serial logで初期化、取得、待ち入り、返却、dumpを確認できるようにする。
 *
 * この段階ではtimer、timeout付き待ち、preemption、interrupt、wait queue、
 * priority inheritance、mutex、event flag、μITRON完全互換APIは導入しない。
 * WAITING状態は観測用の最小モデルであり、将来のwait queueやtimer連携で
 * 置き換えられる境界として扱う。
 */

#ifndef ITRON_RTOS_SEMAPHORE_H
#define ITRON_RTOS_SEMAPHORE_H

#define MAX_SEMAPHORES 16

#define SEM_OK            0
#define SEM_ERR_FULL      (-1)
#define SEM_ERR_INVAL     (-2)
#define SEM_ERR_NOT_FOUND (-3)
#define SEM_ERR_OVERFLOW  (-4)
#define SEM_ERR_TASK      (-5)

/**
 * @struct semaphore_t
 * @brief 静的semaphore tableに保持するセマフォ管理情報。
 *
 * @details
 * 第6章6.1ではcount変化を観測するための最小情報だけを持つ。
 * wait queue、owner、timeout情報、優先度制御情報は保持しない。
 */
typedef struct {
    int id;          /**< セマフォID。0は未使用または無効値として扱う。 */
    const char *name;/**< ログとdumpで識別するためのセマフォ名。 */
    int count;       /**< 現在の資源数。0以上max_count以下を不変条件にする。 */
    int max_count;   /**< countの上限。sig_sem相当操作で超過させない。 */
} semaphore_t;

/**
 * @brief 静的semaphore tableを初期化する。
 *
 * @details
 * 全slotを未使用状態へ戻し、ID採番を初期値へ戻す。
 * timer、preemption、interrupt、timeoutとは接続しない。
 *
 * @return 常にSEM_OK。
 */
int sem_init(void);

/**
 * @brief セマフォを静的tableへ作成する。
 *
 * @param name セマフォ名。NULLは不正。
 * @param initial_count 初期count。0以上max_count以下でなければ不正。
 * @param max_count count上限。1以上でなければ不正。
 * @return 成功時は1以上のセマフォID。失敗時はSEM_ERR_*。
 */
int sem_create(const char *name, int initial_count, int max_count);

/**
 * @brief wai_sem相当のセマフォ取得を行う。
 *
 * @details
 * countが残っている場合はcountを1減らす。countが0の場合はtask moduleへ委譲し、
 * 対象taskをセマフォ待ちのWAITING状態にする。ここではwait queue、timeout、
 * preemption、interrupt連携は行わない。
 *
 * @param sem_id 対象セマフォID。
 * @param task_id 待ち入り対象task ID。
 * @return 成功またはWAITING化成功時はSEM_OK。失敗時はSEM_ERR_*。
 */
int wai_sem(int sem_id, int task_id);

/**
 * @brief sig_sem相当のセマフォ返却を行う。
 *
 * @details
 * 対象セマフォを待つtaskがあれば、最小実装として1 taskだけREADYへ戻す。
 * WAITING taskがなければcountを1増やす。FIFO順や優先度順は保証せず、
 * 将来のwait queue導入で置き換える。
 *
 * @param sem_id 対象セマフォID。
 * @return 成功時はSEM_OK。失敗時はSEM_ERR_*。
 */
int sig_sem(int sem_id);

/**
 * @brief 登録済みセマフォの一覧をHAL consoleへ出力する。
 *
 * @details
 * dumpは観測専用であり、セマフォ状態を変更しない。
 */
void sem_dump(void);

#endif
