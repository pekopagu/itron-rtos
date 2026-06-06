/*
 * SPDX-License-Identifier: MIT
 */

/**
 * @file kernel.c
 * @brief 最小kernel起動と初期タスク登録・簡易scheduler確認（第2章2.1、第2章2.3、第3章3.1、第3章3.2）
 *
 * @details
 * 第2章2.1の最小kernel起動点として `kernel_main()` を提供し、
 * 第2章2.3のHAL console APIだけを通じてログ出力を行う。
 * 第3章3.1では、タスクを実行せずに `task_init()`、`task_register()`、
 * `task_dump()` を呼び出し、QEMUシリアルログで登録状態を確認する。
 * 第3章3.2では `scheduler_select_next()` でREADYタスクを1つ選択し、
 * 選択結果だけをHAL console経由で表示する。entry関数は呼び出さない。
 * 第4章4.1では、dispatcherでcurrentとしてcommit済みのタスクについて、
 * entryを通常のC関数呼び出しとして1回だけ直接呼び出す。
 * 第4章4.2では、entry returnを正式なtask終了ではなく、
 * 観測可能な起動時検証イベントとして扱う。
 * 第4章4.3では、entry returnをcooperative return eventとして扱い、
 * RUNNING taskをREADYへ戻して再びscheduler候補にすることで、
 * 複数回のentry呼び出しを有限の起動時検証loopで観測する。
 * これは一時的なboot-time verification modelであり、第5章では
 * context-switch-based executionへ置き換える前提である。
 * 第6章6.2では、timer foundationのsystem tickを起動時に明示的に進め、
 * QEMU serial logで時間経過を観測する。まだtimer interrupt、preemption、
 * time slice、delay/timeoutとは接続しない。
 *
 * kernel層はarch依存コードを直接呼ばず、依存方向は
 * kernel → HAL → arch(x86_64) → serial → COM1 である。
 */

#include "dispatcher.h"
#include "delay_queue.h"
#include "dispatch_pending.h"
#include "hal/console.h"
#include "hal/interrupt.h"
#include "itron_api.h"
#include "preemption.h"
#include "scheduler.h"
#include "semaphore.h"
#include "task.h"
#include "task_context.h"
#include "timer.h"

#include <stddef.h>

#define TASK_STACK_SIZE 1024
#define TASK_STACK_ALIGNMENT 16
#define KERNEL_COOPERATIVE_ENTRY_LIMIT 3UL

/*
 * 第5章5.1のtask stack foundation用の静的stack領域。
 * 各領域はTCB metadataとして登録し、stack_top候補をログで観測するために使う。
 * まだCPUのRSPへロードせず、task entryもこの領域上では実行しない。
 */
static unsigned char task_a_stack[TASK_STACK_SIZE] __attribute__((aligned(TASK_STACK_ALIGNMENT)));
static unsigned char task_b_stack[TASK_STACK_SIZE] __attribute__((aligned(TASK_STACK_ALIGNMENT)));
static unsigned char task_c_stack[TASK_STACK_SIZE] __attribute__((aligned(TASK_STACK_ALIGNMENT)));
static unsigned char task_yield_from_stack[TASK_STACK_SIZE] __attribute__((aligned(TASK_STACK_ALIGNMENT)));
static unsigned char task_yield_to_stack[TASK_STACK_SIZE] __attribute__((aligned(TASK_STACK_ALIGNMENT)));
static unsigned char task_wai_sem_from_stack[TASK_STACK_SIZE] __attribute__((aligned(TASK_STACK_ALIGNMENT)));
static unsigned char task_wai_sem_to_stack[TASK_STACK_SIZE] __attribute__((aligned(TASK_STACK_ALIGNMENT)));
static unsigned char task_dly_from_stack[TASK_STACK_SIZE] __attribute__((aligned(TASK_STACK_ALIGNMENT)));
static unsigned char task_dly_to_stack[TASK_STACK_SIZE] __attribute__((aligned(TASK_STACK_ALIGNMENT)));
static unsigned char task_twai_from_stack[TASK_STACK_SIZE] __attribute__((aligned(TASK_STACK_ALIGNMENT)));
static unsigned char task_twai_to_stack[TASK_STACK_SIZE] __attribute__((aligned(TASK_STACK_ALIGNMENT)));
static unsigned char task_tick_delay_stack[TASK_STACK_SIZE] __attribute__((aligned(TASK_STACK_ALIGNMENT)));
static unsigned char task_tick_twai_stack[TASK_STACK_SIZE] __attribute__((aligned(TASK_STACK_ALIGNMENT)));
static unsigned char task_tick_current_stack[TASK_STACK_SIZE] __attribute__((aligned(TASK_STACK_ALIGNMENT)));

static void task_wai_sem_to(void);
static void task_dly_to(void);
static void task_twai_to(void);
static void task_tick_delay(void);
static void task_tick_twai(void);
static void task_tick_current(void);

/**
 * @brief 符号なし整数をHAL consoleへ10進出力する。
 *
 * @details
 * `task_register()` の戻り値ログを出すためのkernel内補助関数。
 * printfを導入せず、HAL境界を保ったまま最小限の数値出力を行う。
 *
 * @param value 出力する符号なし整数。
 * @return なし。
 * @note タスク管理ロジックには関与しない表示専用処理である。
 */
static void kernel_write_uint(unsigned long value)
{
    char buffer[20];
    int index = 0;

    if (value == 0) {
        hal_console_putc('0');
        return;
    }

    while (value > 0) {
        buffer[index++] = (char)('0' + (value % 10));
        value /= 10;
    }

    while (index > 0) {
        hal_console_putc(buffer[--index]);
    }
}

/**
 * @brief 符号付き整数をHAL consoleへ10進出力する。
 *
 * @details
 * 成功時IDと負のエラーコードを同じログ経路で確認できるようにする。
 *
 * @param value 出力する符号付き整数。
 * @return なし。
 * @note HAL console APIだけを使い、serial実装を直接呼ばない。
 */
static void kernel_write_int(int value)
{
    if (value < 0) {
        hal_console_putc('-');
        kernel_write_uint((unsigned long)(-value));
        return;
    }

    kernel_write_uint((unsigned long)value);
}

/**
 * @brief `task_register()` の戻り値を起動ログへ出力する。
 *
 * @details
 * 登録成功IDまたはエラーコードをQEMUシリアルログで確認する。
 *
 * @param name 登録対象タスク名。
 * @param result `task_register()` の戻り値。
 * @return なし。
 * @note 登録結果を表示するだけで、タスク状態は変更しない。
 */
static void kernel_log_task_register_result(const char *name, int result)
{
    hal_console_write("[kernel] task_register ");
    hal_console_write(name);
    hal_console_write(" returned ");
    kernel_write_int(result);
    hal_console_write("\n");
}

/**
 * @brief タスク状態を起動ログ表示用の文字列へ変換する。
 *
 * @details
 * schedulerの選択結果をQEMUシリアルログで確認するための表示補助である。
 * 表示だけを担当し、タスク状態の変更や実行制御は行わない。
 *
 * @param state 表示するタスク状態。
 * @return 状態名を表す静的文字列。
 */
static const char *kernel_task_state_to_string(task_state_t state)
{
    switch (state) {
    case TASK_STATE_UNUSED:
        return "UNUSED";
    case TASK_STATE_DORMANT:
        return "DORMANT";
    case TASK_STATE_READY:
        return "READY";
    case TASK_STATE_RUNNING:
        return "RUNNING";
    case TASK_STATE_WAITING:
        return "WAITING";
    default:
        return "UNKNOWN";
    }
}

/**
 * @brief schedulerの選択結果をHAL consoleへ出力する。
 *
 * @details
 * scheduler自身はHALやarchへ依存しないため、起動時確認用のログはkernel側で出力する。
 * 選択されたTCBのentryは呼び出さず、id、name、priority、stateだけを表示する。
 *
 * @param label 確認ケース名。
 * @param task scheduler_select_next() の戻り値。NULLならREADYタスクなし。
 * @return なし。
 * @note 表示専用であり、RUNNING遷移、コンテキストスイッチ、スタック切り替えは行わない。
 */
