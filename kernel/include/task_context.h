/*
 * SPDX-License-Identifier: MIT
 */

/**
 * @file task_context.h
 * @brief 第5章5.3向けのTCB-level最小context switch補助API。
 *
 * @details
 * この層はtask contextの検証、初回task stack frameの準備、学習用serial logの
 * 出力を行い、CPU register保存・復元はx86_64 arch primitiveへ委譲する。
 * READY task選択、current commit、割り込み処理、timer導入、preemptionは行わない。
 */

#ifndef ITRON_RTOS_TASK_CONTEXT_H
#define ITRON_RTOS_TASK_CONTEXT_H

#include "task.h"

#define TASK_CONTEXT_OK             0
#define TASK_CONTEXT_ERR_INVAL     (-1)
#define TASK_CONTEXT_ERR_BAD_STATE (-2)

/**
 * @brief 登録済みtask contextを初回stack-based entry用に準備する。
 *
 * @details
 * 第5章5.2では `context.rsp` をmetadataとして `stack_top` で初期化した。
 * 第5章5.3では、task stack上に最小のreturn targetを置き、この値を実際の
 * 復元候補へ進める。準備されたtargetは学習用trampolineへ入り、task entryを
 * 1回だけ呼び出してからboot contextへ戻る。これは完全なtask lifecycle modelではない。
 *
 * @param task 準備対象の登録済みtask。
 * @return 成功時はTASK_CONTEXT_OK、失敗時はTASK_CONTEXT_ERR_*。
 */
int task_context_prepare_initial_frame(tcb_t *task);

/**
 * @brief あるtask contextから別のtask contextへ切り替える。
 *
 * @details
 * wrapperはswitch元・先task、保存前後の `context.rsp` をlogへ出力し、
 * 実際のregister処理をarch層へ委譲する。scheduler選択とdispatcher current commitは
 * この関数の外側で完了している必要がある。
 *
 * @param from 登録済みのswitch元task。
 * @param to 登録済みのswitch先task。
 * @return switch経路が戻った場合はTASK_CONTEXT_OK、失敗時はerror。
 */
int task_context_switch(tcb_t *from, tcb_t *to);

/**
 * @brief boot contextから準備済みtask contextへ切り替える。
 *
 * @details
 * このhelperは第5章5.3のsmoke pathである。timer、割り込みhandler、
 * preemption、public yield APIを追加せず、kernelが1つのtask stackへ入り
 * boot contextへ戻るための最小経路を提供する。
 *
 * @param to 登録済みかつ準備済みのswitch先task。
 * @return taskがbootへ戻った場合はTASK_CONTEXT_OK、失敗時はerror。
 */
int task_context_switch_to_task(tcb_t *to);

/**
 * @brief 準備済みtask stack上のassembly trampolineから呼ばれる入口。
 *
 * @details
 * assembly trampolineはtask contextを復元し、復元された `r12` 経由でTCB pointerを
 * 渡す。このC関数はtask entryを1回呼び出し、そのreturnを観測してからboot contextへ
 * 切り戻す。assembly境界から呼ぶためだけに公開している。
 *
 * @param task stack/contextが復元されたtask。
 */
void task_context_enter(tcb_t *task);

#endif
