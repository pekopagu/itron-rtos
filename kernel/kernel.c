/*
 * SPDX-License-Identifier: MIT
 */

/**
 * @file kernel.c
 * @brief 最小kernel起動と初期タスク登録・簡易scheduler確認（第4回、第6回、第7回、第8回）
 *
 * @details
 * 第4回の最小kernel起動点として `kernel_main()` を提供し、
 * 第6回のHAL console APIだけを通じてログ出力を行う。
 * 第7回では、タスクを実行せずに `task_init()`、`task_register()`、
 * `task_dump()` を呼び出し、QEMUシリアルログで登録状態を確認する。
 * 第8回では `scheduler_select_next()` でREADYタスクを1つ選択し、
 * 選択結果だけをHAL console経由で表示する。entry関数は呼び出さない。
 *
 * kernel層はarch依存コードを直接呼ばず、依存方向は
 * kernel → HAL → arch(x86_64) → serial → COM1 である。
 */

#include "hal/console.h"
#include "scheduler.h"
#include "task.h"

#include <stddef.h>

#define TASK_STACK_SIZE 1024

static unsigned char task_a_stack[TASK_STACK_SIZE];
static unsigned char task_b_stack[TASK_STACK_SIZE];
static unsigned char task_c_stack[TASK_STACK_SIZE];

/**
 * @brief 符号なし整数をHAL consoleへ10進出力する。
 *
 * @details
 * `task_register()` の戻り値ログを出すためのkernel内補助関数。
 * printfを導入せず、HAL境界を保ったまま最小限の数値出力を行う。
 *
 * @param value 出力する符号なし整数。
 * @return なし。
 * @note タスク管理ロジックには関与しない表示専用処理である。
 */
static void kernel_write_uint(unsigned long value)
{
    char buffer[20];
    int index = 0;

    if (value == 0) {
        hal_console_putc('0');
        return;
    }

    while (value > 0) {
        buffer[index++] = (char)('0' + (value % 10));
        value /= 10;
    }

    while (index > 0) {
        hal_console_putc(buffer[--index]);
    }
}

/**
 * @brief 符号付き整数をHAL consoleへ10進出力する。
 *
 * @details
 * 成功時IDと負のエラーコードを同じログ経路で確認できるようにする。
 *
 * @param value 出力する符号付き整数。
 * @return なし。
 * @note HAL console APIだけを使い、serial実装を直接呼ばない。
 */
static void kernel_write_int(int value)
{
    if (value < 0) {
        hal_console_putc('-');
        kernel_write_uint((unsigned long)(-value));
        return;
    }

    kernel_write_uint((unsigned long)value);
}

/**
 * @brief `task_register()` の戻り値を起動ログへ出力する。
 *
 * @details
 * 登録成功IDまたはエラーコードをQEMUシリアルログで確認する。
 *
 * @param name 登録対象タスク名。
 * @param result `task_register()` の戻り値。
 * @return なし。
 * @note 登録結果を表示するだけで、タスク状態は変更しない。
 */
static void kernel_log_task_register_result(const char *name, int result)
{
    hal_console_write("[kernel] task_register ");
    hal_console_write(name);
    hal_console_write(" returned ");
    kernel_write_int(result);
    hal_console_write("\n");
}

/**
 * @brief タスク状態を起動ログ表示用の文字列へ変換する。
 *
 * @details
 * schedulerの選択結果をQEMUシリアルログで確認するための表示補助である。
 * 表示だけを担当し、タスク状態の変更や実行制御は行わない。
 *
 * @param state 表示するタスク状態。
 * @return 状態名を表す静的文字列。
 */
static const char *kernel_task_state_to_string(task_state_t state)
{
    switch (state) {
    case TASK_STATE_UNUSED:
        return "UNUSED";
    case TASK_STATE_DORMANT:
        return "DORMANT";
    case TASK_STATE_READY:
        return "READY";
    case TASK_STATE_RUNNING:
        return "RUNNING";
    case TASK_STATE_WAITING:
        return "WAITING";
    default:
        return "UNKNOWN";
    }
}

/**
 * @brief schedulerの選択結果をHAL consoleへ出力する。
 *
 * @details
 * scheduler自身はHALやarchへ依存しないため、起動時確認用のログはkernel側で出力する。
 * 選択されたTCBのentryは呼び出さず、id、name、priority、stateだけを表示する。
 *
 * @param label 確認ケース名。
 * @param task scheduler_select_next() の戻り値。NULLならREADYタスクなし。
 * @return なし。
 * @note 表示専用であり、RUNNING遷移、コンテキストスイッチ、スタック切り替えは行わない。
 */