static void kernel_log_scheduler_selection(const char *label, const tcb_t *task)
{
    const char *safe_label = (label != NULL) ? label : "(no-label)";
    const char *safe_name;
    const char *state_name;
    const char *safe_state;

    hal_console_write("[scheduler] ");
    hal_console_write(safe_label);

    if (task == NULL) {
        hal_console_write(" selected: none\n");
        return;
    }

    safe_name = (task->name != NULL) ? task->name : "(null)";
    state_name = kernel_task_state_to_string(task->state);
    safe_state = (state_name != NULL) ? state_name : "UNKNOWN";

    hal_console_write(" selected: id=");
    kernel_write_int(task->id);
    hal_console_write(" name=");
    hal_console_write(safe_name);
    hal_console_write(" prio=");
    kernel_write_int(task->priority);
    hal_console_write(" state=");
    hal_console_write(safe_state);
    hal_console_write("\n");
}

/**
 * @brief schedulerのpreemption判断をログ用理由文字列へ変換する。
 *
 * @details
 * schedulerは意図的に小さな判断分類だけを返す。kernel smoke層では、
 * QEMU serial logで観測しやすいように、より具体的な理由文字列へ変換する。
 * これによりlogging policyをscheduler.cへ入れず、schedulerの責務を
 * 「選択と比較のみ」に保つ。
 *
 * @param decision scheduler helperが生成した読み取り専用の判断結果。
 * @return QEMU serial出力用の静的な理由文字列。
 */
static const char *kernel_preempt_reason_to_string(
    scheduler_preempt_decision_t decision
)
{
    if (decision.reason == SCHEDULER_PREEMPT_NEEDED) {
        return "higher-priority-ready";
    }

    if (decision.reason == SCHEDULER_PREEMPT_INVALID_CURRENT) {
        return "invalid-current";
    }

    if (decision.reason == SCHEDULER_PREEMPT_SAME_PRIORITY) {
        return "same-priority-not-timeslice-target";
    }

    if (decision.current == NULL) {
        return "no-current";
    }

    if (decision.candidate == NULL) {
        return "no-ready";
    }

    return "no-higher-priority-ready";
}

/**
 * @brief preemption判断ログ用にtask情報を短く出力する。
 *
 * @details
 * このhelperは表示専用である。TCBを変更せず、taskをcurrentとして確定しない。
 * kernel.cに置くことで、scheduler.cへHAL logging依存を追加しない。
 *
 * @param label `" current"` や `" candidate"` などの接頭辞。
 * @param task 表示対象task。NULLも許容する。
 */
static void kernel_log_preempt_task(const char *label, const tcb_t *task)
{
    const char *safe_name;
    const char *state_name;
    const char *safe_state;

    hal_console_write(label);
    if (task == NULL) {
        hal_console_write("=none");
        return;
    }

    safe_name = (task->name != NULL) ? task->name : "(null)";
    state_name = kernel_task_state_to_string(task->state);
    safe_state = (state_name != NULL) ? state_name : "UNKNOWN";

    hal_console_write(" id=");
    kernel_write_int(task->id);
    hal_console_write(" name=");
    hal_console_write(safe_name);
    hal_console_write(" prio=");
    kernel_write_int(task->priority);
    hal_console_write(" state=");
    hal_console_write(safe_state);
}

/**
 * @brief preemption判断の観測結果を出力する。
 *
 * @details
 * `"switch-target"` は、高優先度READY taskが切り替え候補として選ばれた
 * ことだけを意味する。この関数はそのtaskへdispatchせず、context switchも
 * 実行しない。current確定はdispatcher、register save/restoreはcontext
 * switch境界の責務として残す。
 *
 * @param label 検証シナリオ名。
 * @param decision ログ出力するscheduler判断結果。
 */
static void kernel_log_preemption_decision(
    const char *label,
    scheduler_preempt_decision_t decision
)
{
    const char *safe_label = (label != NULL) ? label : "(no-label)";
    const char *result =
        (decision.reason == SCHEDULER_PREEMPT_NEEDED) ? "switch-target" : "no-switch";

    hal_console_write("[preempt] ");
    hal_console_write(safe_label);
    hal_console_write(" result=");
    hal_console_write(result);
    hal_console_write(" reason=");
    hal_console_write(kernel_preempt_reason_to_string(decision));
    kernel_log_preempt_task(" current", decision.current);
    kernel_log_preempt_task(" candidate", decision.candidate);
    hal_console_write("\n");
}

/**
 * @brief dispatcherの現在タスク確定結果をHAL consoleへ出力する。
 *
 * @details
 * dispatcher.cはHAL consoleへ依存しないため、起動時検証ログはkernel.cから出力する。
 * 確定成功はcurrent/RUNNING committedとして出力し、失敗はcommit failedとして
 * selectedログと区別できるようにする。
 *
 * @param result dispatcher_commit_current()の戻り値。
 * @return なし。
 * @note 表示専用の処理であり、入口関数呼び出し、コンテキストスイッチ、
 * スタック切り替え、レジスタ保存・復元は行わない。
 */
static void kernel_log_dispatcher_commit_result(int result)
{
    const tcb_t *current;
    const char *safe_name;
    const char *state_name;
    const char *safe_state;

    if (result != DISPATCHER_OK) {
        hal_console_write("[dispatcher] commit failed: err=");
        kernel_write_int(result);
        hal_console_write("\n");
        return;
    }

    current = dispatcher_get_current();
    if (current == NULL) {
        hal_console_write("[dispatcher] committed current: none\n");
        return;
    }

    safe_name = (current->name != NULL) ? current->name : "(null)";
    state_name = kernel_task_state_to_string(current->state);
    safe_state = (state_name != NULL) ? state_name : "UNKNOWN";

    hal_console_write("[dispatcher] committed current: id=");
    kernel_write_int(current->id);
    hal_console_write(" name=");
    hal_console_write(safe_name);
    hal_console_write(" prio=");
    kernel_write_int(current->priority);
    hal_console_write(" state=");
    hal_console_write(safe_state);
    hal_console_write("\n");
}

/**
 * @brief entry呼び出し前のcurrent task情報をHAL consoleへ出力する。
 *
 * @details
 * 第4章4.1のboot-time verification modelとして、current taskのentryを
 * 通常のC関数呼び出しで直接呼ぶ直前に観測点を作る。
 * このログはkernel runtime側に置き、scheduler、dispatcher、task管理へ
 * entry実行責務やログ責務を移さない。
 *
 * @param current entry呼び出し対象のcurrent task。
 * @return なし。
 * @note 表示専用であり、TCB状態やcurrent taskを変更しない。
 */
static void kernel_log_entry_call(const tcb_t *current)
{
    const char *safe_name = (current->name != NULL) ? current->name : "(null)";
    const char *state_name = kernel_task_state_to_string(current->state);
    const char *safe_state = (state_name != NULL) ? state_name : "UNKNOWN";

    hal_console_write("[entry] calling current: id=");
    kernel_write_int(current->id);
    hal_console_write(" name=");
    hal_console_write(safe_name);
    hal_console_write(" prio=");
    kernel_write_int(current->priority);
    hal_console_write(" state=");
    hal_console_write(safe_state);
    hal_console_write("\n");
}

/**
 * @brief entry return後のcurrent task情報をHAL consoleへ出力する。
 *
 * @details
 * 4.2ではentryがreturnしても正式なtask終了状態を導入しない。
 * RUNNINGからDORMANT、READY、WAITING、EXITED相当の状態へは遷移させず、
 * returnは起動時検証ログで観測するだけに留める。
 *
 * @param current returnしたentryを持つcurrent task。
 * @return なし。
 * @note return後もcurrent taskとTASK_STATE_RUNNINGは保持する。
 * @note return後にschedulerを再実行せず、entryも再度呼び出さない。
 */
static void kernel_log_entry_return(const tcb_t *current)
{
    const char *safe_name = (current->name != NULL) ? current->name : "(null)";
    const char *state_name = kernel_task_state_to_string(current->state);
    const char *safe_state = (state_name != NULL) ? state_name : "UNKNOWN";

    hal_console_write("[entry] returned current: id=");
    kernel_write_int(current->id);
    hal_console_write(" name=");
    hal_console_write(safe_name);
    hal_console_write(" prio=");
    kernel_write_int(current->priority);
    hal_console_write(" state=");
    hal_console_write(safe_state);
    hal_console_write("\n");
}

/**
 * @brief cooperative verification iterationの開始をHAL consoleへ出力する。
 *
 * @details
 * 第4章4.3のboot-time verification modelで、scheduler再実行の境界を
 * QEMUシリアルログから追跡できるようにする。
 *
 * @param iteration 1から始まるiteration番号。
 * @return なし。
 * @note 表示専用であり、task状態やcurrent taskを変更しない。
 */
