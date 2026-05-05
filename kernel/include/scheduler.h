/*
 * SPDX-License-Identifier: MIT
 */

/**
 * @file scheduler.h
 * @brief 簡易優先度スケジューラAPI定義（第8回）
 *
 * @details
 * μITRON風RTOSの学習用として、READY状態のタスクから次に実行対象とみなす
 * TCBを1つ選択するためのAPIを定義する。第8回のschedulerは「選択のみ」を担当し、
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
 * @brief 簡易スケジューラを初期化する。
 *
 * @details
 * 第8回ではscheduler自身にREADYキューや現在実行中タスクを持たせないため、
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
 * この関数は第8回の「選択のみ」のAPIであり、TCBやtask_tableを書き換えない。
 * task entryを呼ばず、RUNNING状態へ変更せず、スタック切り替えやコンテキストスイッチも
 * 行わない。μITRON風の優先度規則を確認する足場として設計し、HAL/arch/serialにも依存しない。
 *
 * @param なし。
 * @return 選択されたTCBへの読み取り専用ポインタ。READYタスクがなければNULL。
 */
const tcb_t *scheduler_select_next(void);

#endif
