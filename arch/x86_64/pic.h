/*
 * SPDX-License-Identifier: MIT
 */

/**
 * @file pic.h
 * @brief x86_64 向け legacy PIC 初期化境界。
 *
 * @details
 * このヘッダは arch/x86_64 内で使う PIC 専用の境界である。kernel 共通層は
 * このヘッダを直接 include せず、HAL interrupt API を通して初期化だけを依頼する。
 *
 * 第7章7.2では legacy PIC を学習用の割り込みコントローラとして初期化し、
 * IRQ0 を CPU exception vector と衝突しない vector 32 以降へ移動する。ただし、
 * PIT、timer ISR、EOI、scheduler、dispatcher、preemption、context switch には
 * まだ接続しない。
 */

#ifndef ITRON_RTOS_ARCH_X86_64_PIC_H
#define ITRON_RTOS_ARCH_X86_64_PIC_H

/**
 * @brief legacy PIC を remap し、全 IRQ を mask した状態にする。
 *
 * @details
 * master PIC を vector 32、slave PIC を vector 40 へ remap する。初期化完了後も
 * IRQ line はすべて mask したままにし、timer interrupt が未実装の段階で
 * hardware interrupt が既存の boot-time smoke flow を乱さないようにする。
 *
 * この関数は割り込み配送の準備だけを行う。IDT entry 登録、PIT 設定、EOI、
 * timer tick 呼び出し、scheduler/dispatcher 連携は行わない。
 *
 * @return 成功時は 0。現時点では port I/O の失敗検出を行わないため常に成功を返す。
 */
int arch_pic_init(void);

/**
 * @brief 指定した PIC IRQ line を mask する。
 *
 * @details
 * IRQ 0-15 の mask bit だけを更新する。範囲外の IRQ は無視し、既存 mask 状態を
 * 変更しない。第7章7.2では API 形状を用意するだけで、boot path から個別 IRQ を
 * unmask する用途にはまだ使わない。
 *
 * @param irq mask する IRQ line。0-15 のみ有効。
 */
void arch_pic_mask_irq(unsigned int irq);

/**
 * @brief 指定した PIC IRQ line を unmask する。
 *
 * @details
 * IRQ 0-15 の mask bit だけを更新する。範囲外の IRQ は無視し、既存 mask 状態を
 * 変更しない。この API は第7章7.3以降で IRQ0 を timer interrupt entry へ接続する
 * 前段のために用意する。
 *
 * @param irq unmask する IRQ line。0-15 のみ有効。
 */
void arch_pic_unmask_irq(unsigned int irq);

/**
 * @brief 指定したPIC IRQ lineの処理完了をlegacy PICへ通知する。
 *
 * @details
 * IRQ handlerが最小限の観測処理を終えた後に呼ぶarch-local APIである。
 * port I/Oとmaster/slave PICのEOI順序はx86_64 PIC moduleに閉じる。
 * IRQ 8-15ではslave PICへEOIを送ってからmaster PICへEOIを送る。
 *
 * このAPIはmask stateを変更しない。範囲外IRQは無視する。第7章7.3では
 * IRQ0 timer interrupt entry到達後の完了通知位置を明示するために使い、
 * `timer_tick()`、scheduler、dispatcher、context switchとは接続しない。
 * APIC/IOAPIC/LAPIC対応時には、この境界の内側を置き換える想定である。
 *
 * @param irq EOIを送るIRQ line。0-15のみ有効。
 */
void arch_pic_send_eoi(unsigned int irq);

#endif