static void kernel_log_cooperative_iteration(unsigned long iteration)
{
    hal_console_write("[cooperative] iteration=");
    kernel_write_uint(iteration);
    hal_console_write(" begin\n");
}

/**
 * @brief cooperative return eventをHAL consoleへ出力する。
 *
 * @details
 * entry関数から通常のC関数returnで制御が戻ったことを、
 * 正式なtask終了ではなくcooperative return eventとして観測する。
 *
 * @param current returnしたentryを持つcurrent task。
 * @return なし。
 * @note DORMANT遷移、TASK_STATE_EXITED追加、task restartは行わない。
 */
static void kernel_log_cooperative_return(const tcb_t *current)
{
    const char *safe_name = (current->name != NULL) ? current->name : "(null)";
    const char *state_name = kernel_task_state_to_string(current->state);
    const char *safe_state = (state_name != NULL) ? state_name : "UNKNOWN";

    hal_console_write("[cooperative] returned current: id=");
    kernel_write_int(current->id);
    hal_console_write(" name=");
    hal_console_write(safe_name);
    hal_console_write(" prio=");
    kernel_write_int(current->priority);
    hal_console_write(" state=");
    hal_console_write(safe_state);
    hal_console_write("\n");
}

/**
 * @brief RUNNINGからREADYへの再候補化結果をHAL consoleへ出力する。
 *
 * @details
 * 第4章4.3ではRUNNING taskをcooperative return後にREADYへ戻し、
 * schedulerの再選択候補にする。このログはその結果を観測するだけで、
 * 正式なyield APIやtask restartを意味しない。
 *
 * @param current 再候補化対象のtask。
 * @param result task_mark_ready_from_running() の戻り値。
 * @return なし。
 */
static void kernel_log_ready_recandidacy(const tcb_t *current, int result)
{
    const char *safe_name = (current != NULL && current->name != NULL) ? current->name : "(null)";
    const char *state_name = (current != NULL) ? kernel_task_state_to_string(current->state) : "UNKNOWN";
    const char *safe_state = (state_name != NULL) ? state_name : "UNKNOWN";

    if (result != 0) {
        hal_console_write("[cooperative] ready again failed: err=");
        kernel_write_int(result);
    } else {
        hal_console_write("[cooperative] ready again: result=ok");
    }

    if (current == NULL) {
        hal_console_write(" current=none\n");
        return;
    }

    hal_console_write(" id=");
    kernel_write_int(current->id);
    hal_console_write(" name=");
    hal_console_write(safe_name);
    hal_console_write(" state=");
    hal_console_write(safe_state);
    hal_console_write("\n");
}

/**
 * @brief cooperative verification loopの停止理由をHAL consoleへ出力する。
 *
 * @details
 * 起動時検証が無限loopにならないことを観測できるよう、READYなし、
 * 上限到達、precondition失敗などの停止理由を明示する。
 *
 * @param reason 停止理由を表す静的文字列。
 * @return なし。
 */
static void kernel_log_cooperative_stop(const char *reason)
{
    const char *safe_reason = (reason != NULL) ? reason : "unknown";

    hal_console_write("[cooperative] stop: reason=");
    hal_console_write(safe_reason);
    hal_console_write("\n");
}

/**
 * @brief 第6章6.2のtimer foundation smokeを実行する。
 *
 * @details
 * `timer_init()` でsystem tickを0へ戻し、`timer_tick()` を明示的に複数回呼ぶ。
 * `timer_get_ticks()` で現在値を読み取り、tickがRTOS内部時間の最小単位として
 * 更新されたことをQEMU serial logから確認できるようにする。
 *
 * このhelperは将来のtimer interrupt handler接続を先取りしない。
 * tick増加後にscheduler選択、dispatcher commit、context switch、preemption、
 * time slice、delay/timeout wakeupは行わない。
 */
static void kernel_run_timer_smoke(void)
{
    unsigned long ticks;

    hal_console_write("[timer-smoke] begin\n");
    timer_init();

    /*
     * 第6章6.2では実ハードウェア割り込みではなく、明示呼び出しで時間経過を観測する。
     * 3 ticksだけ進め、将来の割り込み接続前にtimer module単体の振る舞いを確認する。
     */
    timer_tick();
    timer_tick();
    timer_tick();

    ticks = timer_get_ticks();
    hal_console_write("[timer-smoke] current tick=");
    kernel_write_uint(ticks);
    hal_console_write("\n");
    hal_console_write("[timer-smoke] end\n");
}

/**
 * @brief timer契機のpreemption判断を1回実行し、結果をログへ出力する。
 *
 * @details
 * このhelperは第6章6.3の境界を表す。先にtimer tickを進め、その後で
 * kernel smoke層がdispatcherから論理current taskを取得し、schedulerへ
 * 判断を依頼する。timer moduleはschedulingを知らず、schedulerはcurrent
 * 確定やcontext switchを行わない。
 *
 * @param label `[preempt]` serial出力に使うscenario名。
 */
static void kernel_run_preemption_decision_smoke(const char *label)
{
    const tcb_t *current;
    scheduler_preempt_decision_t decision;

    /*
     * timerはtickを進めるだけに留める。tick後の判断はkernel層で明示的に呼び、
     * timer moduleへscheduler責務を混ぜない。
     */
    timer_tick();
    current = dispatcher_get_current();
    decision = scheduler_select_preemption_candidate(current);
    kernel_log_preemption_decision(label, decision);
}

/**
 * @brief boot中にtimer契機preemption判断を検証する。
 *
 * @details
 * これは完全な割り込み駆動preemption経路ではない。このhelperでは、
 * scheduler判断の比較基準を作るためだけに `dispatcher_commit_current()` を使う。
 * 高優先度READY taskが見つかった場合も、候補をログに残すだけで、意図的に
 * dispatchやcontext switchは行わない。
 *
 * @param low_task_id 最初のcurrent基準として使う低優先度task。
 * @param high_task_id 次のcurrent基準として使う高優先度task。
 */
static void kernel_run_preemption_smoke(int low_task_id, int high_task_id)
{
    const tcb_t *low_task;
    const tcb_t *high_task;
    int result;

    hal_console_write("[preempt-smoke] begin\n");
    /*
     * current未確定でも判断処理が破綻しないことを先に観測する。
     * これはtimer契機が来ても比較基準がない場合の非発生ケースである。
     */
    kernel_run_preemption_decision_smoke("no-current");

    /*
     * 低優先度task_aを論理currentにして、高優先度READY task_bを候補にできる
     * ケースを作る。ここでは候補検出だけを確認し、task_bへは切り替えない。
     */
    low_task = task_get_by_id(low_task_id);
    result = dispatcher_commit_current(low_task);
    kernel_log_dispatcher_commit_result(result);
    if (result == DISPATCHER_OK) {
        kernel_run_preemption_decision_smoke("higher-ready");

        /*
         * 後続の既存smokeを壊さないよう、比較基準にしたtaskをREADYへ戻す。
         * これは検証後片付けであり、preemption実行ではない。
         */
        result = task_mark_ready_from_running(low_task_id);
        hal_console_write("[preempt-smoke] restore current result=");
        kernel_write_int(result);
        hal_console_write(" task_id=");
        kernel_write_int(low_task_id);
        hal_console_write("\n");
    }

    /*
     * 高優先度task_bを論理currentにすると、同priorityのtask_cはtime slice対象
     * ではないため切り替え候補にしない。この仕様の非発生ケースを確認する。
     */
    high_task = task_get_by_id(high_task_id);
    result = dispatcher_commit_current(high_task);
    kernel_log_dispatcher_commit_result(result);
    if (result == DISPATCHER_OK) {
        kernel_run_preemption_decision_smoke("no-higher-ready");

        /*
         * 検証でRUNNINGにしたtaskをREADYへ戻し、semaphore/context/cooperative
         * smokeが従来どおりREADY taskを選べる状態へ戻す。
         */
        result = task_mark_ready_from_running(high_task_id);
        hal_console_write("[preempt-smoke] restore current result=");
        kernel_write_int(result);
        hal_console_write(" task_id=");
        kernel_write_int(high_task_id);
        hal_console_write("\n");
    }

    hal_console_write("[preempt-smoke] end\n");
}

/**
 * @brief RUNNING current taskからの `yield_tsk()` を10.4の協調switch観測として実行する。
 *
 * @details
 * 指定されたREADY taskをdispatcherでcurrent/RUNNINGへcommitした直後に
 * `yield_tsk()` を呼び、RUNNING->READY遷移、READY化後のscheduler候補選択、
 * dispatcher境界、task_context task-to-task switchへ進むことを観測する。
 * 起動時smokeでは低優先度taskをyield対象にし、高優先度READY taskが残っている状態で
 * scheduler候補選択を観測する。
 *
 * @param selected RUNNINGへcommitしてからyield対象にするREADY task。
 * @return 成功時は0、失敗時は負の値。
 */
