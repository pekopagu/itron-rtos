/*
 * SPDX-License-Identifier: MIT
 */

/**
 * @file preemption.h
 * @brief preemption decision の薄い kernel 境界。
 *
 * @details
 * この header は、timer IRQ handler などの外側の契機から scheduler の
 * preemption decision を呼ぶための public API を定義する。第8章8.2では
 * 「判断入口へ到達した」ことだけを観測対象にし、dispatcher commit、
 * context switch、task state 変更、dispatch pending の確定は行わない。
 */

#ifndef ITRON_RTOS_PREEMPTION_H
#define ITRON_RTOS_PREEMPTION_H

/**
 * @brief IRQ 起点で preemption decision を評価し、観測ログを出力する。
 *
 * @details
 * timer IRQ handler が `timer_tick()` を完了した後に呼ぶための薄い境界である。
 * 内部では dispatcher から logical current task を読み取り、既存 scheduler の
 * preemption decision helper を呼ぶ。結果は validation log として出力するだけで、
 * task の切り替え、current の再確定、task state 変更、dispatch pending 更新は
 * 意図的に行わない。第8章8.3以降で dispatch pending を観測するための入口であり、
 * 完全な割り込み駆動 preemption ではない。
 *
 * @param なし。
 * @return なし。
 * @note 割り込み中の serial log は第7章7.4の観測方針どおり validation 専用であり、
 *       通常 boot log へ混ざる可能性がある。
 */
void preemption_evaluate_from_irq(void);

#endif
