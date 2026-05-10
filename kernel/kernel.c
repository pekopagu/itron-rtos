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
 *
 * kernel層はarch依存コードを直接呼ばず、依存方向は
 * kernel → HAL → arch(x86_64) → serial → COM1 である。
 */

#include "hal/console.h"
#include "dispatcher.h"
#include "scheduler.h"
#include "task.h"
#include "task_context.h"

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
 * @brief 第5章5.3の最小context switch smokeを1回だけ実行する。
 *
 * @details
 * schedulerはREADY task選択だけ、dispatcherはcurrent commitだけを担当する。
 * このhelperはcommit済みtaskのcontextを初回stack frameとして準備し、boot
 * contextからそのtask contextへ明示的にswitchする。task entryは復元された
 * task stack上のtrampolineから1回呼ばれ、return後にboot contextへ戻る。
 *
 * これはtimer interrupt、preemption、割り込みハンドラ連携、yield API、
 * 本格task lifecycleを導入しないためのboot-time verification modelである。
 *
 * @param selected schedulerが選択したREADY task。
 * @return 成功時は0、失敗時は負の値。
 */
static int kernel_run_minimal_context_switch_smoke(const tcb_t *selected)
{
    tcb_t *current_task;
    int commit_result;
    int prepare_result;
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

    prepare_result = task_context_prepare_initial_frame(current_task);
    if (prepare_result != TASK_CONTEXT_OK) {
        hal_console_write("[context-smoke] stop: reason=prepare-failed\n");
        return prepare_result;
    }

    switch_result = task_context_switch_to_task(current_task);
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
 * @brief kernelのメインエントリポイント。
 *
 * @details
 * HAL consoleを初期化し、起動ログと初期タスク管理の確認ログを出力する。
 * タスク登録後は `task_dump()` で一覧を表示し、簡易schedulerの選択結果を表示する。
 * 第4章4.3では、kernel.cのboot-time cooperative runnerがREADY taskを選び、
 * dispatcherでcurrentとしてcommitし、current task entryを通常のC関数呼び出しで
 * 直接呼ぶ。entry returnは正式終了ではなくcooperative return eventとして観測し、
 * RUNNING taskをREADYへ戻して再びscheduler候補にする。
 *
 * @param なし。
 * @return なし。
 * @note task_start、context_switch、stack switch、割り込み、タイマ、プリエンプションは追加しない。
 */
void kernel_main(void)
{
    int task_a_id;
    int task_b_id;
    int task_c_id;
    const tcb_t *selected_task;

    hal_console_init();
    hal_console_write("itron-rtos booting...\n");
    hal_console_write("kernel_main reached\n");

    /* タスク管理台帳だけを初期化し、スケジューラや実行コンテキストは作らない。 */
    task_init();
    scheduler_init();
    dispatcher_init();

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

    /*
     * 第3章3.2では「どのタスクが次に実行対象になるか」を選ぶだけで、
     * 選択されたentry関数を呼び出したりRUNNINGへ遷移させたりしない。
     */
    selected_task = scheduler_select_next();
    kernel_log_scheduler_selection("after_register", selected_task);

    /*
     * 第5章5.3では、timer/preemptionなしの明示的な最小context switch経路を
     * 1回だけ確認する。既存のcooperative runnerは後続の比較用に残すが、
     * このsmokeではboot contextからtask stackへ入り、entry return後にbootへ戻る。
     */
    if (selected_task != NULL) {
        (void)kernel_run_minimal_context_switch_smoke(selected_task);
    }

    /*
     * 第4章4.3では、cooperative runner側でREADY選択、current commit、
     * current entry直接呼び出し、cooperative return観測、READY再候補化を
     * 有限回だけ繰り返す。dispatcherはentryを呼ばずcurrent commitだけを維持し、
     * schedulerはREADY選択だけを維持する。専用task_runner層は導入しない。
     */
    kernel_run_cooperative_entries();

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
