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

#include "task.h"

#define MAX_SEMAPHORES 16

#define SEM_OK            0
#define SEM_ERR_FULL      (-1)
#define SEM_ERR_INVAL     (-2)
#define SEM_ERR_NOT_FOUND (-3)
#define SEM_ERR_OVERFLOW  (-4)
#define SEM_ERR_TASK      (-5)
#define SEM_WAIT_QUEUE_EMPTY 2
#define SEM_WAIT_REQUIRED 1

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
    int wait_queue[MAX_TASKS]; /**< 12.3のFIFO wait queue。WAITING taskのidだけを固定長で保持する。 */
    int wait_head;   /**< 次にdequeueする位置。priority順制御はまだ行わない。 */
    int wait_tail;   /**< 次にenqueueする位置。timeout queueとは連動しない。 */
    int wait_count;  /**< queue内の待ちtask数。wakeup後preemption判定には使わない。 */
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
 * @brief 登録済みsemaphoreをIDで読み取り専用参照する。
 *
 * @details
 * 第12章12.1では `wai_sem()` のtask文脈制御をAPI層へ移すため、
 * semaphore moduleはtable探索とcount保持だけを担当する。この関数は
 * ログ表示と存在確認のための参照を返すだけで、task状態、scheduler、
 * dispatcher、wait queue、timeout、preemptionには触れない。
 *
 * @param sem_id 参照するsemaphore ID。
 * @return 見つかったsemaphore。存在しない場合はNULL。
 */
const semaphore_t *sem_get_by_id(int sem_id);

/**
 * @brief semaphore countが残っていれば1つ取得する。
 *
 * @details
 * countが1以上ならcountを1減らして `SEM_OK` を返す。countが0なら
 * `SEM_WAIT_REQUIRED` を返し、task WAITING化は呼び出し側の `wai_sem()`
 * task文脈APIに委ねる。ここでは `sig_sem()`、wait queue、timeout、
 * wakeup後preemption、time slice、round-robinは実装しない。
 *
 * @param sem_id 対象semaphore ID。
 * @param count_before 更新前countの格納先。NULL可。
 * @param count_after 更新後countの格納先。NULL可。
 * @return 取得成功はSEM_OK、待ち入りが必要ならSEM_WAIT_REQUIRED、失敗時はSEM_ERR_*。
 */
int sem_take_if_available(int sem_id, int *count_before, int *count_after);

/**
 * @brief 待ちtaskがいない `sig_sem()` 経路用にsemaphore countを1つ増やす。
 *
 * @details
 * semaphore moduleがcountの所有権を持つため、API層はcountを直接変更しない。
 * max_countを超える場合は失敗し、countは変更しない。
 *
 * @param sem_id 対象semaphore ID。
 * @param count_before 更新前countの格納先。NULL可。
 * @param count_after 更新後countの格納先。NULL可。
 * @return 成功時はSEM_OK。失敗時はSEM_ERR_*。
 */
int sem_increment_count(int sem_id, int *count_before, int *count_after);

/**
 * @brief WAITING化前にsemaphore wait queueへ登録可能かを確認する。
 *
 * @details
 * `twai_sem()` がRUNNING current taskをWAITINGへ変更する前に呼び、対象semaphoreの存在、
 * task ID、固定長wait queueの空きだけを確認する。task状態やqueue内容は変更しない。
 *
 * @param sem_id 対象semaphore ID。
 * @param task_id 登録予定のtask ID。
 * @return 登録可能ならSEM_OK。失敗時はSEM_ERR_*。
 */
int sem_can_enqueue_waiter(int sem_id, int task_id);

/**
 * @brief 12.3のFIFO wait queueへWAITING task idを登録する。
 *
 * @details
 * `wai_sem()` がRUNNING taskをWAITINGへ落とした後に呼ぶqueue操作である。
 * この関数はtask状態を変更せず、対象semaphoreが所有する固定長FIFO queueだけを更新する。
 * priority順、timeout到達処理、time slice、round-robinはここでは扱わない。
 * 第13章13.3では `twai_sem()` のtimeout付きsemaphore waiterも `sig_sem()` の
 * wakeup対象として同じFIFO queueへ登録できるが、timeout時のqueue削除はまだ行わない。
 *
 * @param sem_id 対象semaphore ID。
 * @param task_id WAITING化済みtask ID。
 * @return 成功時はSEM_OK。失敗時はSEM_ERR_*。
 */
int sem_enqueue_waiter(int sem_id, int task_id);

/**
 * @brief 12.3のFIFO wait queueから待ちtask idを1件取り出す。
 *
 * @details
 * `sig_sem()` がtask table全体を探索せず、対象semaphoreの待ち行列だけから
 * wakeup対象を決めるためのqueue操作である。空の場合はSEM_WAIT_QUEUE_EMPTYを返す。
 *
 * @param sem_id 対象semaphore ID。
 * @param task_id 取り出したtask IDの格納先。NULLは不正。
 * @return 成功時はSEM_OK。空の場合はSEM_WAIT_QUEUE_EMPTY。失敗時はSEM_ERR_*。
 */
int sem_dequeue_waiter(int sem_id, int *task_id);

/**
 * @brief timeout到達taskをsemaphore wait queueからtask id指定で削除する。
 *
 * @details
 * 13.4のtimeout付きsemaphore待ち用APIである。`sig_sem()` のFIFO dequeueとは別に、
 * delay queueでtimeout到達を検出したtaskだけを対象semaphoreのwait queueから取り除く。
 * queueの残り順序は維持する。task状態は変更しない。
 *
 * @param sem_id 対象semaphore ID。
 * @param task_id 削除するWAITING task ID。
 * @return 成功時はSEM_OK。対象なしはSEM_WAIT_QUEUE_EMPTY。失敗時はSEM_ERR_*。
 */
int sem_remove_waiter(int sem_id, int task_id);

/*
 * 12.2のsig_sem相当のセマフォ返却を行う。
 *
 * 対象セマフォを待つtaskがあれば、最小実装として1 taskだけREADYへ戻し、
 * `wait_sem_id` を未待ち状態へ戻す。この場合countは増やさない。
 * WAITING taskがなければcountを1増やす。FIFO順や優先度順、wakeup後preemption、
 * timeout、同一優先度time slice、round-robinは保証せず、将来のwait queue導入で置き換える。
 */
/*
 * 12.4のsig_sem相当のセマフォ返却とwakeup後preemption判定を行う。
 *
 * 対象semaphoreのFIFO wait queueにWAITING taskがあれば、1 taskだけdequeueしてREADYへ戻す。
 * READY復帰時には `wait_sem_id` を必ずclearし、このwakeup経路ではsemaphore countを増やさない。
 * READYへ戻した後、task文脈のcurrent RUNNING taskとwoken READY taskのpriorityを比較し、
 * woken taskのpriority値がcurrentより小さい場合だけ既存の `dispatcher_switch_to()` 境界へ進む。
 *
 * 同一priorityはまだtime slice対象にしない。低優先度wakeupと同じくno-switchで完了する。
 * wait queueが空の場合だけcountを1増やす。priority順wait queue、timeout付き `twai_sem`、
 * sleep/delay queue、round-robin、timer IRQ handlerからの呼び出し、dispatch pending経路との統合、
 * 完全な割り込み復帰フレーム切替はここでは扱わない。
 */
/* sig_sem() は14.3以降、μITRON風API層の itron_api.c が所有する。 */

/**
 * @brief 登録済みセマフォの一覧をHAL consoleへ出力する。
 *
 * @details
 * dumpは観測専用であり、セマフォ状態を変更しない。
 */
void sem_dump(void);

#endif
