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
 * 第9章9.4ではentry return後のtask lifecycle確定だけをこの層に追加し、
 * dispatcherのswitch boundary責務とは分離したままDORMANTへ最終化する。
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
 * @brief boot contextからfirst taskへ入り、firstからsecond taskへ一度だけ切り替える。
 *
 * @details
 * 第9章9.1の起動時smoke専用APIである。2つのtask stack frameを準備し、
 * first taskのentry return観測点でsecond task contextへ切り替える。
 * second taskのentry return後は既存のboot復帰経路へ戻る。
 *
 * これはtask間context switchを観測するための教育用モデルである。
 * 第10章10.4では `yield_tsk()` がdispatcher境界へ接続された結果として
 * この補助経路へ到達できる。ただし割り込みexitからの切替、dispatch pending消費、
 * timer IRQからの切替、preemption、time sliceは実装しない。
 *
 * @param first boot contextから最初に入るtask。dispatcherでRUNNING確定済みであること。
 * @param second first taskから切り替えるtask。READY状態からsmoke用にRUNNINGへ遷移させる。
 * @return bootへ戻った場合はTASK_CONTEXT_OK、失敗時はTASK_CONTEXT_ERR_*。
 */
/*
 * 第9章9.2以降、上位層からの切替開始点はdispatcher_switch_to()境界へ
 * 寄せる。この関数はtask_context層に残るboot-time smoke補助APIであり、
 * 正式なdispatcher境界として直接依存する対象ではない。
 */
/**
 * @note 第9章9.3以降、RUNNING/READY状態遷移とdispatcher current更新は
 * dispatcher_switch_to()側の責務である。第9章9.4では、このtask_context層が
 * entry return後の起動分完了をDORMANTへ最終化するが、dispatcher current更新、
 * READY task選択、dispatch pending消費、interrupt exit接続は行わない。
 */
int task_context_switch_to_task_pair(tcb_t *first, tcb_t *second);

/**
 * @note 第6章6.3のpreemption判断は、この層へcontext switchを依頼する前段で行う。
 * task context層の責務は、準備済みstack/contextの検証とregister save/restoreの
 * arch層への委譲だけである。timer tickの解釈、READY task選択、dispatcher current
 * 確定は行わない。
 */

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
