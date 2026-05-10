/*
 * SPDX-License-Identifier: MIT
 */

/**
 * @file context_switch.h
 * @brief x86_64最小context switch primitive境界。
 *
 * @details
 * このarch-local interfaceは、第5章5.3の協調的switch smokeで使う
 * CPU register保存・復元処理だけを担当する。task選択、current commit、
 * log出力、割り込み処理、preemptionは担当しない。
 */

#ifndef ITRON_RTOS_ARCH_X86_64_CONTEXT_SWITCH_H
#define ITRON_RTOS_ARCH_X86_64_CONTEXT_SWITCH_H

#include "task.h"

/**
 * @brief 現在のcallee-saved CPU contextを保存し、別のcontextを復元する。
 *
 * @details
 * `rsp`, `rbp`, `rbx`, `r12`-`r15` を `from` へ保存し、同じregister群を
 * `to` から復元する。制御は、復元したstack上のreturn addressへ戻ることで
 * 再開される。呼び出し側は、このprimitiveへ入る前に両方のpointer検証と
 * 復元先stackの準備を完了しておく必要がある。
 *
 * @param from 現在実行中のCPU状態を保存するcontext領域。
 * @param to 復元するcontext領域。
 * @return 後続のswitchで `from` が復元されたときに戻る。
 * @note caller-saved register、割り込み状態、timer、preemptionは
 * 意図的にこのprimitiveの対象外である。
 */
void arch_context_switch(task_context_t *from, const task_context_t *to);

#endif
