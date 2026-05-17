/*
 * SPDX-License-Identifier: MIT
 */

/**
 * @file interrupt.h
 * @brief kernel共通層向けHAL interrupt API。
 *
 * @details
 * kernel共通層がCPU例外受信基盤を初期化するための抽象境界である。
 * IDT、GDT selector、lidt、例外entry stubなどのCPU固有処理は、
 * 各archのHAL実装の内側に閉じ込める。
 *
 * このAPIは第7章7.1の例外受信基盤を起動するための入口であり、
 * timer interrupt、IRQ routing、preemption、scheduler、dispatcher、
 * context switchを開始するためのAPIではない。
 */

#ifndef ITRON_RTOS_HAL_INTERRUPT_H
#define ITRON_RTOS_HAL_INTERRUPT_H

/**
 * @brief CPU例外受信基盤をHAL境界越しに初期化する。
 *
 * @details
 * kernel共通層はこの関数だけを呼び、実際のIDT構築やIDT loadはarch固有の
 * HAL実装へ委譲する。これにより、kernelからx86_64固有headerへの直接依存を
 * 避ける。
 *
 * @return 成功時は0、初期化失敗時は負値。
 */
int hal_interrupt_init(void);

/**
 * @brief 割り込みコントローラ初期化を HAL 境界越しに要求する。
 *
 * @details
 * kernel 共通層はこの関数だけを呼び、PIC の I/O port、vector base、初期化 sequence は
 * arch 固有実装へ閉じる。第7章7.2では legacy PIC を remap し、全 IRQ を mask した
 * ままにする。PIT、timer ISR、EOI、scheduler、dispatcher、preemption、context switch
 * には接続しない。
 *
 * @return 成功時は 0、初期化失敗時は負値。
 */
int hal_interrupt_controller_init(void);

/**
 * @brief 明示的な検証buildでCPU例外到達を確認する。
 *
 * @details
 * 通常bootでは呼び出さない。検証buildではarch固有の安全な検証trapへ委譲し、
 * handler到達ログを観測した後に停止してよい。
 */
void hal_interrupt_trigger_validation_exception(void);

#endif
