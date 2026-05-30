/*
 * SPDX-License-Identifier: MIT
 */

/**
 * @file task_context.c
 * @brief 第5章5.3向けのTCB-level最小context switch実装。
 */

#include <stddef.h>

#include "context_switch.h"
#include "hal/console.h"
#include "task_context.h"

/**
 * @brief 初期task stackから最初に戻るASM trampoline。
 *
 * @details
 * `task_context_prepare_initial_frame()`がtask stack上にreturn addressとして積む入口である。
 * C側では直接呼び出さず、arch context switch後にtask entryへ橋渡しする。
 * 完全なtask lifecycleや割り込み復帰frameを実装するものではない。
 */
extern void task_context_trampoline(void);

static task_context_t boot_context;
static tcb_t *active_task;
/*
 * 第9章9.1の起動時smokeだけで使う一時的な切替予約である。
 * first taskのentry return観測点まで保持し、そこでsecond taskへ進むかを判定する。
 * 汎用dispatcher状態ではないため、通常のtask選択や割り込み経路からは参照しない。
 */
static tcb_t *task_to_task_smoke_from;
static tcb_t *task_to_task_smoke_to;

/**
 * @brief 符号なし整数をHAL consoleへ10進数で出力する。
 *
 * @details
 * freestanding環境ではprintfを導入していないため、context switch smokeの
 * 観測に必要な最小限の数値出力だけを行う。表示補助であり、TCBやCPU contextは
 * 変更しない。
 *
 * @param value 出力する符号なし整数。
 * @return なし。
 */
static void context_write_uint(unsigned long value)
{
    char buffer[20];
    int index = 0;

    if (value == 0) {
        /* 0は桁分解loopでは出力されないため、特別扱いで1文字だけ出す。 */
        hal_console_putc('0');
        return;
    }

    /* 下位桁から取り出すため、いったん逆順でbufferへ積む。 */
    while (value > 0) {
        buffer[index++] = (char)('0' + (value % 10));
        value /= 10;
    }

    /* bufferへ逆順に積んだ桁を、表示順へ戻して出力する。 */
    while (index > 0) {
        hal_console_putc(buffer[--index]);
    }
}

/**
 * @brief 符号付き整数をHAL consoleへ10進数で出力する。
 *
 * @details
 * context switch wrapperの戻り値やtask idを同じ出力経路で観測するための
 * 補助関数である。負数は先に符号を出し、絶対値部分を符号なし出力へ委譲する。
 *
 * @param value 出力する符号付き整数。
 * @return なし。
 */
static void context_write_int(int value)
{
    if (value < 0) {
        /* error codeを読みやすくするため、符号を明示してから数値を出す。 */
        hal_console_putc('-');
        context_write_uint((unsigned long)(-value));
        return;
    }

    context_write_uint((unsigned long)value);
}

/**
 * @brief register値やaddress値を16進数で出力する。
 *
 * @details
 * `context.rsp` などのpointer相当値は10進数より16進数の方がstack範囲との
 * 対応を確認しやすい。先頭の0を省略して、QEMU serial logを短く保つ。
 *
 * @param value 出力する値。
 * @return なし。
 */
static void context_write_hex(unsigned long value)
{
    static const char digits[] = "0123456789abcdef";
    int shift;
    int started = 0;

    hal_console_write("0x");
    if (value == 0) {
        /* 0だけは先頭0省略の結果が空になるため、明示的に出力する。 */
        hal_console_putc('0');
        return;
    }

    /* 上位nibbleから確認し、最初の非0以降だけを出力する。 */
    for (shift = (int)(sizeof(unsigned long) * 8) - 4; shift >= 0; shift -= 4) {
        unsigned long nibble = (value >> shift) & 0xFUL;

        if (nibble != 0 || started) {
            hal_console_putc(digits[nibble]);
            started = 1;
        }
    }
}

/**
 * @brief task名をlog出力用の安全な文字列として返す。
 *
 * @details
 * 異常系logでもNULL pointerをHAL consoleへ渡さないようにするための
 * 表示補助である。task状態やcontextは変更しない。
 *
 * @param task 表示対象task。NULLも許容する。
 * @return 表示用の静的文字列またはTCB上のtask名。
 */