static int kernel_run_yield_running_smoke(const tcb_t *selected)
{
    int commit_result;
    int yield_result;

    hal_console_write("[yield-smoke] begin\n");

    if (selected == NULL) {
        /*
         * yield検証はREADY taskをいったんcurrentへcommitする前提で行う。
         * 対象がなければ検証経路を作らず、schedulerを再実行して補わない。
         */
        hal_console_write("[yield-smoke] stop: reason=no-selected-task\n");
        return DISPATCHER_ERR_INVAL;
    }

    /*
     * yield_tsk()自身はcurrentを選ばないため、smoke側で既存dispatcher境界を使って
     * RUNNING currentを明示的に用意する。これは検証準備であり、yield内の責務ではない。
     */
    commit_result = dispatcher_commit_current(selected);
    kernel_log_dispatcher_commit_result(commit_result);
    if (commit_result != DISPATCHER_OK) {
        hal_console_write("[yield-smoke] stop: reason=commit-failed\n");
        return commit_result;
    }

    /*
     * 10.4ではyield_tsk()が協調API経由でdispatcher/context switchへ接続される。
     * timer IRQやdispatch pending経由ではなく、通常のAPI smokeとしてだけ実行する。
     */
    yield_result = yield_tsk();
    if (yield_result != YIELD_TSK_OK) {
        hal_console_write("[yield-smoke] stop: reason=yield-failed err=");
        kernel_write_int(yield_result);
        hal_console_write("\n");
        return yield_result;
    }

    /*
     * 成功後はtask_context smokeまで進むため、entry returnしたtaskは9.4どおり
     * DORMANTへ最終化される。これはyieldをentry return扱いする処理ではない。
     */
    hal_console_write("[yield-smoke] end\n");
    return 0;
}

/**
 * @brief 第9章9.1のtask間context switch smokeを1回だけ実行する。
 *
 * @details
 * schedulerはREADY task選択だけ、dispatcherはcurrent commitだけを担当する。
 * このhelperはcommit済みtaskと次taskのcontextを初回stack frameとして準備し、
 * boot contextからfirst taskへ入り、first taskのentry return観測点から
 * second task contextへ一度だけswitchする。second taskのreturn後はbootへ戻る。
 *
 * これはtimer interrupt、preemption、割り込みハンドラ連携、yield API、
 * 本格task lifecycleを導入しないためのboot-time verification modelである。
 *
 * @param selected schedulerが選択したREADY task。
 * @param next_selected task間切替先として使うREADY task。
 * @return 成功時は0、失敗時は負の値。
 */
static int kernel_run_minimal_context_switch_smoke(const tcb_t *selected, const tcb_t *next_selected)
{
    tcb_t *current_task;
    tcb_t *next_task;
    int commit_result;
    int switch_result;

    hal_console_write("[context-smoke] begin\n");

    commit_result = dispatcher_commit_current(selected);
    kernel_log_dispatcher_commit_result(commit_result);
    if (commit_result != DISPATCHER_OK) {
        hal_console_write("[context-smoke] stop: reason=commit-failed\n");
        return commit_result;
    }

    current_task = task_get_mutable_by_id(selected->id);
    if (current_task == NULL) {
        hal_console_write("[context-smoke] stop: reason=current-not-found\n");
        return TASK_CONTEXT_ERR_INVAL;
    }

    if (next_selected == NULL || next_selected->id == selected->id) {
        hal_console_write("[context-smoke] stop: reason=next-not-found\n");
        return TASK_CONTEXT_ERR_INVAL;
    }

    next_task = task_get_mutable_by_id(next_selected->id);
    if (next_task == NULL) {
        hal_console_write("[context-smoke] stop: reason=next-not-found\n");
        return TASK_CONTEXT_ERR_INVAL;
    }

    /*
     * 第9章9.2では、切替開始点をtask_context smoke補助APIへ直接置かず、
     * dispatcherのswitch boundary経由に寄せる。ここでは次taskの選び直し、
     * dispatch pending消費、interrupt exit接続、正式な状態遷移完成は行わない。
     */
    switch_result = dispatcher_switch_to(current_task, next_task);
    if (switch_result != TASK_CONTEXT_OK) {
        hal_console_write("[context-smoke] stop: reason=switch-failed\n");
        return switch_result;
    }

    hal_console_write("[context-smoke] end\n");
    return 0;
}

/**
 * @brief entryを呼ばない理由をHAL consoleへ出力する。
 *
 * @details
 * current未設定、RUNNING以外、entry未設定のいずれかを検出した場合、
 * 不正なTCBを実行対象にせず、skip理由だけを観測可能にする。
 *
 * @param reason skip理由を表す静的文字列。
 * @param current 判定対象のcurrent task。NULLも許容する。
 * @return なし。
 * @note skip時もTCB状態やcurrent taskを変更しない。
 */
static void kernel_log_entry_skip(const char *reason, const tcb_t *current)
{
    const char *safe_reason = (reason != NULL) ? reason : "unknown";
    const char *safe_name;
    const char *state_name;
    const char *safe_state;

    hal_console_write("[entry] skipped: reason=");
    hal_console_write(safe_reason);

    if (current == NULL) {
        hal_console_write(" current=none\n");
        return;
    }

    safe_name = (current->name != NULL) ? current->name : "(null)";
    state_name = kernel_task_state_to_string(current->state);
    safe_state = (state_name != NULL) ? state_name : "UNKNOWN";

    hal_console_write(" id=");
    kernel_write_int(current->id);
    hal_console_write(" name=");
    hal_console_write(safe_name);
    hal_console_write(" prio=");
    kernel_write_int(current->priority);
    hal_console_write(" state=");
    hal_console_write(safe_state);
    hal_console_write("\n");
}

/**
 * @brief current taskのentryを4.3の協調実行検証モデルとして有限回直接呼び出す。
 *
 * @details
 * schedulerでREADY taskを選択し、dispatcherでcurrentとしてcommitした後、
 * `dispatcher_get_current()` で取得したcurrent taskだけをentry呼び出し対象にする。
 * `current != NULL`、`current->state == TASK_STATE_RUNNING`、
 * `current->entry != NULL` を満たす場合だけ、`current->entry()` を通常のC関数呼び出し
 * として1回実行する。
 *
 * entryがreturnした場合、そのreturnは正式なtask終了ではなくcooperative return event
 * として観測する。その後、RUNNING taskをREADYへ戻し、再びscheduler候補にする。
 * このREADY再候補化はtask restartではなく、`yield_tsk`互換APIでもない。
 *
 * このloopは一時的なboot-time verification modelである。
 * RUNNINGはcurrentとして採用済みでentry呼び出し対象になったことを示すが、
 * CPUで継続実行中、独立stack上で実行中、CPU context復元済みであることは意味しない。
 *
 * dispatcherでentryを呼ばないのは、dispatcherをcurrent commitだけの境界に保つためである。
 * schedulerの責務を変更しないのは、READY task選択だけに限定するためである。
 * task_runner専用層を導入しないのは、4.3では第5章前の検証用loopであり、
 * 新規public APIやMakefile変更を伴う抽象化がまだ不要だからである。
 *
 * @param なし。
 * @return なし。
 * @note コンテキストスイッチ、スタック切り替え、レジスタ保存・復元、
 * 割り込み、タイマ、プリエンプションは行わない。
 */
