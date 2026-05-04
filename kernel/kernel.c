/*
 * SPDX-License-Identifier: MIT
 */

/**
 * @file kernel.c
 * @brief 最小kernel起動と初期タスク登録確認（第4回、第6回、第7回）
 *
 * @details
 * 第4回の最小kernel起動点として `kernel_main()` を提供し、
 * 第6回のHAL console APIだけを通じてログ出力を行う。
 * 第7回では、タスクを実行せずに `task_init()`、`task_register()`、
 * `task_dump()` を呼び出し、QEMUシリアルログで登録状態を確認する。
 *
 * kernel層はarch依存コードを直接呼ばず、依存方向は
 * kernel → HAL → arch(x86_64) → serial → COM1 である。
 */

#include "hal/console.h"
#include "task.h"

#define TASK_STACK_SIZE 1024

static unsigned char task_a_stack[TASK_STACK_SIZE];
static unsigned char task_b_stack[TASK_STACK_SIZE];

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
 * @brief kernelのメインエントリポイント。
 *
 * @details
 * HAL consoleを初期化し、起動ログと初期タスク管理の確認ログを出力する。
 * タスク登録後は `task_dump()` で一覧を表示し、従来どおりHLTループに入る。
 *
 * @param なし。
 * @return なし。
 * @note task_start、scheduler_start、context_switch、割り込み、タイマは追加しない。
 */
void kernel_main(void)
{
    int task_a_id;
    int task_b_id;

    hal_console_init();
    hal_console_write("itron-rtos booting...\n");
    hal_console_write("kernel_main reached\n");

    /* タスク管理台帳だけを初期化し、スケジューラや実行コンテキストは作らない。 */
    task_init();

    /*
     * stack領域はTCBへ保持するだけで初期化しない。
     * entry関数も登録対象として渡すだけで、この時点では呼び出されない。
     */
    task_a_id = task_register(
        "task_a",
        task_a,
        1,
        task_a_stack,
        sizeof(task_a_stack)
    );
    kernel_log_task_register_result("task_a", task_a_id);

    /* 2件目の登録により、IDが配列位置ではなく採番状態から進むことを確認する。 */
    task_b_id = task_register(
        "task_b",
        task_b,
        2,
        task_b_stack,
        sizeof(task_b_stack)
    );
    kernel_log_task_register_result("task_b", task_b_id);

    /* 登録済みTCBだけを表示し、UNUSEDスロットは表示されないことを確認する。 */
    task_dump();

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