static const char *context_task_name(const tcb_t *task)
{
    if (task == NULL) {
        return "(null)";
    }

    return (task->name != NULL) ? task->name : "(unnamed)";
}

/**
 * @brief task stateをentry return最終化ログ用の短い文字列へ変換する。
 *
 * @details
 * 第9章9.4では、dispatcher_switch_to()が行ったRUNNING/READY遷移とは別に、
 * task_context層がentry return後の最終状態をDORMANTへ確定する。
 * その直前状態をログへ残すための表示補助であり、TCB状態は変更しない。
 *
 * @param state 表示するtask状態。
 * @return 状態名を表す静的文字列。
 */
static const char *context_task_state_name(task_state_t state)
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
 * @brief task識別情報をprefix付きでserial logへ出力する。
 *
 * @details
 * switch元、switch先、復帰元を同じ形式で出力し、QEMU serial log上で
 * context switchの方向を追いやすくする。表示専用であり、scheduler選択や
 * dispatcher commitは行わない。
 *
 * @param label `from` や `to` など、出力する役割名。
 * @param task 表示対象task。NULLの場合はnoneとして出力する。
 * @return なし。
 */
static void context_log_task_prefix(const char *label, const tcb_t *task)
{
    hal_console_write(label);
    if (task == NULL) {
        /* 異常系でもlogの形を崩さず、対象taskなしを明示する。 */
        hal_console_write(" none");
        return;
    }

    hal_console_write(" id=");
    context_write_int(task->id);
    hal_console_write(" name=");
    hal_console_write(context_task_name(task));
}

/**
 * @brief `rsp` 相当値をlabel付きでserial logへ出力する。
 *
 * @details
 * 保存前、復元対象、保存後の `context.rsp` を同じ形式で出し、stack switchの
 * 観測点を揃えるための補助関数である。
 *
 * @param label 出力前に付ける説明文字列。
 * @param rsp 出力するstack pointer値。
 * @return なし。
 */
static void context_log_rsp(const char *label, unsigned long rsp)
{
    hal_console_write(label);
    context_write_hex(rsp);
}

/**
 * @brief context switch対象taskが最小条件を満たすか検証する。
 *
 * @details
 * arch primitiveは低レベル境界であり、NULLや未準備stackを防御しない。
 * C wrapper側でTCB、stack metadata、`context.rsp` を検証し、不正な入力で
 * CPU register復元へ進まないようにする。
 *
 * @param task 検証対象task。
 * @return 成功時はTASK_CONTEXT_OK、失敗時はTASK_CONTEXT_ERR_*。
 */
static int context_validate_task(const tcb_t *task)
{
    if (task == NULL || task->stack_base == NULL || task->stack_top == NULL) {
        return TASK_CONTEXT_ERR_INVAL;
    }

    if (task->stack_size < 64) {
        /* trampoline用return addressを置けないほど小さいstackは拒否する。 */
        return TASK_CONTEXT_ERR_INVAL;
    }

    if (task->context.rsp == 0) {
        /* context.rspが0のtaskは、復元先stack pointerを持たない。 */
        return TASK_CONTEXT_ERR_BAD_STATE;
    }

    return TASK_CONTEXT_OK;
}

/**
 * @brief 登録済みtaskのcontextに初回entry用stack frameを準備する。
 *
 * @details
 * task登録時点の `context.rsp == stack_top` を、実際に `ret` できるstackへ
 * 進める。task stack上にはtrampoline addressだけを置き、完全なtask lifecycleや
 * 割り込み復帰frameはまだ作らない。
 *
 * @param task 初回frameを準備するtask。
 * @return 成功時はTASK_CONTEXT_OK、失敗時はTASK_CONTEXT_ERR_*。
 */
