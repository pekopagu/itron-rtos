/*
 * SPDX-License-Identifier: MIT
 */

/**
 * @file interrupt.c
 * @brief x86_64 IDT設定とCPU例外観測handler。
 *
 * @details
 * このmoduleは第7章7.1のx86_64向けinterrupt/exception foundationを担当する。
 * 小さなIDTを構築して `lidt` でloadし、CPU例外handlerへの到達をHAL consoleへ
 * 出力する。PIC、APIC、PIT、HPET、IRQ routing、preemption、scheduling、
 * dispatching、context switchingは意図的に初期化・接続しない。第8章8.4では
 * timer IRQ pathをinterrupt entry、kernel IRQ handler、interrupt exit boundaryへ
 * 分けて観測するが、実際のtask切り替えにはまだ接続しない。
 */

#include "interrupt.h"

#include "dispatch_pending.h"
#include "hal/console.h"
#include "pic.h"
#include "preemption.h"
#include "timer.h"

#include <stddef.h>

#define ARCH_IDT_ENTRY_COUNT 256U
#define ARCH_IDT_PRESENT_INTERRUPT_GATE 0x8EU
#define ARCH_GDT_KERNEL_CODE_SELECTOR 0x08U
#define ARCH_INTERRUPT_OK 0
#define ARCH_INTERRUPT_ERR_INVAL (-1)
#define ARCH_TIMER_IRQ_VECTOR 32U
#define ARCH_TIMER_IRQ_LINE 0U

typedef unsigned char arch_u8_t;
typedef unsigned short arch_u16_t;
typedef unsigned int arch_u32_t;
typedef unsigned long long arch_u64_t;
typedef unsigned long arch_uptr_t;

/**
 * @brief x86_64の16-byte IDT gate descriptor。
 *
 * @details
 * selector値は現在のboot/boot.asmのGDT layoutに依存する。64-bit kernel code
 * descriptorはselector 0x08に置かれている。第7章7.1ではGDTを作り直さず、
 * その前提をここに明示してIDT側から参照する。
 */
typedef struct __attribute__((packed)) {
    arch_u16_t offset_low;
    arch_u16_t selector;
    arch_u8_t ist;
    arch_u8_t type_attr;
    arch_u16_t offset_mid;
    arch_u32_t offset_high;
    arch_u32_t reserved;
} arch_idt_entry_t;

/**
 * @brief `lidt` 命令へ渡すIDTR operand。
 */
typedef struct __attribute__((packed)) {
    arch_u16_t limit;
    arch_u64_t base;
} arch_idtr_t;

/**
 * @brief ASM例外stubからC handlerへ渡す最小観測frame。
 *
 * @details
 * これは完全なCPU contextではない。task context switchやpreemption判断には
 * 使わず、例外到達ログに必要な最小情報だけを保持する。
 */
typedef struct {
    arch_u64_t vector;
    arch_u64_t error_code;
    arch_u64_t rip;
    arch_u64_t cs;
    arch_u64_t rflags;
} arch_exception_frame_t;

/**
 * @brief divide error例外をC handlerへ渡すASM entry stub。
 *
 * @details
 * IDT登録用の入口アドレスとしてだけ参照する。C側では呼び出さず、
 * register save/restoreや復帰処理の完成を意味しない。
 */
extern void arch_exception_stub_divide_error(void);

/**
 * @brief breakpoint例外をC handlerへ渡すASM entry stub。
 *
 * @details
 * `VALIDATE_EXCEPTION=1`の観測入口としてIDTへ登録する。
 * 回復可能なdebug trap処理やtask切替には接続しない。
 */
extern void arch_exception_stub_breakpoint(void);

/**
 * @brief invalid opcode例外をC handlerへ渡すASM entry stub。
 *
 * @details
 * 代表的なCPU例外到達を観測するための入口である。
 * 例外原因の修復や実行再開モデルはまだ扱わない。
 */
extern void arch_exception_stub_invalid_opcode(void);