static void kernel_run_cooperative_entries(void)
{
    unsigned long iteration;

    for (iteration = 1; iteration <= KERNEL_COOPERATIVE_ENTRY_LIMIT; iteration++) {
        const tcb_t *selected;
        const tcb_t *current;
        int commit_result;
        int ready_result;

        kernel_log_cooperative_iteration(iteration);

        selected = scheduler_select_next();
        kernel_log_scheduler_selection("cooperative", selected);
        if (selected == NULL) {
            kernel_log_cooperative_stop("no-ready");
            return;
        }

        commit_result = dispatcher_commit_current(selected);
        kernel_log_dispatcher_commit_result(commit_result);
        if (commit_result != DISPATCHER_OK) {
            kernel_log_cooperative_stop("commit-failed");
            return;
        }

        current = dispatcher_get_current();
        if (current == NULL) {
            kernel_log_entry_skip("current-null", current);
            kernel_log_cooperative_stop("current-null");
            return;
        }

        if (current->state != TASK_STATE_RUNNING) {
            kernel_log_entry_skip("current-not-running", current);
            kernel_log_cooperative_stop("current-not-running");
            return;
        }

        if (current->entry == NULL) {
            kernel_log_entry_skip("entry-null", current);
            kernel_log_cooperative_stop("entry-null");
            return;
        }

        kernel_log_entry_call(current);
        current->entry();
        /*
         * ここに制御が戻ったことだけをcooperative return eventとして観測する。
         * これは正式なtask終了ではなく、DORMANT遷移やtask restartも行わない。
         */
        kernel_log_entry_return(current);
        kernel_log_cooperative_return(current);

        ready_result = task_mark_ready_from_running(current->id);
        kernel_log_ready_recandidacy(current, ready_result);
        if (ready_result != 0) {
            kernel_log_cooperative_stop("ready-recandidate-failed");
            return;
        }
    }

    kernel_log_cooperative_stop("limit-reached");
}

/**
 * @brief 第12章12.4のsemaphore wakeup preemption smoke sequenceを実行する。
 *
 * @details
 * 静的セマフォを作成し、RUNNING current taskから `wai_sem()` を2回呼ぶ。
 * 1回目はcountを取得してswitchせず、2回目はcount不足によりcurrentをWAITINGへ
 * 落としてFIFO wait queueへenqueueする。その後、低優先度のtask文脈をcurrent
 * RUNNINGとして確定し、`sig_sem()` で高優先度taskをREADYへ戻した直後に
 * `dispatcher_switch_to()` へ進むことを観測する。
 *
 * この検証はtask文脈APIのsmokeであり、timer IRQ handlerやdispatch pending経路から
 * `sig_sem()` を呼ばない。priority順wait queue、timeout、同一優先度time slice、
 * round-robin、完全な割り込み復帰フレーム切替も扱わない。
 *
 * @param current_task_id `wai_sem()` を呼ぶRUNNING current taskのid。
 */
static void kernel_run_semaphore_smoke(int current_task_id)
{
    int sem_a_id;
    int signaler_task_id;

    hal_console_write("[sem-smoke] begin\n");
    /*
     * セマフォtableだけを初期化する。task tableやscheduler状態には触れず、
     * 「同期機構の台帳を起動時に用意する」観測点として分離する。
     */
    (void)sem_init();

    /*
     * count=1/max=1の単純なセマフォに固定することで、
     * 1回目のwai_semは成功、2回目のwai_semはWAITINGという流れを確実に作る。
     */
    sem_a_id = sem_create("sem_a", 1, 1);
    if (sem_a_id < 0) {
        hal_console_write("[sem-smoke] stop: reason=create-failed\n");
        return;
    }

    /*
     * task文脈APIとしてcurrentを確定してから呼ぶ。1回目はcountを消費し、
     * 2回目は同じRUNNING current taskをWAITINGへ落として次READY taskを選ぶ。
     */
    if (dispatcher_commit_current(task_get_by_id(current_task_id)) != DISPATCHER_OK) {
        hal_console_write("[sem-smoke] stop: reason=current-commit-failed\n");
        return;
    }
    (void)wai_sem(sem_a_id);
    (void)wai_sem(sem_a_id);

    /*
     * 12.4では、WAITINGへ落としたtaskより低優先度のtask文脈をsig_sem()直前に
     * current RUNNINGとして確定する。これにより、wakeup後にREADYへ戻った高優先度task
     * だけがdispatcher switch対象になることを、timer IRQ経路と混ぜずに観測する。
     */
    signaler_task_id = task_register(
        "task_wai_sem_to",
        task_wai_sem_to,
        5,
        task_wai_sem_to_stack,
        sizeof(task_wai_sem_to_stack)
    );
    kernel_log_task_register_result("task_wai_sem_to", signaler_task_id);
    if (signaler_task_id <= 0 ||
        dispatcher_commit_current(task_get_by_id(signaler_task_id)) != DISPATCHER_OK) {
        hal_console_write("[sem-smoke] stop: reason=signaler-current-commit-failed\n");
        return;
    }

    (void)sig_sem(sem_a_id);
    (void)sig_sem(sem_a_id);

    /*
     * task dumpとsemaphore dumpを続けて出すことで、READY復帰、wait_sem_idのclear、
     * wakeup時のcount非増加、待ちtaskなしcount-upを同じQEMUログで確認できる。
     */
    task_dump();
    sem_dump();
    hal_console_write("[sem-smoke] end\n");
}

/**
 * @brief 第13章13.1のdelay task API smoke sequenceを実行する。
 *
 * @details
 * `dly_tsk(0)` のinvalid-delay経路と、`dly_tsk(10)` によるRUNNING current taskの
 * delay WAITING化、schedulerによる次READY選択、既存dispatcher switch境界への接続を
 * 起動時ログで観測する。
 *
 * この検証はtask文脈APIの入口だけを扱う。tick到達時READY復帰は13.4の専用smokeで確認する。
 * timer IRQ handlerからの `dly_tsk()` 呼び出しは行わない。
 *
 * @param current_task_id `dly_tsk()` を呼ぶRUNNING current taskのid。
 */
static void kernel_run_delay_smoke(int current_task_id)
{
    hal_console_write("[dly-smoke] begin\n");

    if (dispatcher_commit_current(task_get_by_id(current_task_id)) != DISPATCHER_OK) {
        hal_console_write("[dly-smoke] stop: reason=current-commit-failed\n");
        return;
    }

    /*
     * 0 tickはyield相当やno-opにせず、13.1の仕様として入力エラーにする。
     * この呼び出しではRUNNING current taskの状態を維持し、続く10 tick検証へ進む。
     */
    (void)dly_tsk(0U);

    /*
     * RUNNING current taskをdelay WAITINGへ落とし、既存schedulerが次READY taskだけを
     * 選ぶことを確認する。delay満了によるREADY復帰は後続の13.4 smokeで確認する。
     */
    (void)dly_tsk(10U);

    task_dump();
    hal_console_write("[dly-smoke] end\n");
}

/**
 * @brief 第13章13.3のtimeout付きsemaphore待ち smoke sequenceを実行する。
 *
 * @details
 * `twai_sem(sem_id, 0)` のinvalid timeout、countが残る場合の即時取得、
 * countが0の場合のtimeout付きsemaphore WAITING化を順に観測する。
 * timeout待ちに入ったtaskはsemaphore wait queueとdelay queueの両方へ登録される。
 *
 * この検証はtask文脈APIのsmokeであり、timer IRQ handlerから `twai_sem()` を呼ばない。
 * timeout到達時READY復帰とtimeout時のsemaphore wait queue削除は後続の13.4 smokeで確認する。
 * `sig_sem()` 成功時のdelay queue削除はまだ行わない。
 *
 * @param current_task_id `twai_sem()` を呼ぶRUNNING current taskのid。
 */
static void kernel_run_twai_sem_smoke(int current_task_id)
{
    int sem_twai_id;

    hal_console_write("[twai-smoke] begin\n");

    /*
     * 13.3の検証は専用semaphoreで行う。semaphore tableだけを初期化し、
     * delay queueは13.2の観測entryを残したまま、両queue登録を確認する。
     */
    (void)sem_init();
    sem_twai_id = sem_create("sem_twai", 1, 1);
    if (sem_twai_id < 0) {
        hal_console_write("[twai-smoke] stop: reason=create-failed\n");
        return;
    }

    if (dispatcher_commit_current(task_get_by_id(current_task_id)) != DISPATCHER_OK) {
        hal_console_write("[twai-smoke] stop: reason=current-commit-failed\n");
        return;
    }

    /*
     * 0 timeoutはpollではなくinvalid timeoutとして扱う。
     * current taskをRUNNINGのまま保ち、後続の即時取得とtimeout待ち検証へ進む。
     */
    (void)twai_sem(sem_twai_id, 0U);

    /*
     * countが残る場合はtimeout待ちに入らず、即時取得として完了する。
     */
    (void)twai_sem(sem_twai_id, 10U);

    /*
     * countが0になった同じsemaphoreで、timeout付きsemaphore待ちへ入る。
     * この時点でcurrent taskはsemaphore wait queueとdelay queueの両方に登録される。
     */
    (void)twai_sem(sem_twai_id, 10U);

    task_dump();
    sem_dump();
    hal_console_write("[twai-smoke] end\n");
}

