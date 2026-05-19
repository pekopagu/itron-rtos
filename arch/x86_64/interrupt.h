/*
 * SPDX-License-Identifier: MIT
 */

/**
 * @file interrupt.h
 * @brief x86_64向け割り込み・CPU例外の観測基盤。
 *
 * @details
 * 第7章7.1の教育用例外受信基盤で使うIDTを初期化するための、
 * arch/x86_64ローカルな公開境界である。kernel共通層へは初期化APIだけを
 * 見せ、IDT entry形式、IDTR、GDT selector前提、entry stubは
 * arch/x86_64側に閉じ込める。
 *
 * これはtimer interrupt、IRQ routing、preemption、scheduler、
 * dispatcher、context switchのためのinterfaceではない。
 */

#ifndef ITRON_RTOS_ARCH_X86_64_INTERRUPT_H
#define ITRON_RTOS_ARCH_X86_64_INTERRUPT_H

/**
 * @brief CPU例外到達を観測するためにx86_64 IDTを初期化する。
 *
 * @details
 * 代表的なCPU例外handlerだけを登録した最小IDTをloadする。
 * 初期化の進行はHAL console経由でログ出力する。maskable hardware
 * interruptは有効化せず、例外到達をschedulerやdispatcherへ接続しない。
 *
 * @return 成功時は0、初期化失敗時は負値。
 */
int arch_interrupt_init(void);

/**
 * @brief 第7章7.1のsmoke検証用に制御された例外を発生させる。
 *
 * @details
 * 明示的な検証buildだけで使うhelperである。共通handlerは例外到達を
 * ログ出力した後にhaltしてよいため、通常bootでは呼び出さない。
 */
void arch_interrupt_trigger_validation_exception(void);

/**
 * @brief timer IRQ tick接続確認用にIRQ0を開き、maskable interruptを有効化する。
 *
 * @details
 * 第7章7.3の明示validation buildだけで使うarch-local helperである。
 * IRQ0をlegacy PIC上でunmaskし、`sti` によってCPUのmaskable interruptを
 * 受けられる状態にする。第8章8.1ではhandler内で `timer_tick()` を1回呼ぶが、
 * PIT programming、周期設定、scheduler、dispatcher、context switch、
 * task state変更は行わない。
 *
 * 通常bootでは呼び出さない。QEMU serial logでvector 32 handler到達とtick更新だけを
 * 観測するための一時的な入口であり、後続章のtimer subsystem接続で置き換える。
 *
 * @note この関数は第7章7.4の観測モデルに属する。目的はIRQ0/vector 32の
 * handler到達とtick更新をQEMU serial logで確認することであり、通常運用の割り込み開始ではない。
 *
 * @warning 割り込み中の観測ログは通常boot logへ混ざり得る。ログ順序の保証、
 * nested interrupt、連続割り込み、通常の割り込み復帰、preemption接続は扱わない。
 */
void arch_interrupt_enable_timer_entry_validation(void);

#endif