/**
 * @brief general protection例外をC handlerへ渡すASM entry stub。
 *
 * @details
 * IDT gateの到達確認と最小frame観測に使う入口である。
 * privilege管理、user mode、recoverable fault処理は実装しない。
 */
extern void arch_exception_stub_general_protection(void);

/**
 * @brief page fault例外をC handlerへ渡すASM entry stub。
 *
 * @details
 * 例外vector観測用の入口であり、ページテーブル修復やVM subsystemには接続しない。
 */
extern void arch_exception_stub_page_fault(void);

/**
 * @brief IRQ0/vector 32のtimer interruptをC handlerへ渡すASM entry stub。
 *
 * @details
 * 第11章11.2ではtimer IRQ handler到達後にtick更新、高優先度READY検出、
 * dispatch pending要求観測を行い、interrupt exit boundaryから後段dispatch境界へ
 * 委譲できる。ただし、このstub自体は完全な割り込み復帰frame切替や
 * nested interrupt対応を行わない。
 */
extern void arch_timer_irq_stub(void);

static arch_idt_entry_t arch_idt[ARCH_IDT_ENTRY_COUNT];
static int arch_interrupt_initialized;

/**
 * @brief HAL consoleへ10進数を出力する。
 *
 * @details
 * 例外handler内でも使えるように、標準libraryへ依存せず小さな変換処理だけで
 * vector番号を表示する。
 */
static void arch_write_uint(arch_u64_t value)
{
    char buffer[20];
    int index = 0;

    if (value == 0U) {
        hal_console_putc('0');
        return;
    }

    while (value > 0U) {
        buffer[index++] = (char)('0' + (value % 10U));
        value /= 10U;
    }

    while (index > 0) {
        hal_console_putc(buffer[--index]);
    }
}

/**
 * @brief HAL consoleへ64-bit値を16進数で出力する。
 *
 * @details
 * RIPやRFLAGSなどの観測値を読むための補助である。printfを導入せず、
 * HAL consoleの最小出力APIだけでboot-time logを構成する。
 */
static void arch_write_hex64(arch_u64_t value)
{
    static const char digits[] = "0123456789abcdef";
    int nibble;
    int started = 0;

    hal_console_write("0x");
    for (nibble = 15; nibble >= 0; nibble--) {
        unsigned int digit = (unsigned int)((value >> ((unsigned int)nibble * 4U)) & 0xFU);
        if (digit != 0U || started || nibble == 0) {
            hal_console_putc(digits[digit]);
            started = 1;
        }
    }
}

/**
 * @brief 観測対象のCPU例外vectorを表示名へ変換する。
 *
 * @details
 * 第7章7.1では代表的な例外だけを登録する。未登録または未分類のvectorは
 * unknownとして扱い、完全な例外分類表は後続章へ先送りする。
 */
static const char *arch_exception_name(arch_u64_t vector)
{
    switch (vector) {
    case 0:
        return "divide-error";
    case 3:
        return "breakpoint";
    case 6:
        return "invalid-opcode";
    case 13:
        return "general-protection";
    case 14:
        return "page-fault";
    default:
        return "unknown";
    }
}

/**
 * @brief IDT table全体を未使用状態へ初期化する。
 *
 * @details
 * 明示的に登録した例外gateだけが有効になるように、一度すべてのentryを0で
 * 初期化する。未登録vectorを誤ってhandlerへ接続しないための準備処理である。
 */
static void arch_idt_clear(void)
{
    unsigned int index;

    for (index = 0; index < ARCH_IDT_ENTRY_COUNT; index++) {
        arch_idt[index].offset_low = 0;
        arch_idt[index].selector = 0;
        arch_idt[index].ist = 0;
        arch_idt[index].type_attr = 0;
        arch_idt[index].offset_mid = 0;
        arch_idt[index].offset_high = 0;
        arch_idt[index].reserved = 0;
    }
}

