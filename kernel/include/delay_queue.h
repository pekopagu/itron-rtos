/*
 * SPDX-License-Identifier: MIT
 */

/**
 * @file delay_queue.h
 * @brief 13.2 sleep/delay queueの観測用管理API。
 *
 * @details
 * このヘッダは、`dly_tsk()` によってdelay WAITINGへ入ったtaskを
 * semaphore wait queueとは独立して保持する固定長queueを公開する。
 * 13.2ではremaining tickを観測用に保持するだけで、tickごとのdecrement、
 * tick到達時READY復帰、dequeueによるwakeupはまだ実装しない。
 */

#ifndef ITRON_RTOS_DELAY_QUEUE_H
#define ITRON_RTOS_DELAY_QUEUE_H

#include <stdint.h>

/**
 * @brief delay queue操作が成功したことを示す戻り値。
 */
#define DELAY_QUEUE_OK 0

/**
 * @brief 不正なtask IDまたはdelay tick値を示す戻り値。
 */
#define DELAY_QUEUE_ERR_INVAL (-1)

/**
 * @brief 固定長delay queueが満杯で登録できないことを示す戻り値。
 */
#define DELAY_QUEUE_ERR_FULL (-2)

/**
 * @brief 同じtaskがすでにdelay queueへ登録済みであることを示す戻り値。
 */
#define DELAY_QUEUE_ERR_DUPLICATE (-3)

/**
 * @brief 対象taskがdelay WAITINGではないため登録できないことを示す戻り値。
 */
#define DELAY_QUEUE_ERR_TASK_STATE (-4)

/**
 * @brief delay queueを空状態へ初期化する。
 *
 * @details
 * 起動時に呼び出し、過去の観測entryが残らないように固定長entry配列を消去する。
 * semaphore tableやtask tableは変更しない。13.2ではtimer IRQ handlerから呼ばない。
 *
 * @return `DELAY_QUEUE_OK`。
 */
int delay_queue_init(void);

/**
 * @brief WAITING化前にdelay queueへ登録可能かを確認する。
 *
 * @details
 * `dly_tsk()` がRUNNING current taskをWAITINGへ変更する前に呼び、
 * queue満杯や二重enqueueを検出する。不整合なWAITING taskを残さないための
 * 事前確認APIであり、queueやtask状態は変更しない。
 *
 * @param task_id 登録予定のtask ID。
 * @return 登録可能なら `DELAY_QUEUE_OK`、不可なら `DELAY_QUEUE_ERR_*`。
 */
int delay_queue_can_enqueue(int task_id);

/**
 * @brief delay WAITING化済みtaskをdelay queueへ登録する。
 *
 * @details
 * 対象taskは `TASK_STATE_WAITING` かつ `TASK_WAIT_REASON_DELAY` でなければならない。
 * `wait_sem_id` はdelay queue管理に使わず、queue entryはtask idとremaining tickだけを
 * 保持する。13.2では登録後のdecrementやREADY復帰は行わない。
 *
 * @param task_id delay WAITING化済みtask ID。
 * @param delay_ticks 観測用に保持するremaining tick数。0は不正。
 * @return 登録成功なら `DELAY_QUEUE_OK`、失敗なら `DELAY_QUEUE_ERR_*`。
 */
int delay_queue_enqueue(int task_id, uint32_t delay_ticks);

/**
 * @brief delay queueの観測dumpをHAL consoleへ出力する。
 *
 * @details
 * queue countと各entryのtask id/name/remaining/reason/stateを出力する。
 * dumpは観測専用で、queue entry、task状態、scheduler候補、dispatcher currentを変更しない。
 */
void delay_queue_dump(void);

#endif
