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
/**
 * @brief IRQ由来のpreemption decisionを評価し、dispatch pendingを更新する。
 *
 * @details
 * 第8章8.3から第11章11.3の境界として、scheduler/dispatcher状態は既存のkernel API経由で
 * 読み取るだけにする。currentより高優先度のREADY taskを検出した場合は後続観測用に
 * dispatch pendingを記録するが、同一優先度READYだけの場合は
 * `same-priority-not-timeslice-target` としてpendingをrequestしない。
 * dispatcher commit、context switch、stack切り替え、register保存・復元、task状態変更は行わない。
 *
 * @return dispatch pending観測用のnot-requested reason。pending要求時はNULL。
 */
/**
 * @brief IRQ由来のpreemption decisionを評価し、固定順序の観測ログとpending更新を行う。
 * @details
 * 第11章11.4では、IRQ由来preemptionログの順序を
 * current -> candidate/no-candidate -> decision -> dispatch pending の順に固定する。
 * priority値が小さいREADY taskだけを切替候補とし、同一priority READYは
 * `same-priority-not-timeslice-target` のままpending requestしない。timer IRQ handler本体から
 * `yield_tsk()` や `dispatcher_switch_to()` を直接呼ぶ経路はここでも追加しない。
 *
 * @return dispatch pending観測用のnot-requested reason。pending要求時はNULL。
 */
const char *preemption_evaluate_from_irq(void);

#endif