/**
 * @brief 指定vectorへ例外entry stubを登録する。
 *
 * @details
 * handler addressをx86_64 IDT gate descriptorのlow/mid/high offsetへ分割して
 * 格納する。selectorはboot時GDTのkernel code selectorを使い、ISTはまだ使わない。
 * ここではgateを作るだけで、割り込み有効化やscheduler連携は行わない。
 */
static int arch_idt_set_gate(unsigned int vector, void (*handler)(void))
{
    arch_uptr_t address;

    if (vector >= ARCH_IDT_ENTRY_COUNT || handler == NULL) {
        return ARCH_INTERRUPT_ERR_INVAL;
    }

    /*
     * x86_64のIDT gateはhandler addressを3つのfieldへ分割して持つ。
     * C側では関数pointerを整数化し、descriptor layoutに合わせて詰める。
     */
    address = (arch_uptr_t)handler;
    arch_idt[vector].offset_low = (arch_u16_t)(address & 0xFFFFU);
    arch_idt[vector].selector = (arch_u16_t)ARCH_GDT_KERNEL_CODE_SELECTOR;
    arch_idt[vector].ist = 0;
    arch_idt[vector].type_attr = (arch_u8_t)ARCH_IDT_PRESENT_INTERRUPT_GATE;
    arch_idt[vector].offset_mid = (arch_u16_t)((address >> 16U) & 0xFFFFU);
    arch_idt[vector].offset_high = (arch_u32_t)((address >> 32U) & 0xFFFFFFFFU);
    arch_idt[vector].reserved = 0;

    return ARCH_INTERRUPT_OK;
}

/**
 * @brief 第7章7.1で観測する代表的なCPU例外handlerをIDTへ登録する。
 *
 * @details
 * timer interruptやIRQではなく、CPUが同期的に発生させる例外だけを対象にする。
 * breakpointは検証buildの `int3` smokeで安全にhandler到達を観測するために含める。
 */
static int arch_idt_install_exception_gates(void)
{
    if (arch_idt_set_gate(0U, arch_exception_stub_divide_error) != ARCH_INTERRUPT_OK) {
        return ARCH_INTERRUPT_ERR_INVAL;
    }
    if (arch_idt_set_gate(3U, arch_exception_stub_breakpoint) != ARCH_INTERRUPT_OK) {
        return ARCH_INTERRUPT_ERR_INVAL;
    }
    if (arch_idt_set_gate(6U, arch_exception_stub_invalid_opcode) != ARCH_INTERRUPT_OK) {
        return ARCH_INTERRUPT_ERR_INVAL;
    }
    if (arch_idt_set_gate(13U, arch_exception_stub_general_protection) != ARCH_INTERRUPT_OK) {
        return ARCH_INTERRUPT_ERR_INVAL;
    }
    if (arch_idt_set_gate(14U, arch_exception_stub_page_fault) != ARCH_INTERRUPT_OK) {
        return ARCH_INTERRUPT_ERR_INVAL;
    }

    return ARCH_INTERRUPT_OK;
}

/**
 * @brief remap済みIRQ0に対応するvector 32 timer IRQ gateを登録する。
 *
 * @details
 * 第8章8.4では、このgateがinterrupt entryの入口である。CPUはIRQ0/vector 32へ
 * 入り、ASM stubからC側のkernel IRQ handlerへ渡す。ただし、この登録自体は
 * 本格的なregister save/restore、通常のinterrupt return、dispatcher接続を意味しない。
 */
static int arch_idt_install_timer_irq_gate(void)
{
    return arch_idt_set_gate(ARCH_TIMER_IRQ_VECTOR, arch_timer_irq_stub);
}

/**
 * @brief 構築済みIDTRをCPUへloadする。
 *
 * @details
 * `lidt` はIDTの場所をCPUへ教えるだけで、maskable interruptを有効化しない。
 * そのため、この処理はtimer interruptやpreemption開始を意味しない。
 */
static void arch_lidt(const arch_idtr_t *idtr)
{
    __asm__ volatile ("lidt %0" : : "m"(*idtr) : "memory");
}