static void kernel_log_scheduler_selection(const char *label, const tcb_t *task)
{
    const char *safe_label = (label != NULL) ? label : "(no-label)";
    const char *safe_name;
    const char *state_name;
    const char *safe_state;

    hal_console_write("[scheduler] ");
    hal_console_write(safe_label);

    if (task == NULL) {
        hal_console_write(" selected: none\n");
        return;
    }

    safe_name = (task->name != NULL) ? task->name : "(null)";
    state_name = kernel_task_state_to_string(task->state);
    safe_state = (state_name != NULL) ? state_name : "UNKNOWN";

    hal_console_write(" selected: id=");
    kernel_write_int(task->id);
    hal_console_write(" name=");
    hal_console_write(safe_name);
    hal_console_write(" prio=");
    kernel_write_int(task->priority);
    hal_console_write(" state=");
    hal_console_write(safe_state);
    hal_console_write("\n");
}

/**
 * @brief サンプルタスクAのentry関数。
 *
 * @details
 * 第7回ではentry関数を登録するだけで呼び出さない。
 * このログが出た場合は、タスク実行禁止の制約に反していることを示す。
 *
 * @param なし。
 * @return なし。
 * @note スケジューラ未実装のため通常は実行されない。
 */
static void task_a(void)
{
    /*
     * このログはentryが誤って呼ばれた場合だけ出る。
     * 第7回では登録確認のみなので、通常のQEMUログには出てはいけない。
     */
    hal_console_write("[task_a] executed\n");
}

/**
 * @brief サンプルタスクBのentry関数。
 *
 * @details
 * 第7回ではentry関数を登録するだけで呼び出さない。
 * 将来のタスク実行フェーズで初めて実行対象になる想定である。
 *
 * @param なし。
 * @return なし。
 * @note コンテキストスイッチ未実装のため通常は実行されない。
 */
static void task_b(void)
{
    /*
     * task_aと同じく、将来のタスク実行フェーズまで呼び出さない。
     * 登録APIがentryを保持するだけであることをログ上で確認するために残す。
     */
    hal_console_write("[task_b] executed\n");
}

/**
 * @brief サンプルタスクCのentry関数。
 *
 * @details
 * 第8回の同一priority確認用に登録するだけの関数である。
 * schedulerはTCBを選択するだけで、このentryを呼び出さない。
 *
 * @param なし。
 * @return なし。
 * @note このログが出た場合は、第8回の「選択のみ」制約に反している。
 */
static void task_c(void)
{
    hal_console_write("[task_c] executed\n");
}

/**
 * @brief kernelのメインエントリポイント。
 *
 * @details
 * HAL consoleを初期化し、起動ログと初期タスク管理の確認ログを出力する。
 * タスク登録後は `task_dump()` で一覧を表示し、簡易schedulerの選択結果を表示してから
 * 従来どおりHLTループに入る。
 *
 * @param なし。
 * @return なし。
 * @note task_start、context_switch、割り込み、タイマ、プリエンプションは追加しない。
 */
void kernel_main(void)
{
    int task_a_id;
    int task_b_id;
    int task_c_id;
    const tcb_t *selected_task;

    hal_console_init();
    hal_console_write("itron-rtos booting...\n");
    hal_console_write("kernel_main reached\n");

    /* タスク管理台帳だけを初期化し、スケジューラや実行コンテキストは作らない。 */
    task_init();
    scheduler_init();

    /*
     * READYタスクがまだ存在しない段階での選択結果を確認する。
     * NULLはエラーではなく、選択対象なしを表す通常の結果である。
     */
    selected_task = scheduler_select_next();
    kernel_log_scheduler_selection("before_register", selected_task);

    /*
     * stack領域はTCBへ保持するだけで初期化しない。
     * entry関数も登録対象として渡すだけで、この時点では呼び出されない。
     */
    task_a_id = task_register(
        "task_a",
        task_a,
        5,
        task_a_stack,
        sizeof(task_a_stack)
    );
    kernel_log_task_register_result("task_a", task_a_id);

    /*
     * task_bとtask_cは同一priorityにし、同一priorityでは先に登録されたtask_bが
     * 選ばれることを確認する。task_aはより大きいpriority値なので選ばれない。
     */
    task_b_id = task_register(
        "task_b",
        task_b,
        1,
        task_b_stack,
        sizeof(task_b_stack)
    );
    kernel_log_task_register_result("task_b", task_b_id);

    task_c_id = task_register(
        "task_c",
        task_c,
        1,
        task_c_stack,
        sizeof(task_c_stack)
    );
    kernel_log_task_register_result("task_c", task_c_id);

    /* 登録済みTCBだけを表示し、UNUSEDスロットは表示されないことを確認する。 */
    task_dump();

    /*
     * 第8回では「どのタスクが次に実行対象になるか」を選ぶだけで、
     * 選択されたentry関数を呼び出したりRUNNINGへ遷移させたりしない。
     */
    selected_task = scheduler_select_next();
    kernel_log_scheduler_selection("after_register", selected_task);

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
