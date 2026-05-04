/*
 * SPDX-License-Identifier: MIT
 */

/**
 * @file task.h
 * @brief 初期タスク管理API定義（第7回）
 *
 * @details
 * kernel層でタスクを「実行対象」ではなく「管理対象」として登録するための
 * TCB、状態、エラーコード、公開APIを定義する。
 * 第7回ではタスクを実行せず、entry関数呼び出し、コンテキスト作成、
 * スタック初期化、スケジューラ導入は行わない。
 *
 * ログ出力は第6回のHAL境界を守り、kernel → HAL → arch(x86_64) → serial → COM1
 * の層構造を維持する。
 */

#ifndef ITRON_RTOS_TASK_H
#define ITRON_RTOS_TASK_H

#define MAX_TASKS 256

#define TASK_ERR_FULL        (-1)
#define TASK_ERR_INVAL       (-2)
#define TASK_ERR_ID_OVERFLOW (-3)

typedef void (*task_entry_t)(void);

/**
 * @enum task_state_t
 * @brief タスク管理テーブル上のタスク状態。
 *
 * @details
 * 第7回では登録状態を表すために使う。READYは登録済みであることを示すが、
 * スケジューラやコンテキストスイッチは未実装である。
 */
typedef enum {
    TASK_STATE_UNUSED = 0, /**< 未使用スロット。空き判定はこの値だけで行う。 */
    TASK_STATE_DORMANT,    /**< 将来の生成済み未開始状態。第7回では遷移しない。 */
    TASK_STATE_READY,      /**< 登録済み状態。第7回では実行せずdump対象にする。 */
    TASK_STATE_RUNNING,    /**< 将来の実行中状態。第7回では設定しない。 */
} task_state_t;

/**
 * @struct tcb_t
 * @brief Task Control Block。
 *
 * @details
 * タスクを実行するためではなく、kernel内部で登録済みタスクを管理・表示するための
 * 最小情報を保持する。スタック情報とentryは将来拡張に備えて保持するだけで、
 * 第7回では呼び出しや初期化を行わない。
 */
typedef struct {
    int id;                    /**< 登録後に割り当てられるタスクID。0は未使用または無効ID。 */
    const char *name;          /**< dump時に識別しやすくするためのタスク名。 */
    task_entry_t entry;        /**< 将来の実行開始で使う入口関数。第7回では呼び出さない。 */
    int priority;              /**< 将来のスケジューラ用優先度。今回は保存と表示のみ。 */
    task_state_t state;        /**< 空き判定と登録状態の根拠。空き判定はUNUSEDだけを使う。 */
    void *stack_base;          /**< 将来のスタック管理に渡す基底アドレス。今回は保持のみ。 */
    unsigned long stack_size;  /**< stack_baseが指す領域のサイズ。今回は保持と表示のみ。 */
} tcb_t;

/**
 * @brief 静的タスクテーブルとID採番状態を初期化する。
 *
 * @details
 * 全スロットをUNUSEDにし、次のタスクIDを1へ戻す。
 * 再起動直後と同じ管理状態にするためのAPIである。
 *
 * @param なし。
 * @return なし。
 * @note タスク実行、コンテキスト作成、スタック初期化は行わない。
 */
void task_init(void);

/**
 * @brief タスク情報を静的テーブルへ登録する。
 *
 * @details
 * 引数検証、空きスロット探索、ID採番、TCB設定、登録ログ出力を行う。
 * 登録直後の状態はREADYだが、第7回ではentry関数を呼ばず、実行もしない。
 *
 * @param name タスク名。NULLは不正。
 * @param entry タスク入口関数。NULLは不正だが、このAPIでは呼び出さない。
 * @param priority 優先度。今回は保存と表示のみ行う。
 * @param stack_base スタック領域の基底アドレス。NULLは不正だが初期化しない。
 * @param stack_size スタック領域のサイズ。0は不正。
 * @return 成功時は1以上のタスクID。失敗時はTASK_ERR_*。
 * @note スケジューラ、コンテキストスイッチ、スタック初期化は未実装である。
 */
int task_register(
    const char *name,
    task_entry_t entry,
    int priority,
    void *stack_base,
    unsigned long stack_size
);

/**
 * @brief 登録済みタスクの一覧をHAL consoleへ出力する。
 *
 * @details
 * UNUSEDスロットを除外し、TCBの主要フィールドをdumpする。
 * HAL境界を守るため、出力はHAL console API経由で行われる。
 *
 * @param なし。
 * @return なし。
 * @note dumpは観測用であり、タスク状態を変更しない。
 */
void task_dump(void);

#endif