/**
 * @brief 第13章13.4のtick到達READY復帰 smoke sequenceを実行する。
 *
 * @details
 * `dly_tsk(1)` と `twai_sem(sem, 1)` でdelay queueへ登録したtaskを、
 * 明示的な `timer_tick()` でtimeout到達させる。timeout付きsemaphore待ちtaskは
 * semaphore wait queueからも削除される。READY復帰後は既存のIRQ preemption境界を
 * 明示的に呼び、timer IRQ handler本体から直接dispatcherへ進まない設計を観測する。
 *
 * @param delay_task_id delay timeout復帰を確認するtask ID。
 * @param twai_task_id timeout付きsemaphore復帰を確認するtask ID。
 * @param current_task_id timeout後preemption比較のcurrentになる低優先度task ID。
 */
static void kernel_run_tick_ready_wakeup_smoke(
    int delay_task_id,
    int twai_task_id,
    int current_task_id
)
{
    int sem_tick_id;
    const char *not_requested_reason;

    hal_console_write("[tick-ready-smoke] begin\n");

    sem_tick_id = sem_create("sem_tick_timeout", 0, 1);
    if (sem_tick_id < 0) {
        hal_console_write("[tick-ready-smoke] stop: reason=create-failed\n");
        return;
    }

    if (dispatcher_commit_current(task_get_by_id(delay_task_id)) != DISPATCHER_OK) {
        hal_console_write("[tick-ready-smoke] stop: reason=delay-current-commit-failed\n");
        return;
    }

    /*
     * 13.4 smokeではtick処理そのものを観測するため、task/queue helperで1 tickの
     * delay待ち状態を作る。dly_tsk()のtask文脈API経路は直前の13.1/13.2 smokeで確認済みである。
     */
    if (task_mark_waiting_on_delay(delay_task_id, 1U) != 0 ||
        delay_queue_enqueue(delay_task_id, 1U) != DELAY_QUEUE_OK) {
        hal_console_write("[tick-ready-smoke] stop: reason=delay-queue-setup-failed\n");
        return;
    }

    if (dispatcher_commit_current(task_get_by_id(twai_task_id)) != DISPATCHER_OK) {
        hal_console_write("[tick-ready-smoke] stop: reason=twai-current-commit-failed\n");
        return;
    }

    /*
     * timeout付きsemaphore待ちtaskも1 tickで満了するよう両queueへ登録する。
     * twai_sem()のAPI経路は直前の13.3 smokeで確認済みであり、ここではtimeout到達後の
     * semaphore wait queue削除とREADY復帰を明示的に観測する。
     */
    if (task_mark_waiting_on_sem_timeout(twai_task_id, sem_tick_id, 1U) != 0 ||
        sem_enqueue_waiter(sem_tick_id, twai_task_id) != SEM_OK ||
        delay_queue_enqueue(twai_task_id, 1U) != DELAY_QUEUE_OK) {
        hal_console_write("[tick-ready-smoke] stop: reason=twai-queue-setup-failed\n");
        return;
    }

    if (dispatcher_commit_current(task_get_by_id(current_task_id)) != DISPATCHER_OK) {
        hal_console_write("[tick-ready-smoke] stop: reason=low-current-commit-failed\n");
        return;
    }

    /*
     * 1 tick進めることで2つの13.4対象entryをtimeout到達させる。
     * READY復帰後のpreemption pendingは既存IRQ preemption境界で観測する。
     */
    timer_tick();
    not_requested_reason = preemption_evaluate_from_irq();
    dispatch_pending_log_state_from_irq(not_requested_reason);

    delay_queue_dump();
    task_dump();
    sem_dump();
    hal_console_write("[tick-ready-smoke] end\n");
}

/**
 * @brief サンプルタスクAのentry関数。
 *
 * @details
 * 第3章3.1ではentry関数を登録するだけで呼び出さない。
 * 第4章4.1ではcurrentとしてcommitされた場合だけ、boot-time verification modelとして
 * 通常のC関数呼び出しで実行される。
 *
 * @param なし。
 * @return なし。
 * @note 独立stack実行やCPU context復元を伴うものではない。
 */
static void task_a(void)
{
    /*
     * 第4章4.1では、このログがcurrent entryの直接呼び出しを確認する観測点になる。
     * 独立stackや復元済みCPU context上での実行ではなく、通常のC関数呼び出しである。
     */
    hal_console_write("[task_a] executed\n");
}

/**
 * @brief サンプルタスクBのentry関数。
 *
 * @details
 * 第3章3.1ではentry関数を登録するだけで呼び出さない。
 * 第4章4.1では、優先度選択とcurrent commit後にboot-time verification modelとして
 * 通常のC関数呼び出しで1回だけ実行対象になる。
 *
 * @param なし。
 * @return なし。
 * @note コンテキストスイッチ、スタック切り替え、レジスタ保存・復元は伴わない。
 */
static void task_b(void)
{
    /*
     * 4.1ではこのログが、current taskのentryが通常のC関数として
     * 1回だけ直接呼ばれたことを示す。
     */
    hal_console_write("[task_b] executed\n");
}

/**
 * @brief サンプルタスクCのentry関数。
 *
 * @details
 * 第3章3.2の同一priority確認用に登録するだけの関数である。
 * schedulerはTCBを選択するだけで、このentryを呼び出さない。
 * 第4章4.1でも、schedulerが選択しdispatcherがcommitしたcurrentだけが
 * kernel.cのboot-time verification helperから直接呼ばれる。
 *
 * @param なし。
 * @return なし。
 * @note このログが出た場合は、第3章3.2の「選択のみ」制約に反している。
 */
static void task_c(void)
{
    hal_console_write("[task_c] executed\n");
}

/**
 * @brief 10.4専用の協調yield switch元task entry。
 *
 * @details
 * 既存のtask_b -> task_c context switch smokeを壊さないよう、10.4検証では
 * 追加登録したtaskを使う。entry return後は9.4のtask_context層によりDORMANTへ
 * 最終化される。timer IRQ、dispatch pending、preemptionとは接続しない。
 *
 * @param なし。
 * @return なし。
 */
static void task_yield_from(void)
{
    hal_console_write("[task_yield_from] executed\n");
}

/**
 * @brief 10.4専用の協調yield switch先task entry。
 *
 * @details
 * `yield_tsk()` からdispatcher境界を通って到達するREADY taskである。
 * このentryもreturn後はDORMANTへ最終化され、task restartや継続実行は行わない。
 *
 * @param なし。
 * @return なし。
 */
static void task_yield_to(void)
{
    hal_console_write("[task_yield_to] executed\n");
}

/**
 * @brief 12.1 wai_sem smokeでセマフォ待ちへ入る側のtask entry。
 *
 * @details
 * このentryは登録とcontext switch smokeの到達確認だけに使う。`wai_sem()` の呼び出しは
 * task文脈APIの観測としてkernel smoke側から行う。12.3ではWAITING化後に
 * semaphoreごとのFIFO wait queueへ登録するが、timeout、priority順、time slice、
 * round-robinは開始しない。
 *
 * @param なし。
 * @return なし。
 */
static void task_wai_sem_from(void)
{
    hal_console_write("[task_wai_sem_from] executed\n");
}

/**
 * @brief 12.1 wai_sem smokeでWAITING後に選ばれるREADY task entry。
 *
 * @details
 * `wai_sem()` がRUNNING current taskをWAITINGへ落とした後、schedulerが選ぶ
 * READY候補として使う。これはboot-time verification modelであり、完全な
 * semaphore wakeupや割り込み復帰フレーム切替ではない。
 *
 * @param なし。
 * @return なし。
 */
static void task_wai_sem_to(void)
{
    hal_console_write("[task_wai_sem_to] executed\n");
}

/**
 * @brief 13.1 dly_tsk smokeでdelay WAITINGへ入る側のtask entry。
 *
 * @details
 * このentryは登録とcontext switch smokeの到達確認用であり、entry本体から
 * `dly_tsk()` を呼ばない。13.1ではkernel smoke側でtask文脈API呼び出しを観測し、
 * timer IRQ handlerやentry内自動delay処理とは接続しない。
 */
static void task_dly_from(void)
{
    hal_console_write("[task_dly_from] executed\n");
}

/**
 * @brief 13.1 dly_tsk smokeでdelay WAITING後に選ばれるREADY task entry。
 *
 * @details
 * `dly_tsk()` がRUNNING current taskをWAITINGへ落とした後、既存schedulerが選ぶ
 * READY taskとして使う。delay満了復帰やround-robinはまだ扱わない。
 */
