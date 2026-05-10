/*
 * SPDX-License-Identifier: MIT
 */

/**
 * @file scheduler.c
 * @brief 簡易優先度スケジューラ実装（第3章3.2）
 *
 * @details
 * READY状態のタスクから、priority値が最も小さいタスクを1つ選択する。
 * 第3章3.2では選択のみを扱い、task entry呼び出し、RUNNING遷移、スタック切り替え、
 * コンテキストスイッチ、割り込み、タイマ、プリエンプションは実装しない。
 *
 * このファイルはtask読み取りAPIだけに依存し、HAL consoleやarch固有serialを呼ばない。
 */

#include <stddef.h>

#include "scheduler.h"
#include "task.h"

/**
 * @brief 簡易スケジューラを初期化する。
 *
 * @details
 * 第3章3.2ではREADYキューや実行中タスクを持たないため、初期化すべき内部状態はない。
 * 将来READYキューやラウンドロビン状態を持つ場合の拡張点としてAPI境界だけを用意する。
 *
 * @param なし。
 * @return なし。
 * @note 動的メモリ確保、HAL出力、タスク実行は行わない。
 */
void scheduler_init(void)
{
    /*
     * 第3章3.2のschedulerはREADY taskを走査して選ぶだけで、内部queueや
     * 現在実行中taskを持たない。将来ready queueを導入する場合の入口として
     * APIだけを先に固定しておく。
     */
}

/**
 * @brief READY状態の最高優先度タスクを選択する。
 *
 * @details
 * 選択アルゴリズムは次の通り。
 * 1. `best` をNULLにする。
 * 2. `task_get_count()` で得た範囲を0から順に走査する。
 * 3. `task_get_by_index()` がNULLを返したスロットは無視する。
 * 4. `TASK_STATE_READY` でないタスクは候補から除外する。
 * 5. まだ候補がない場合、最初のREADYタスクを `best` にする。
 * 6. 現在の候補よりpriority値が小さいREADYタスクだけで `best` を更新する。
 * 7. 同一priorityでは更新しないため、先に見つかったタスクが選ばれる。
 *
 * μITRON風にpriorityの数値が小さいほど高優先度として扱うが、第3章3.2では選択のみで
 * 実行しない。TCBやtask_tableを書き換えず、entry呼び出し、RUNNING遷移、スタック切り替え、
 * コンテキストスイッチ、HAL出力、arch依存処理を行わない。
 *
 * @param なし。
 * @return 選択されたTCBへの読み取り専用ポインタ。READYタスクがなければNULL。
 */
const tcb_t *scheduler_select_next(void)
{
    const tcb_t *best = NULL;
    int count;
    int index;

    count = task_get_count();
    for (index = 0; index < count; index++) {
        const tcb_t *task = task_get_by_index(index);

        if (task == NULL) {
            /* 範囲外や未取得slotは選択候補にしない。走査は継続する。 */
            continue;
        }

        if (task->state != TASK_STATE_READY) {
            /* schedulerの責務はREADY候補の選択だけなので、他状態は変更せず除外する。 */
            continue;
        }

        if (best == NULL || task->priority < best->priority) {
            /*
             * 数値が小さいpriorityを高優先度として扱う。
             * 同一priorityでは先に見つかったtaskを維持し、登録順の観測性を保つ。
             */
            best = task;
        }
    }

    /* READY候補がなければNULLを返し、呼び出し側に停止判断を委ねる。 */
    return best;
}
