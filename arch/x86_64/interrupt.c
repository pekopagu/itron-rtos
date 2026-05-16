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
 * dispatching、context switchingは意図的に初期化・接続しない。
 */

#include "interrupt.h"

#include "hal/console.h"

#include <stddef.h>

#define ARCH_IDT_ENTRY_COUNT 256U
#define ARCH_IDT_PRESENT_INTERRUPT_GATE 0x8EU
#define ARCH_GDT_KERNEL_CODE_SELECTOR 0x08U
#define ARCH_INTERRUPT_OK 0
#define ARCH_INTERRUPT_ERR_INVAL (-1)

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

extern void arch_exception_stub_divide_error(void);
extern void arch_exception_stub_breakpoint(void);
extern void arch_exception_stub_invalid_opcode(void);
extern void arch_exception_stub_general_protection(void);
extern void arch_exception_stub_page_fault(void);

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

int arch_interrupt_init(void)
{
    arch_idtr_t idtr;

    hal_console_write("[interrupt] init begin\n");
    arch_idt_clear();

    if (arch_idt_install_exception_gates() != ARCH_INTERRUPT_OK) {
        hal_console_write("[interrupt] init failed: gate install\n");
        return ARCH_INTERRUPT_ERR_INVAL;
    }

    /*
     * IDTRのlimitはtable byte数そのものではなく「末尾offset」なので、
     * sizeof(table) - 1を設定する。baseは静的IDT tableの先頭addressである。
     */
    idtr.limit = (arch_u16_t)(sizeof(arch_idt) - 1U);
    idtr.base = (arch_u64_t)(arch_uptr_t)&arch_idt[0];
    arch_lidt(&idtr);

    arch_interrupt_initialized = 1;
    hal_console_write("[interrupt] idt initialized\n");
    hal_console_write("[interrupt] idt loaded\n");
    return ARCH_INTERRUPT_OK;
}

void arch_interrupt_trigger_validation_exception(void)
{
    if (!arch_interrupt_initialized) {
        hal_console_write("[interrupt] validation skipped: not initialized\n");
        return;
    }

    hal_console_write("[interrupt] validation trigger: int3\n");
    /*
     * int3はbreakpoint例外(vector 3)を同期的に発生させる。外部IRQやtimerを
     * 使わず、IDT登録とentry stub到達だけをQEMU serial logで確認する。
     */
    __asm__ volatile ("int3");
}