static void task_dly_to(void)
{
    hal_console_write("[task_dly_to] executed\n");
}

/**
 * @brief 13.3 twai_sem smokeでtimeout付きsemaphore WAITINGへ入る側のtask entry。
 *
 * @details
 * このentryは登録とcontext switch smokeの到達確認用であり、entry本体から
 * `twai_sem()` を呼ばない。13.3ではkernel smoke側でtask文脈API呼び出しを観測し、
 * timer IRQ handlerやentry内自動timeout処理とは接続しない。
 */
static void task_twai_from(void)
{
    hal_console_write("[task_twai_from] executed\n");
}

/**
 * @brief 13.3 twai_sem smokeでtimeout付きsemaphore WAITING後に選ばれるREADY task entry。
 *
 * @details
 * `twai_sem()` がRUNNING current taskをWAITINGへ落とした後、既存schedulerが選ぶ
 * READY taskとして使う。timeout満了復帰やround-robinはまだ扱わない。
 */
static void task_twai_to(void)
{
    hal_console_write("[task_twai_to] executed\n");
}

/**
 * @brief 13.4 tick READY復帰smoke用のdelay待ちtask entry。
 *
 * @details
 * entry本体から `dly_tsk()` は呼ばない。kernel smoke側でtask文脈APIを呼び、
 * timer tick到達時のREADY復帰を観測する。
 */
static void task_tick_delay(void)
{
    hal_console_write("[task_tick_delay] executed\n");
}

/**
 * @brief 13.4 tick READY復帰smoke用のtimeout付きsemaphore待ちtask entry。
 *
 * @details
 * entry本体から `twai_sem()` は呼ばない。kernel smoke側でtask文脈APIを呼び、
 * timeout時のsemaphore wait queue削除を観測する。
 */
static void task_tick_twai(void)
{
    hal_console_write("[task_tick_twai] executed\n");
}

/**
 * @brief 13.4 tick READY復帰smoke用の低優先度current task entry。
 *
 * @details
 * READY復帰した高優先度taskとのpreemption比較対象にするための観測用entryである。
 * このentry本体は呼び出さない。
 */
static void task_tick_current(void)
{
    hal_console_write("[task_tick_current] executed\n");
}

/**
 * @brief kernelのメインエントリポイント。
 *
 * @details
 * HAL consoleを初期化し、起動ログと初期タスク管理の確認ログを出力する。
 * タスク登録後は `task_dump()` で一覧を表示し、簡易schedulerの選択結果を表示する。
 * 第4章4.3では、kernel.cのboot-time cooperative runnerがREADY taskを選び、
 * dispatcherでcurrentとしてcommitし、current task entryを通常のC関数呼び出しで
 * 直接呼ぶ。entry returnは正式終了ではなくcooperative return eventとして観測し、
 * RUNNING taskをREADYへ戻して再びscheduler候補にする。
 * 第6章6.2では、system tickをtimer interruptなしで明示的に進める
 * timer foundation smokeを追加する。
 *
 * @param なし。
 * @return なし。
 * @note task_start、割り込み駆動のタイマ、プリエンプション、time sliceは追加しない。
 */
