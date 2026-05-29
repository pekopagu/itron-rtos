/*
 * SPDX-License-Identifier: MIT
 */

/**
 * @file dispatcher.h
 * @brief 第3章3.3の現在タスク確定境界。
 *
 * @details
 * dispatcherは論理的な現在タスク境界を担当する。schedulerが選択したタスクを受け取り、
 * task moduleへREADYからRUNNINGへの論理状態遷移を依頼して現在タスクとして確定する。
 *
 * この章では、実行前の最終状態を記録するだけである。タスク入口関数の呼び出し、
 * コンテキストスイッチ、スタック切り替え、レジスタ保存・復元、割り込み、タイマ、
 * プリエンプション、動的メモリ確保は行わない。第4章以降では、commit済みの
 * 現在タスクを入口関数実行モデルへの入力として利用できる。
 */

#ifndef ITRON_RTOS_DISPATCHER_H
#define ITRON_RTOS_DISPATCHER_H

#include "task.h"

#define DISPATCHER_OK            0
#define DISPATCHER_ERR_INVAL     (-1)
#define DISPATCHER_ERR_BAD_STATE (-2)
#define DISPATCHER_ERR_NOT_FOUND (-3)

/**
 * @brief dispatcherの現在タスク状態を初期化する。
 *
 * @details
 * 現在タスクをクリアし、dispatcherを明示的な未設定状態から開始できるようにする。
 * TCB状態は変更せず、taskも実行しない。
 *
 * 今回の責務は、選択候補タスクが現在タスクとして確定済みかどうかを保持することに限定する。
 * 将来の実行処理は、この初期化契約を変えずに確定済み現在タスクを利用できる。
 */
void dispatcher_init(void);

/**
 * @brief 選択候補タスクを現在タスクとして確定する。
 *
 * @details
 * scheduler_select_next()が返した選択候補タスクを受け取り、現在タスクとして確定する責務を持つ。
 * このAPIが行うのは、task moduleを通したREADYからRUNNINGへの論理状態遷移だけである。
 *
 * タスク入口関数は呼び出さない。コンテキストスイッチは行わない。スタック切り替えは行わない。
 * レジスタ保存・復元は行わない。確定済み現在タスクは、第4章の入口関数実行モデルへ
 * 接続するための前提になる。
 *
 * @param selected 読み取り専用の選択候補タスク。NULLは不正。
 * @return 成功時はDISPATCHER_OK、失敗時は負のDISPATCHER_ERR_*値。
 */
int dispatcher_commit_current(const tcb_t *selected);

/**
 * @brief dispatcherが確定した現在タスクを返す。
 *
 * @details
 * 返されるTCBは呼び出し側から読み取り専用として扱う。NULLは、まだ選択候補タスクが
 * 確定されていないことを示す。このAPIはdispatcher状態を観測するだけであり、
 * タスク状態の変更やタスクコードの実行は行わない。
 *
 * @return 現在タスクのTCB。現在タスク未設定時はNULL。
 */
const tcb_t *dispatcher_get_current(void);

/**
 * @note 第6章6.3のpreemption foundationでは、schedulerが高優先度READY候補を
 * 見つける場合がある。ただし、その候補をcurrentに確定するのはschedulerではない。
 * current taskの確定はdispatcherの責務に残し、register save/restoreは
 * このinterfaceの外側に残す。
 */

/**
 * @brief dispatcher層からtask context切替へ進む境界を実行する。
 *
 * @details
 * 第9章9.2では、dispatcherがcurrent commitだけでなく「切替先へ進める
 * 境界」を持つことを明確化する。このAPIは、9.1で追加した起動時
 * task-to-task context switch smoke補助APIへ委譲する薄い境界である。
 *
 * 第10章10.4では、yield APIからの協調switch入口としても使う。
 * 通常のdispatcher smokeではfrom taskをRUNNINGからREADYへ戻すが、
 * `yield_tsk()` が先にREADYへ戻した協調API経路ではREADY化済みfromを受け付ける。
 * dispatch pending消費、interrupt exit boundaryからの接続、timer IRQからの実切替、
 * preemption、time slice、task終了状態確定は行わない。
 *
 * @param from 切替元task。通常はRUNNING task。10.4のyield経路ではREADY化済みtask。
 * @param to 切替先task。9.2 smokeではREADY task。
 * @return 成功時はDISPATCHER_OK、失敗時は負のDISPATCHER_ERR_*または
 * task_context層から返された負の値。
 */
/**
 * @note 第9章9.3では、この境界でfrom taskをRUNNINGからREADYへ戻し、
 * to taskをREADYからRUNNINGへ進める。第10章10.4のyield経路では、
 * from taskがすでにREADYへ戻っている場合にその状態を維持する。entry return時の最終状態確定、
 * dispatch pending消費、interrupt exit接続、timer IRQからの実切替は
 * まだ行わない。
 */
int dispatcher_switch_to(tcb_t *from, tcb_t *to);

#endif
