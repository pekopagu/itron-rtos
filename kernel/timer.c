/*
 * SPDX-License-Identifier: MIT
 */

/**
 * @file timer.c
 * @brief 学習用μITRON風RTOSのtimer foundation実装（第6章6.2）。
 *
 * @details
 * このmoduleはsystem tick counterだけを所有する。tickはRTOS内部の
 * 時間管理の最小単位として扱うが、この段階では実ハードウェアタイマ割り込み、
 * preemption、time slice、delay queue、timeout処理へ接続しない。
 *
 * `timer_tick()` は将来のtimer interrupt handlerから呼べる境界として
 * 用意する。ただし今回はboot-time verificationから明示的に呼び出し、
 * QEMU serial logで時間経過を観測するだけである。
 */

#include "hal/console.h"
#include "timer.h"

static unsigned long system_ticks;

/**
 * @brief 符号なし整数をHAL consoleへ10進出力する。
 *
 * @details
 * freestanding環境でprintfを導入せず、timer logに必要なtick値だけを
 * 出力するための表示補助である。tick状態は変更しない。
 *
 * @param value 出力する値。
 */
static void timer_write_uint(unsigned long value)
{
    char buffer[20];
    int index = 0;

    if (value == 0) {
        /* 0は剰余ループに入らないため、timer init log用に直接出力する。 */
        hal_console_putc('0');
        return;
    }

    /*
     * 下位桁からbufferへ積む。freestanding kernelなので標準printfには依存せず、
     * HAL console境界だけでtick値を観測できるようにする。
     */
    while (value > 0) {
        buffer[index++] = (char)('0' + (value % 10));
        value /= 10;
    }

    /* bufferには逆順で桁が入っているため、末尾から取り出して通常の10進表記に戻す。 */
    while (index > 0) {
        hal_console_putc(buffer[--index]);
    }
}

/**
 * @brief system tick counterを0へ戻す。
 *
 * @details
 * 起動時検証の再現性を保つため、timer moduleが所有するtick状態だけを初期値へ戻す。
 * scheduler、dispatcher、task、semaphore、arch interruptの状態には触れない。
 */
void timer_init(void)
{
    /*
     * 第6章6.2ではtickの起点を明示することが目的である。
     * hardware timerの現在時刻や周期設定はまだ存在しない。
     */
    system_ticks = 0;

    hal_console_write("[timer] init: tick=");
    timer_write_uint(system_ticks);
    hal_console_write("\n");
}

/**
 * @brief 明示呼び出し1回分のsystem tickを進める。
 *
 * @details
 * 将来はtimer interrupt handlerから呼ばれる予定の境界だが、現段階では
 * kernel boot verificationから直接呼ぶ。tick増加以外の副作用を持たせないことで、
 * 次章以降のpreemptionやtimeout設計と責務を分離する。
 */
void timer_tick(void)
{
    /*
     * 第6章6.2では明示呼び出し1回につきtickを1だけ進める。
     * ここではschedulerやdispatcherを呼ばず、割り込み由来の強制切替も行わない。
     */
    system_ticks++;

    hal_console_write("[timer] tick: ");
    timer_write_uint(system_ticks);
    hal_console_write("\n");
}

/**
 * @brief 現在のsystem tick counterを読み取る。
 *
 * @details
 * 読み取り専用の観測APIである。ログ出力も行わないことで、呼び出し側が
 * 「取得」と「表示」の責務を分けられるようにする。
 *
 * @return 現在のsystem tick値。
 */
unsigned long timer_get_ticks(void)
{
    /* 取得だけを行い、tick値やtask実行状態は変更しない。 */
    return system_ticks;
}
