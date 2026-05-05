/*
 * SPDX-License-Identifier: MIT
 */

/**
 * @file task.h
 * @brief 初期タスク管理API定義（第7回、第8回）
 *
 * @details
 * kernel層でタスクを「実行対象」ではなく「管理対象」として登録するための
 * TCB、状態、エラーコード、公開APIを定義する。
 * 第7回ではタスクを実行せず、entry関数呼び出し、コンテキスト作成、
 * スタック初期化は行わない。第8回ではREADY状態のタスクを選ぶ
 * 簡易スケジューラから参照できるよう、状態と優先度を公開契約として整理する。
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
 * μITRON風RTOSの学習用として、将来のタスクライフサイクルを表す状態を定義する。
 * 第8回でスケジューラが選択対象にするのはREADYのみであり、RUNNINGへの遷移、
 * WAITINGへの待ち入り、コンテキストスイッチはまだ行わない。
 */
typedef enum {
    TASK_STATE_UNUSED = 0, /**< 未使用スロット。空き判定はこの値だけで行う内部管理状態。 */
    TASK_STATE_DORMANT,    /**< 生成済みだが未開始の状態。第8回では将来拡張用に定義のみ行う。 */
    TASK_STATE_READY,      /**< 実行可能な状態。第8回のscheduler_select_next()が唯一の選択対象にする。 */
    TASK_STATE_RUNNING,    /**< 実行中状態。第8回ではコンテキストスイッチを行わないため設定しない。 */
    TASK_STATE_WAITING,    /**< 待ち状態。第8回では待ちキューや待ち解除を実装せず、将来拡張用に残す。 */
} task_state_t;

/**
 * @struct tcb_t
 * @brief Task Control Block。
 *
 * @details
 * タスクを実行するためではなく、kernel内部で登録済みタスクを管理・表示し、
 * 第8回の簡易スケジューラがREADYタスクを選択するための最小情報を保持する。
 * スタック情報とentryは将来拡張に備えて保持するだけで、第8回では呼び出し、
 * スタック切り替え、コンテキスト作成を行わない。
 */
typedef struct {
    int id;                    /**< 登録後に割り当てられるタスクID。0は未使用または無効ID。 */
    const char *name;          /**< dump時に識別しやすくするためのタスク名。 */
    task_entry_t entry;        /**< 将来の実行開始で使う入口関数。第8回では呼び出さない。 */
    int priority;              /**< scheduler選択用優先度。数値が小さいほど高優先度として扱う。 */
    task_state_t state;        /**< 空き判定とscheduler候補判定の根拠。READYだけが第8回の選択対象。 */
    void *stack_base;          /**< 将来のスタック管理に渡す基底アドレス。第8回では保持のみ。 */
    unsigned long stack_size;  /**< stack_baseが指す領域のサイズ。第8回では保持と表示のみ。 */
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
 * @note タスク実行、コンテキスト作成、スタック初期化、スケジューラ選択は行わない。
 */
void task_init(void);

/**
 * @brief タスク情報を静的テーブルへ登録する。
 *
 * @details
 * 引数検証、空きスロット探索、ID採番、TCB設定、登録ログ出力を行う。
 * 登録直後の状態はREADYであり、第8回の簡易スケジューラの選択候補になる。
 * ただし、この関数はentry関数を呼ばず、実行もコンテキスト作成も行わない。
 *
 * @param name タスク名。NULLは不正。
 * @param entry タスク入口関数。NULLは不正だが、このAPIでは呼び出さない。
 * @param priority 優先度。数値が小さいほど高優先度として第8回schedulerが扱う。
 * @param stack_base スタック領域の基底アドレス。NULLは不正だが初期化しない。
 * @param stack_size スタック領域のサイズ。0は不正。
 * @return 成功時は1以上のタスクID。失敗時はTASK_ERR_*。
 * @note task_start、コンテキストスイッチ、スタック初期化は未実装である。
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

/**
 * @brief schedulerが走査できるタスクスロット数を返す。
 *
 * @details
 * 第8回ではschedulerがtask_tableを直接extern参照しないよう、この読み取りAPIを使う。
 * 戻り値は固定長テーブルの上限であるMAX_TASKSであり、動的メモリや可変長配列は使わない。
 *
 * @param なし。
 * @return 走査可能なタスクスロット数。
 * @note このAPIはtask_tableの内容を変更しない。
 */
int task_get_count(void);

/**
 * @brief 指定indexのTCBを読み取り専用で参照する。
 *
 * @details
 * schedulerはこのAPIを通じてTCBを確認し、`state == TASK_STATE_READY` のタスクだけを
 * 選択候補にする。task_tableの所有権はtask.cに残し、呼び出し側は返されたTCBを変更しない。
 *
 * @param index 参照するtask_table上のindex。
 * @return 範囲内ならTCBへの読み取り専用ポインタ。範囲外ならNULL。
 * @note task_tableをextern公開しないための境界APIであり、タスク実行は行わない。
 */
const tcb_t *task_get_by_index(int index);

#endif