int task_context_prepare_initial_frame(tcb_t *task)
{
    unsigned long stack_top;
    unsigned long *stack_pointer;

    if (context_validate_task(task) != TASK_CONTEXT_OK) {
        hal_console_write("[context] prepare failed: invalid task\n");
        return TASK_CONTEXT_ERR_INVAL;
    }

    stack_top = (unsigned long)task->stack_top;
    stack_top &= ~0xFUL;
    stack_pointer = (unsigned long *)stack_top;

    /*
     * arch switchは復元されたstackへretする。初回実行taskには保存済みreturn addressが
     * まだ存在しないため、学習用frameとしてtrampolineへ入り、entryを1回だけ呼んで
     * bootへ切り戻す。これは実際のtask lifecycle frameより意図的に小さい。
     */
    stack_pointer--;
    *stack_pointer = (unsigned long)task_context_trampoline;

    /*
     * 初回復元時のret先をstackへ積んだので、context.rspはそのreturn addressを
     * 指す位置に更新する。r12にはtrampolineがC側へ渡すTCB pointerを置く。
     */
    task->context.rsp = (unsigned long)stack_pointer;
    task->context.rbp = 0;
    task->context.rbx = 0;
    task->context.r12 = (unsigned long)task;
    task->context.r13 = 0;
    task->context.r14 = 0;
    task->context.r15 = 0;

    hal_console_write("[context] prepared initial frame:");
    context_log_task_prefix(" task", task);
    context_log_rsp(" context.rsp=", task->context.rsp);
    hal_console_write("\n");

    return TASK_CONTEXT_OK;
}

/**
 * @brief task context同士を明示的に切り替える。
 *
 * @details
 * switch元・先のTCBを検証し、保存前後の `context.rsp` をlogへ出したうえで
 * arch primitiveへ委譲する。scheduler選択とdispatcher commitは呼び出し側の責務である。
 *
 * @param from switch元task。
 * @param to switch先task。
 * @return switch経路が戻った場合はTASK_CONTEXT_OK、失敗時はTASK_CONTEXT_ERR_*。
 */
int task_context_switch(tcb_t *from, tcb_t *to)
{
    unsigned long before_rsp;

    if (context_validate_task(from) != TASK_CONTEXT_OK ||
        context_validate_task(to) != TASK_CONTEXT_OK) {
        hal_console_write("[context] switch failed: invalid task context\n");
        return TASK_CONTEXT_ERR_INVAL;
    }

    before_rsp = from->context.rsp;

    /* switch前に保存前rspと復元対象rspを出し、以後の変化と比較できるようにする。 */
    hal_console_write("[context] switch begin:");
    context_log_task_prefix(" from", from);
    context_log_task_prefix(" to", to);
    context_log_rsp(" from.rsp.before=", before_rsp);
    context_log_rsp(" to.rsp.restore=", to->context.rsp);
    hal_console_write("\n");

    arch_context_switch(&from->context, &to->context);

    /*
     * ここに戻るのは、後続のswitchで `from` が復元された後である。
     * 保存後のrspを出力し、実際にarch primitiveがfrom contextを更新したことを観測する。
     */
    hal_console_write("[context] switch resumed:");
    context_log_task_prefix(" task", from);
    context_log_rsp(" from.rsp.before=", before_rsp);
    context_log_rsp(" from.rsp.after=", from->context.rsp);
    hal_console_write("\n");

    return TASK_CONTEXT_OK;
}

/**
 * @brief boot contextから準備済みtask contextへ切り替える。
 *
 * @details
 * 第5章5.3のsmoke pathとして、boot stack上の実行を保存してtask stackへ入り、
 * task側からboot contextが復元された後に戻る。timerやpreemptionは使わない。
 *
 * @param to switch先task。
 * @return taskからbootへ戻った場合はTASK_CONTEXT_OK、失敗時はTASK_CONTEXT_ERR_*。
 */
int task_context_switch_to_task(tcb_t *to)
{
    if (context_validate_task(to) != TASK_CONTEXT_OK) {
        hal_console_write("[context] boot switch failed: invalid task context\n");
        return TASK_CONTEXT_ERR_INVAL;
    }

    active_task = to;

    /* boot_contextはこの時点では未保存なので、before値は0として観測される。 */
    hal_console_write("[context] switch begin:");
    hal_console_write(" from=boot");
    context_log_task_prefix(" to", to);
    context_log_rsp(" boot.rsp.before=", boot_context.rsp);
    context_log_rsp(" to.rsp.restore=", to->context.rsp);
    hal_console_write("\n");

    arch_context_switch(&boot_context, &to->context);

    /* task側からboot_contextが復元された後、保存済みtask rspを確認する。 */
    hal_console_write("[context] switch resumed:");
    hal_console_write(" task=boot");
    context_log_rsp(" boot.rsp.after=", boot_context.rsp);
    if (active_task != NULL) {
        context_log_task_prefix(" from", active_task);
        context_log_rsp(" from.rsp.saved=", active_task->context.rsp);
    }
    hal_console_write("\n");

    active_task = NULL;
    return TASK_CONTEXT_OK;
}