void kernel_main(void)
{
    int task_a_id;
    int task_b_id;
    int task_c_id;
    int task_yield_from_id;
    int task_yield_to_id;
    int task_wai_sem_from_id;
    int task_dly_from_id;
    int task_dly_to_id;
    int task_twai_from_id;
    int task_twai_to_id;
    int task_tick_delay_id;
    int task_tick_twai_id;
    int task_tick_current_id;
    const tcb_t *selected_task;
    const tcb_t *context_next_task;

    hal_console_init();
    hal_console_write("itron-rtos booting...\n");
    hal_console_write("kernel_main reached\n");

    /*
     * 第7章7.1ではCPU例外を受け取るための土台だけを初期化する。
     * ここでIDTをloadしてもhardware interruptは有効化せず、例外処理を
     * scheduling、dispatching、context switchingへ接続しない。
     */
    if (hal_interrupt_init() != 0) {
        hal_console_write("[kernel] interrupt init failed\n");
        for (;;) {
            __asm__ volatile ("hlt");
        }
    }

    /*
     * 第7章7.2では割り込みコントローラの routing preparation として legacy PIC を
     * 初期化する。IRQ0 は vector 32 以降へ移すが、全 IRQ は mask したままにし、
     * timer ISR、scheduler、dispatcher、context switch へはまだ接続しない。
     */
    if (hal_interrupt_controller_init() != 0) {
        hal_console_write("[kernel] interrupt controller init failed\n");
        for (;;) {
            __asm__ volatile ("hlt");
        }
    }

#ifdef ARCH_INTERRUPT_VALIDATE_EXCEPTION
    /*
     * 明示的な検証buildでは、例外handler到達ログを出した後に停止する。
     * 通常のsmokeでは無効にしておき、IDT初期化後も既存の第6章6.3 flowを継続する。
     */
    hal_interrupt_trigger_validation_exception();
#endif

    /* タスク管理台帳だけを初期化し、スケジューラや実行コンテキストは作らない。 */
    task_init();
    /*
     * 13.2のsleep/delay queueを空状態へ初期化する。
     * 13.4以降はtimer tickによりremaining tickが減算され、timeout到達taskがREADYへ戻る。
     */
    delay_queue_init();
    scheduler_init();
    dispatcher_init();

    /*
     * 第6章6.2では、timer interruptではなく明示呼び出しによるtimer foundationを
     * 先に観測する。既存のtask登録、semaphore smoke、context smokeの相対順序は
     * 変えず、tick増加からscheduler/dispatcher/context switchも呼び出さない。
     */
    kernel_run_timer_smoke();

    /*
     * READYタスクがまだ存在しない段階での選択結果を確認する。
     * NULLはエラーではなく、選択対象なしを表す通常の結果である。
     * この時点でcurrentへcommitしないことで、選択と確定を別の段階として観測できる。
     */
    selected_task = scheduler_select_next();
    kernel_log_scheduler_selection("before_register", selected_task);

    /*
     * stack領域はTCBへ保持するだけで初期化しない。
     * entry関数も登録対象として渡すだけで、この時点では呼び出されない。
     */
    task_a_id = task_register(
        "task_a",
        task_a,
        5,
        task_a_stack,
        sizeof(task_a_stack)
    );
    kernel_log_task_register_result("task_a", task_a_id);

    /*
     * task_bとtask_cは同一priorityにし、同一priorityでは先に登録されたtask_bが
     * 選ばれることを確認する。task_aはより大きいpriority値なので選ばれない。
     */
    task_b_id = task_register(
        "task_b",
        task_b,
        1,
        task_b_stack,
        sizeof(task_b_stack)
    );
    kernel_log_task_register_result("task_b", task_b_id);

    task_c_id = task_register(
        "task_c",
        task_c,
        1,
        task_c_stack,
        sizeof(task_c_stack)
    );
    kernel_log_task_register_result("task_c", task_c_id);

    /* 登録済みTCBだけを表示し、UNUSEDスロットは表示されないことを確認する。 */
    task_dump();

#ifdef ARCH_TIMER_IRQ_ENTRY_VALIDATE
    {
        const char *not_requested_reason;

        /*
         * 11.3の同一優先度READY除外をvalidation buildで観測する。
         * task_bをRUNNING currentにするとtask_cは同一priority READYなので、
         * dispatch pendingはrequestされない。これはtimer IRQからの実切替ではなく、
         * IRQを開く前の限定的な境界確認である。
         */
        kernel_log_dispatcher_commit_result(dispatcher_commit_current(task_get_by_id(task_b_id)));
        not_requested_reason = preemption_evaluate_from_irq();
        dispatch_pending_log_state_from_irq(not_requested_reason);
        /*
         * 11.3のno-pending側もvalidation証跡として残す。ここではIRQ0をまだ開かず、
         * 同一優先度READYがtime slice対象外でpendingを作らないことと、後段境界が
         * no-dispatchで終わることだけを観測する。timer IRQ handler本体から
         * dispatcher_switch_to()やyield_tsk()を呼ぶ経路ではない。
         */
        hal_console_write("[timer-irq] exit boundary: dispatch-pending=none action=no-dispatch\n");
        (void)dispatch_pending_consume_at_deferred_boundary();
        (void)task_mark_ready_from_running(task_b_id);
    }

    /*
     * 第11章11.1の明示validation buildでは、timer IRQを開く前にpreemption判定用の
     * 観測状態を作る。task_aをRUNNING current、task_b/task_cをREADYのまま残し、
     * IRQ handler側では高優先度READY検出とdispatch pending requestだけを確認する。
     * ここで実dispatch、pending消費、preemptive context switchへは接続しない。
     */
    kernel_log_dispatcher_commit_result(dispatcher_commit_current(task_get_by_id(task_a_id)));
    hal_interrupt_enable_timer_entry_validation();
    for (;;) {
        __asm__ volatile ("hlt");
    }
#endif

    /*
     * 第6章6.3では、既存のsemaphore/context switch smokeへ進む前に、
     * timer契機のpreemption判断だけを観測する。dispatcherで論理current基準を
     * 作り、schedulerへ候補判断を依頼した後、taskをREADYへ戻すことで
     * 後続の検証フローの前提を保つ。
     */
    kernel_run_preemption_smoke(task_a_id, task_b_id);

    /*
     * 第3章3.2では「どのタスクが次に実行対象になるか」を選ぶだけで、
     * 選択されたentry関数を呼び出したりRUNNINGへ遷移させたりしない。
     */
    selected_task = scheduler_select_next();
    kernel_log_scheduler_selection("after_register", selected_task);

    /*
     * 第9章9.1では、既存のboot-to-task smokeをtask-to-taskへ拡張する。
     * selected taskでentry returnを観測した後、もう1つのREADY task contextへ
     * 一度だけ切り替え、second taskのreturn後にbootへ戻る。これは起動時smokeであり、
     * IRQ exit、dispatch pending、yield API、preemptionとは接続しない。
     */
    context_next_task = task_get_by_id(task_c_id);
    if (selected_task != NULL && context_next_task != NULL &&
        context_next_task->id == selected_task->id) {
        /*
         * 現在の優先度設定ではselected_taskはtask_b、切替先はtask_cになる。
         * 将来priorityや登録順を変えた場合でも、同じtaskへ切り替える
         * 無意味なsmokeにならないよう別taskへ退避する。
         */
        context_next_task = task_get_by_id(task_b_id);
    }
    if (selected_task != NULL && context_next_task != NULL) {
        (void)kernel_run_minimal_context_switch_smoke(selected_task, context_next_task);
    }

    /*
     * 第10章10.1-10.3では、μITRON風API層の入口としてyield_tsk()を観測する。
     * ここでは9.1-9.4のcontext switch smoke後のcurrentを読むだけに留め、
     * DORMANT current taskがREADYへ戻されずrejectされることを確認する。
     */
    (void)yield_tsk();

    /*
     * 第10章10.4では、既存のtask_b -> task_c smokeを壊さないよう、
     * その完了後に協調yield専用taskを追加登録して成功経路を観測する。
     * ここでもtimer IRQ、interrupt exit、dispatch pending経由には接続しない。
     */
    task_yield_from_id = task_register(
        "task_yield_from",
        task_yield_from,
        5,
        task_yield_from_stack,
        sizeof(task_yield_from_stack)
    );
    kernel_log_task_register_result("task_yield_from", task_yield_from_id);

    task_yield_to_id = task_register(
        "task_yield_to",
        task_yield_to,
        1,
        task_yield_to_stack,
        sizeof(task_yield_to_stack)
    );
    kernel_log_task_register_result("task_yield_to", task_yield_to_id);

    if (task_yield_from_id > 0 && task_yield_to_id > 0) {
        /*
         * task_yield_fromは低優先度、task_yield_toは高優先度にしておく。
         * これによりyield_tsk()がfromをREADYへ戻した直後でも、schedulerは
         * from自身ではなくtask_yield_toを次READY候補として選べる。
         */
        (void)kernel_run_yield_running_smoke(task_get_by_id(task_yield_from_id));
    }

    /*
     * 10.4のno-next分岐も通常の協調API smokeで観測する。
     * この時点ではtask_aだけがREADY候補として残るため、yield_tsk()は
     * READY化したcurrent自身を次taskとして扱わず、switchせずに停止する。
     */
    if (task_a_id > 0) {
        (void)kernel_run_yield_running_smoke(task_get_by_id(task_a_id));
    }

    /*
     * 第4章4.3では、cooperative runner側でREADY選択、current commit、
     * current entry直接呼び出し、cooperative return観測、READY再候補化を
     * 有限回だけ繰り返す。dispatcherはentryを呼ばずcurrent commitだけを維持し、
     * schedulerはREADY選択だけを維持する。専用task_runner層は導入しない。
     */
    kernel_run_cooperative_entries();

    /*
     * 第12章12.4では、専用のREADY taskを追加して `wai_sem()` のWAITING化、
     * semaphoreごとのFIFO wait queue enqueue/dequeue、`sig_sem()` によるREADY復帰後の
     * priority比較、そして高優先度wakeup時だけdispatcher境界へ進むことを観測する。
     * priority順wait queue、timeout、同一優先度time slice、round-robinはまだ導入しない。
     */
    task_wai_sem_from_id = task_register(
        "task_wai_sem_from",
        task_wai_sem_from,
        1,
        task_wai_sem_from_stack,
        sizeof(task_wai_sem_from_stack)
    );
    kernel_log_task_register_result("task_wai_sem_from", task_wai_sem_from_id);

    if (task_wai_sem_from_id > 0) {
        kernel_run_semaphore_smoke(task_wai_sem_from_id);
    }

    /*
     * 第13章13.1では、semaphore待ちとは別にdelay待ちの入口だけを観測する。
     * RUNNING currentをdelay WAITINGへ落として既存dispatcher境界へ進むところを確認する。
     * tick到達時READY復帰は後続の13.4 smokeで確認する。
     */
    task_dly_from_id = task_register(
        "task_dly_from",
        task_dly_from,
        5,
        task_dly_from_stack,
        sizeof(task_dly_from_stack)
    );
    kernel_log_task_register_result("task_dly_from", task_dly_from_id);

    task_dly_to_id = task_register(
        "task_dly_to",
        task_dly_to,
        1,
        task_dly_to_stack,
        sizeof(task_dly_to_stack)
    );
    kernel_log_task_register_result("task_dly_to", task_dly_to_id);

    if (task_dly_from_id > 0 && task_dly_to_id > 0) {
        kernel_run_delay_smoke(task_dly_from_id);
    }

    /*
     * 第13章13.3では、timeout付きsemaphore待ちを観測する。
     * twai_sem()はtask文脈APIとしてkernel smoke側から呼び、timer IRQ handlerや
     * task entry本体からは呼ばない。timeout待ちtaskはsemaphore wait queueと
     * delay queueの両方へ登録される。timeout到達時READY復帰は後続の13.4 smokeで確認する。
     */
    task_twai_from_id = task_register(
        "task_twai_from",
        task_twai_from,
        5,
        task_twai_from_stack,
        sizeof(task_twai_from_stack)
    );
    kernel_log_task_register_result("task_twai_from", task_twai_from_id);

    task_twai_to_id = task_register(
        "task_twai_to",
        task_twai_to,
        0,
        task_twai_to_stack,
        sizeof(task_twai_to_stack)
    );
    kernel_log_task_register_result("task_twai_to", task_twai_to_id);

    if (task_twai_from_id > 0 && task_twai_to_id > 0) {
        kernel_run_twai_sem_smoke(task_twai_from_id);
    }

    /*
     * 第13章13.4では、timer tick到達時にdelay queue上のtaskをREADYへ復帰させる。
     * timeout付きsemaphore待ちtaskはsemaphore wait queueからも削除する。
     */
    task_tick_delay_id = task_register(
        "task_tick_delay",
        task_tick_delay,
        1,
        task_tick_delay_stack,
        sizeof(task_tick_delay_stack)
    );
    kernel_log_task_register_result("task_tick_delay", task_tick_delay_id);

    task_tick_twai_id = task_register(
        "task_tick_twai",
        task_tick_twai,
        0,
        task_tick_twai_stack,
        sizeof(task_tick_twai_stack)
    );
    kernel_log_task_register_result("task_tick_twai", task_tick_twai_id);

    task_tick_current_id = task_register(
        "task_tick_current",
        task_tick_current,
        5,
        task_tick_current_stack,
        sizeof(task_tick_current_stack)
    );
    kernel_log_task_register_result("task_tick_current", task_tick_current_id);

    if (task_tick_delay_id > 0 && task_tick_twai_id > 0 && task_tick_current_id > 0) {
        kernel_run_tick_ready_wakeup_smoke(
            task_tick_delay_id,
            task_tick_twai_id,
            task_tick_current_id
        );
    }

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
