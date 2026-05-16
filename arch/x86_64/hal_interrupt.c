/*
 * SPDX-License-Identifier: MIT
 */

/**
 * @file hal_interrupt.c
 * @brief x86_64向けHAL interrupt実装。
 *
 * @details
 * kernel共通層に見せるHAL interrupt APIを、x86_64固有の
 * interrupt/exception foundationへ委譲する。IDT、GDT selector、lidt、
 * 例外entry stub、検証trapの詳細はこの層より下に閉じ込める。
 */

#include "hal/interrupt.h"

#include "interrupt.h"

/**
 * @brief HAL interrupt初期化をx86_64固有実装へ委譲する。
 *
 * @details
 * kernelから見える入口はHAL APIに限定し、arch固有のIDT初期化APIは
 * このadapter内だけで呼ぶ。timer interruptやpreemptionは開始しない。
 *
 * @return 成功時は0、初期化失敗時は負値。
 */
int hal_interrupt_init(void)
{
    return arch_interrupt_init();
}

/**
 * @brief 検証用例外発生をx86_64固有実装へ委譲する。
 *
 * @details
 * `int3` などのCPU固有trap命令はkernel共通層へ見せない。
 */
void hal_interrupt_trigger_validation_exception(void)
{
    arch_interrupt_trigger_validation_exception();
}