/**
 * @brief bootからfirstへ入り、firstのentry return後にsecondへ一度だけ切り替える。
 *
 * @details
 * 第9章9.1の起動時smoke専用helperである。既存のboot-to-task smokeを
 * 再利用しつつ、first task上のreturn観測点でsecond task contextへ進む
 * 候補を設定する。これは正式なdispatcher境界ではなく、dispatch pending、
 * timer IRQ、yield API、preemption、time sliceとは接続しない。
 *
 * @param first boot contextから最初に入るtask。
 * @param second first taskから切り替えるtask。
 * @return bootへ戻った場合はTASK_CONTEXT_OK、失敗時はTASK_CONTEXT_ERR_*。
 */
int task_context_switch_to_task_pair(tcb_t *first, tcb_t *second)
{
    int result;

    if (first == NULL || second == NULL || first == second) {
        hal_console_write("[context] task-to-task smoke failed: invalid task pair\n");
        return TASK_CONTEXT_ERR_INVAL;
    }

    result = task_context_prepare_initial_frame(first);
    if (result != TASK_CONTEXT_OK) {
        return result;
    }

    /*
     * firstとsecondの両方をあらかじめret可能なstack frameにしておく。
     * firstからsecondへ切り替えた直後はsecond側のtrampolineへretするため、
     * secondのframe準備をfirst実行後に遅らせることはできない。
     */
    result = task_context_prepare_initial_frame(second);
    if (result != TASK_CONTEXT_OK) {
        return result;
    }

    /*
     * boot -> first の既存経路を再利用するため、ここでは予約だけを置く。
     * 実際の first -> second switch はtask_context_enter()内の
     * entry return観測点で行う。
     */
    task_to_task_smoke_from = first;
    task_to_task_smoke_to = second;

    result = task_context_switch_to_task(first);

    /*
     * bootへ戻ってきた時点で、予約したfirst -> secondの観測経路は完了している。
     * 10.4ではyield_tsk()から到達したことをログだけで追えるよう、beginに対応する
     * endをtask_context層で明示する。
     */
    hal_console_write("[context] task-to-task switch end\n");

    task_to_task_smoke_from = NULL;
    task_to_task_smoke_to = NULL;
    return result;
}

/**
 * @brief trampolineから呼ばれ、task entryを1回実行してbootへ戻す。
 *
 * @details
 * arch primitiveがtask stackを復元した後、trampoline経由でこの関数へ入る。
 * 第9章9.4では、entry returnをREADY復帰ではなく、その起動分の実行完了として
 * DORMANTへ最終化する。ここで行うのはentry return後のtask lifecycle確定だけであり、
 * scheduler選択、dispatcher current更新、dispatch pending消費、interrupt exit接続は行わない。
 *
 * @param task 実行対象task。
 * @return なし。正常系ではboot contextへswitch backする。
 */
