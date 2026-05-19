/*
 * SPDX-License-Identifier: MIT
 */

/**
 * @file timer.h
 * @brief 学習用μITRON風RTOSのtimer foundation API（第6章6.2）。
 *
 * @details
 * このAPIはRTOS内部の時間管理の最小単位であるsystem tickを扱う。
 * 第6章6.2では、tickを初期化し、明示的な `timer_tick()` 呼び出しで
 * 時間経過を観測し、現在tickを取得するところまでを責務にする。
 *
 * `timer_tick()` は将来のtimer interrupt handlerから呼ばれる想定の
 * 境界として定義する。ただし、この段階ではPIT/APIC/HPETなどの
 * 実ハードウェアタイマ割り込みには接続しない。tick増加を契機にした
 * scheduler選択、dispatcher commit、context switch、preemption、
 * time slice、delay/timeout処理も行わない。
 */

#ifndef ITRON_RTOS_TIMER_H
#define ITRON_RTOS_TIMER_H

/**
 * @brief system tickを起動直後の状態へ初期化する。
 *
 * @details
 * tick counterを0へ戻し、QEMU serial logへ初期化状態を出力する。
 * timer interruptやschedulerには接続せず、timer moduleが所有する
 * 時間管理状態だけを初期化する。
 */
void timer_init(void);

/**
 * @brief system tickを1つ進める。
 *
 * @details
 * 将来はtimer interrupt handlerから呼ばれる想定の関数だが、
 * 第6章6.2ではkernel boot verificationから明示的に呼び出す。
 * tickを進めるだけで、preemption、time slice、context switch、
 * delay/timeout wakeupは実行しない。
 *
 * @note 第8章8.1ではtimer interrupt handlerからも呼ばれる。ただし、この関数は
 * tickを1つ進めるだけで、scheduler、dispatcher、context switch、preemption、
 * task state変更、delay/timeout wakeupは起動しない。
 */
void timer_tick(void);

/**
 * @note 第6章6.3では、timer tickをkernel層のpreemption判断smokeの契機として使う。
 * timer moduleが所有するのは引き続きtick状態だけである。READY task選択、
 * dispatcher current確定、context switch層の呼び出しは行わない。
 * ハードウェアタイマ割り込みと完全な割り込み復帰時dispatchは将来作業として残す。
 */

/**
 * @brief 現在のsystem tickを取得する。
 *
 * @details
 * 読み取り専用APIであり、tick counterやtask状態を変更しない。
 *
 * @return 起動後から明示的に進められたsystem tick数。
 */
unsigned long timer_get_ticks(void);

#endif