/**
 * @brief CPU例外到達を観測目的だけで処理する。
 *
 * @details
 * このhandlerは例外情報をログ出力してからhaltする。復帰可能な例外handlerでは
 * ない。scheduler、dispatcher、preemption、task状態変更、semaphore timeout、
 * context switch codeは呼び出さない。
 *
 * @param frame ASM entry stubが作成した最小観測frame。
 */
void arch_exception_handle(const arch_exception_frame_t *frame) __attribute__((noreturn));
void arch_exception_handle(const arch_exception_frame_t *frame)
{
    hal_console_write("[interrupt] exception: vector=");
    if (frame == NULL) {
        hal_console_write("unknown name=unknown\n");
    } else {
        arch_write_uint(frame->vector);
        hal_console_write(" name=");
        hal_console_write(arch_exception_name(frame->vector));
        hal_console_write(" error=");
        arch_write_hex64(frame->error_code);
        hal_console_write(" rip=");
        arch_write_hex64(frame->rip);
        hal_console_write(" cs=");
        arch_write_hex64(frame->cs);
        hal_console_write(" rflags=");
        arch_write_hex64(frame->rflags);
        hal_console_write("\n");
    }

    hal_console_write("[interrupt] exception handler halting\n");
    /*
     * 第7章7.1では復帰経路を実装しない。ログを残した後は停止し、
     * 後続章で安全な復帰や割り込みnestingを設計する余地を残す。
     */
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

/**
 * @brief timer IRQのinterrupt exit boundaryで後段dispatch境界へ委譲する。
 *
 * @details
 * 第11章11.2では、IRQ handler本体から直接dispatcherを呼ばず、このexit boundaryが
 * dispatch_pending moduleのconsume APIへ委譲する。第11章11.3では同一優先度READYだけで
 * pendingが作られない場合を `dispatch-pending=none action=no-dispatch` として観測する。
 * pendingがなければ何もせず、
 * requestedの場合だけdeferred dispatchとして既存のtask-to-task switch境界へ進む。
 * これは完全な割り込み復帰frame切替ではなく、EOI前の教育用後段境界である。
 */
static void arch_timer_irq_exit_observe_boundary(void)
{
    hal_console_write("[timer-irq] exit boundary: dispatch-pending=");
    if (dispatch_pending_is_requested()) {
        /*
         * requestedの場合だけ後段dispatchへ委譲する。
         * handler本体にscheduler/dispatcherの詳細を増やさず、exit boundaryを
         * 「割り込み中に決めた要求を安全に消費する場所」として観測できるようにする。
         */
        hal_console_write("requested action=deferred-dispatch\n");
        (void)dispatch_pending_consume_at_deferred_boundary();
        return;
    }

    /*
     * pendingがない場合は切替を試みない。
     * no-dispatch側でもconsume APIを呼び、11.3の同一優先度READY除外を含む
     * no-pendingの観測ログを同じ境界に集約する。
     */
    hal_console_write("none action=no-dispatch\n");
    (void)dispatch_pending_consume_at_deferred_boundary();
}

/**
 * @brief IRQ0/vector 32 timer interrupt entry到達を最小限に観測する。
 *
 * @details
 * このhandlerは第8章8.4で整理するkernel IRQ handlerである。割り込み中のserial logは
 * 通常boot logと同じ出力経路を使うため、既存log列の途中に混ざり得る。従って、
 * この出力は通常ログの順序保証やinterrupt-safe logging基盤を示すものではなく、
 * 明示validation時のhandler到達、tick更新、preemption decision、dispatch pending観測、
 * exit boundary観測の証跡としてだけ扱う。
 *
 * handlerの責務は `timer_tick()`、`preemption_evaluate_from_irq()`、
 * `dispatch_pending_log_state_from_irq()`、interrupt exit boundary委譲、IRQ0 EOIまでである。
 * 第11章11.2ではexit boundary側でpendingをconsumeし、妥当な場合だけ
 * dispatcher/task_contextの既存task-to-task switch smokeへ接続する。handler本体から
 * `yield_tsk()` や `dispatcher_switch_to()` は直接呼ばない。nested interrupt、
 * 連続割り込み、完全な割り込み復帰frame切替は扱わない。
 */
void arch_timer_irq_handle(void)
{
    const char *dispatch_not_requested_reason;

    /*
     * 明示validationでIRQ0/vector 32へ到達した事実だけを残す。
     * 割り込み中のserial出力は通常ログへ混ざり得るため、この行を
     * 通常bootの順序保証されたログとして扱わない。
     */
    hal_console_write("[timer-irq] entry reached: vector=32 irq=0\n");

    /*
     * 第8章8.1では、hardware timer interrupt起点でkernel timerのtickを1つ進める。
     * timer_tick()はpublic timer APIであり、scheduler、dispatcher、context switch、
     * preemption、task state変更へは接続しない。
     */
    timer_tick();

    /*
     * 第8章8.2では、tick更新後にpreemption decisionの入口を呼ぶ。
     * 第8章8.3では、switch-target decisionをdispatch pendingへ記録する。
     * ただし、ここではdispatcher commit、context switch、task state変更へ進まない。
     */
    dispatch_not_requested_reason = preemption_evaluate_from_irq();

    /*
     * 第8章8.3では dispatch pending を観測専用の論理状態として扱う。
     * ここでは log に出すだけで、dispatcher commit、context switch、
     * task state変更、interrupt return直前の切替には接続しない。
     */
    /*
     * 第11章11.4では、この観測点でrequest/not-requestedを出した後に
     * exit boundaryへ進む順序を固定する。handler本体はここでも
     * yield_tsk()やdispatcher_switch_to()を直接呼ばない。
     */
    dispatch_pending_log_state_from_irq(dispatch_not_requested_reason);

    /*
     * 第8章8.4では、interrupt exit boundaryを将来のdispatch pending消費候補として
     * 観測するだけに留める。requestedでもnot-requestedでも実dispatchは行わない。
     */
    arch_timer_irq_exit_observe_boundary();

    arch_pic_send_eoi(ARCH_TIMER_IRQ_LINE);
    hal_console_write("[timer-irq] eoi sent: irq=0\n");
}

/**
 * @brief x86_64の割り込み・例外観測用IDTを初期化する。
 *
 * @details
 * CPU例外用gateと、remap済みIRQ0/vector 32用gateをIDTへ登録してから
 * `lidt` でCPUへ渡す。ここで行うのはentry到達を観測できる最小構造の準備であり、
 * maskable interruptの有効化、PIT設定、timer tick、scheduler、dispatcher、
 * context switch、preemptionへの接続は行わない。
 *
 * @return 成功時は0。IDT gate登録に失敗した場合は負値。
 */
int arch_interrupt_init(void)
{
    arch_idtr_t idtr;

    /*
     * 初期化開始を通常boot logへ残す。ここではまだ割り込みは有効化されておらず、
     * 以降の失敗位置をserial logから切り分けるための通常ログである。
     */
    hal_console_write("[interrupt] init begin\n");

    /*
     * 前回の内容を持ち越さず、登録したgateだけが有効になる状態から組み立てる。
     * 未登録vectorを誤ってhandlerへ接続しないための防御的な初期化である。
     */
    arch_idt_clear();

    /*
     * CPU例外の観測経路を先に登録する。IRQ用vectorとは分けておくことで、
     * 例外観測と外部割り込み観測の責務を混同しない。
     */
    if (arch_idt_install_exception_gates() != ARCH_INTERRUPT_OK) {
        hal_console_write("[interrupt] init failed: gate install\n");
        return ARCH_INTERRUPT_ERR_INVAL;
    }

    /*
     * IRQ0/vector 32のentryだけを登録する。登録は到達可能性の準備であり、
     * 通常bootでIRQ0をunmaskすることやtimer subsystemを開始することを意味しない。
     */
    if (arch_idt_install_timer_irq_gate() != ARCH_INTERRUPT_OK) {
        hal_console_write("[interrupt] init failed: timer irq gate install\n");
        return ARCH_INTERRUPT_ERR_INVAL;
    }

    /*
     * IDTRのlimitはtable byte数そのものではなく「末尾offset」なので、
     * sizeof(table) - 1を設定する。baseは静的IDT tableの先頭addressである。
     */
    idtr.limit = (arch_u16_t)(sizeof(arch_idt) - 1U);
    idtr.base = (arch_u64_t)(arch_uptr_t)&arch_idt[0];

    /*
     * CPUへIDTの場所を知らせるだけで、割り込み配送はまだ開始しない。
     * `sti` はvalidation専用入口に閉じ、通常bootのsmoke flowを安定させる。
     */
    arch_lidt(&idtr);

    arch_interrupt_initialized = 1;
    hal_console_write("[interrupt] idt initialized\n");
    hal_console_write("[interrupt] idt loaded\n");
    return ARCH_INTERRUPT_OK;
}

/**
 * @brief 例外entry到達確認用の同期trapを発生させる。
 *
 * @details
 * 明示validation buildでだけ使う補助入口である。`int3` によりCPU例外handlerへの
 * 到達を観測するが、外部IRQ、timer interrupt、scheduler、dispatcher、
 * context switchへは接続しない。通常bootでは呼び出さない。
 */
void arch_interrupt_trigger_validation_exception(void)
{
    if (!arch_interrupt_initialized) {
        hal_console_write("[interrupt] validation skipped: not initialized\n");
        return;
    }

    /*
     * trap発生直前に通常ログを出し、以降の例外handler logが意図したvalidationの
     * 結果であることをQEMU serial log上で識別できるようにする。
     */
    hal_console_write("[interrupt] validation trigger: int3\n");
    /*
     * int3はbreakpoint例外(vector 3)を同期的に発生させる。外部IRQやtimerを
     * 使わず、IDT登録とentry stub到達だけをQEMU serial logで確認する。
     */
    __asm__ volatile ("int3");
}

/**
 * @brief timer IRQ tick接続観測用にIRQ0配送を一時的に許可する。
 *
 * @details
 * `VALIDATE_TIMER_IRQ_ENTRY=1` のbuildでだけ呼ばれるx86_64固有のvalidation入口である。
 * legacy PIC上のIRQ0をunmaskし、`sti` でCPUのmaskable interruptを受け付ける。
 * これはhandler到達とtick更新を観測するための一時的な構成であり、PIT programming、
 * scheduler、dispatcher、context switch、preemption、通常のinterrupt return modelを開始しない。
 */
void arch_interrupt_enable_timer_entry_validation(void)
{
    if (!arch_interrupt_initialized) {
        hal_console_write("[timer-irq] validation skipped: interrupt not initialized\n");
        return;
    }

    /*
     * validation入口に入ったことを通常ログとして先に残す。
     * この後に出る `[timer-irq] entry reached` は割り込み中観測ログであり、
     * 通常ログ列へ混ざる可能性がある。
     */
    hal_console_write("[timer-irq] validation enable: unmask irq0 and sti\n");

    /*
     * IRQ0だけを開く。PICのI/O portやmask bitの詳細はPIC moduleへ閉じ込め、
     * interrupt.c側は「IRQ0を観測対象にする」という意図だけを表す。
     */
    arch_pic_unmask_irq(ARCH_TIMER_IRQ_LINE);
    /*
     * PIT programmingは行わない。QEMUの既定状態で届くIRQ0がvector 32 entryへ
     * 到達できることだけを観測するため、CPU側のmaskable interruptを開く。
     */
    /*
     * CPU側でmaskable interruptを受け付ける。通常bootではここへ到達しないため、
     * 既存smoke flowはtimer IRQ観測の影響を受けない。
     */
    __asm__ volatile ("sti");
}