void task_context_enter(tcb_t *task)
{
    task_state_t returned_state;
    int finalize_result;

    if (task == NULL || task->entry == NULL) {
        hal_console_write("[context] task entry skipped: invalid task\n");
        for (;;) {
            /* 復帰先が安全に作れないため、異常系ではCPUを停止して観測可能にする。 */
            __asm__ volatile ("hlt");
        }
    }

    /* trampolineからtask stackへ入ったことを、entry呼び出し前に明示する。 */
    hal_console_write("[context] entered task stack:");
    context_log_task_prefix(" task", task);
    context_log_rsp(" current.rsp=", task->context.rsp);
    hal_console_write("\n");

    task->entry();

    /* entry returnを観測し、9.4のtask lifecycle確定点としてDORMANTへ落とす。 */
    hal_console_write("[context] task entry returned:");
    context_log_task_prefix(" task", task);
    hal_console_write("\n");

    /*
     * 第9章9.3ではRUNNING/READY遷移をdispatcher_switch_to()境界へ移した。
     * 第9章9.4では、その境界責務は維持したまま、entry returnしたtaskを
     * task_context層でDORMANTへ最終化する。READYを受け付けるのは、
     * dispatcherがfrom taskへRUNNING->READYを適用済みの9.1 smoke経路を
     * 最終的にDORMANTへ落とすためであり、READY復帰として継続実行させるためではない。
     */
    /*
     * 最終化前の状態を先に保存する。
     * DORMANTへ更新した後では、ログ上で「RUNNINGから完了したのか」
     * 「9.3のfrom遷移でREADYへ戻った後に完了したのか」を判別できなくなるためである。
     */
    returned_state = task->state;

    /*
     * entry returnは再スケジュール可能なREADY復帰ではなく、この起動分の完了として扱う。
     * 実際のTCB更新はtask管理層へ委譲し、task_context層はentry return観測点から
     * lifecycle確定を依頼するだけに留める。
     */
    finalize_result = task_mark_dormant_from_entry_return(task->id);
    if (finalize_result == 0) {
        /*
         * 成功時は「直前状態->DORMANT」を明示する。
         * task_bは9.3のdispatcher遷移によりREADY->DORMANT、
         * task_cは実行中entryのreturnとしてRUNNING->DORMANTになる。
         */
        hal_console_write("[context] task entry finalized:");
        context_log_task_prefix(" task", task);
        hal_console_write(" ");
        hal_console_write(context_task_state_name(returned_state));
        hal_console_write("->DORMANT\n");
    } else {
        /*
         * ここへ来る場合は、task_context層が想定外の状態を完了扱いにしようとした
         * 可能性がある。異常を隠してswitchを続けるとstate modelの崩れを追えないため、
         * 元状態と結果コードを必ずログへ残す。
         */
        hal_console_write("[context] task entry finalize failed:");
        context_log_task_prefix(" task", task);
        hal_console_write(" state=");
        hal_console_write(context_task_state_name(returned_state));
        hal_console_write(" result=");
        context_write_int(finalize_result);
        hal_console_write("\n");
    }

    if (task_to_task_smoke_from == task &&
        task_to_task_smoke_to != NULL) {
        tcb_t *next_task = task_to_task_smoke_to;

        /*
         * 第9章9.4では、first taskはすでにDORMANTへ最終化済みである。
         * それでも9.1のtask-to-task smokeを維持するため、予約済みsecond contextへ
         * 一度だけ進む。これはDORMANT taskを再起動する処理ではなく、準備済みstack間の
         * 観測を継続するための限定経路である。
         */
        task_to_task_smoke_from = NULL;
        task_to_task_smoke_to = NULL;
            /*
             * ここでactive_taskをsecondへ移すことで、secondからbootへ戻った後の
             * resumedログが「最後にbootへ戻したtask」を示せるようにする。
             * dispatcherのcurrent pointerを更新する正式処理ではない。
             */
            active_task = next_task;
            hal_console_write("[context] task-to-task switch begin:");
            context_log_task_prefix(" from", task);
            context_log_task_prefix(" to", next_task);
            context_log_rsp(" from.rsp.before=", task->context.rsp);
            context_log_rsp(" to.rsp.restore=", next_task->context.rsp);
            hal_console_write("\n");

            arch_context_switch(&task->context, &next_task->context);

            /*
             * 現在の9.1 smokeでは通常ここへ戻らない。将来secondからfirstへ戻る
             * 経路を作った場合に、保存済みrspを観測できるようログだけ残す。
             */
            hal_console_write("[context] task-to-task switch resumed:");
            context_log_task_prefix(" task", task);
            context_log_rsp(" from.rsp.after=", task->context.rsp);
            hal_console_write("\n");
    }

    hal_console_write("[context] switch back:");
    context_log_task_prefix(" from", task);
    hal_console_write(" to=boot");
    context_log_rsp(" from.rsp.before=", task->context.rsp);
    context_log_rsp(" boot.rsp.restore=", boot_context.rsp);
    hal_console_write("\n");

    arch_context_switch(&task->context, &boot_context);

    for (;;) {
        /* 正常系ではここへ戻らない。戻った場合は制御流異常として停止する。 */
        __asm__ volatile ("hlt");
    }
}
