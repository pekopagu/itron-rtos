/*
 * SPDX-License-Identifier: MIT
 */

/**
 * @file pic.c
 * @brief x86_64 legacy PIC 初期化と IRQ mask 制御。
 *
 * @details
 * この module は第7章7.2の割り込みコントローラ初期化方針を実装する。
 * legacy PIC を CPU exception vector 0-31 と衝突しない vector 32/40 へ remap し、
 * 初期状態ではすべての IRQ を mask する。
 *
 * ここにある I/O port 操作は arch/x86_64 に閉じた実装詳細である。kernel 共通層は
 * HAL interrupt API だけを呼び、PIC port 番号や初期化 sequence を所有しない。
 *
 * この module はまだ timer interrupt を発火させない。PIT 設定、timer ISR、EOI、
 * `timer_tick()` 呼び出し、scheduler/dispatcher/preemption/context switch 連携は
 * 将来章へ残す。
 */

#include "pic.h"

#include "hal/console.h"

#define ARCH_PIC_MASTER_COMMAND 0x20U
#define ARCH_PIC_MASTER_DATA 0x21U
#define ARCH_PIC_SLAVE_COMMAND 0xA0U
#define ARCH_PIC_SLAVE_DATA 0xA1U
#define ARCH_PIC_IO_WAIT_PORT 0x80U

#define ARCH_PIC_MASTER_VECTOR_BASE 32U
#define ARCH_PIC_SLAVE_VECTOR_BASE 40U
#define ARCH_PIC_IRQ_COUNT 16U
#define ARCH_PIC_IRQS_PER_CHIP 8U

#define ARCH_PIC_ICW1_INIT 0x10U
#define ARCH_PIC_ICW1_EXPECT_ICW4 0x01U
#define ARCH_PIC_ICW4_8086_MODE 0x01U
#define ARCH_PIC_MASTER_HAS_SLAVE_ON_IRQ2 0x04U
#define ARCH_PIC_SLAVE_CASCADE_ID 0x02U
#define ARCH_PIC_ALL_MASKED 0xFFU
#define ARCH_PIC_EOI 0x20U

typedef unsigned char arch_pic_u8_t;
typedef unsigned short arch_pic_u16_t;

static arch_pic_u8_t pic_master_mask = ARCH_PIC_ALL_MASKED;
static arch_pic_u8_t pic_slave_mask = ARCH_PIC_ALL_MASKED;

/**
 * @brief x86_64 の I/O port へ 1 byte 書き込む。
 *
 * @details
 * `outb` 命令を PIC module 内に閉じるための最小 helper である。serial driver にも
 * 同種の処理はあるが、PIC の port 操作を serial 実装へ依存させないため、この module
 * 内に限定して持つ。
 *
 * @param port 書き込み先 I/O port。
 * @param value 書き込む 8 bit 値。
 */
static void arch_pic_outb(arch_pic_u16_t port, arch_pic_u8_t value)
{
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

/**
 * @brief PIC command sequence の間に短い I/O wait を入れる。
 *
 * @details
 * legacy I/O device 向けの小さな待ち合わせである。ここでは timing を精密に測る
 * 目的ではなく、初期化 command が連続しすぎないようにする教育用の明示点として置く。
 */
static void arch_pic_io_wait(void)
{
    arch_pic_outb((arch_pic_u16_t)ARCH_PIC_IO_WAIT_PORT, 0U);
}

/**
 * @brief 現在の mask mirror を master/slave PIC data port へ反映する。
 *
 * @details
 * mask state の source of truth は `pic_master_mask` と `pic_slave_mask` である。
 * この helper は mirror を実 hardware register 相当の data port へ書き込むだけで、
 * IRQ handler 登録や割り込み許可は行わない。
 */
static void arch_pic_write_masks(void)
{
    /*
     * mask mirrorをPICのdata portへ反映する。hardware registerを直接source of truthに
     * しないことで、mask/unmaskの意図をC側の状態更新として追跡しやすくする。
     */
    arch_pic_outb((arch_pic_u16_t)ARCH_PIC_MASTER_DATA, pic_master_mask);
    arch_pic_outb((arch_pic_u16_t)ARCH_PIC_SLAVE_DATA, pic_slave_mask);
}

/**
 * @brief IRQ 番号が PIC の管理範囲にあるか確認する。
 *
 * @param irq 確認対象の IRQ line。
 * @return 0-15 なら非0、範囲外なら0。
 */
static int arch_pic_irq_is_valid(unsigned int irq)
{
    return irq < ARCH_PIC_IRQ_COUNT;
}

/**
 * @brief legacy PICを学習用の初期IRQ配置へremapし、全IRQをmaskする。
 *
 * @details
 * master PICをvector 32、slave PICをvector 40へ移し、CPU exception vector 0-31と
 * 外部IRQの観測範囲を分離する。初期化後も全IRQをmaskしたままにし、通常bootの
 * smoke flowへhardware interruptが混入しないようにする。
 *
 * この処理は割り込みコントローラの準備だけを行う。PIT programming、timer ISR起動、
 * `timer_tick()`、scheduler、dispatcher、context switch、preemptionへの接続は行わない。
 *
 * @return 現段階では常に0。port I/O失敗検出はまだ扱わない。
 */
int arch_pic_init(void)
{
    hal_console_write("[pic] init begin\n");

    /*
     * 初期化 sequence 中に予期しない IRQ が届かないよう、まず全 line を mask する。
     * 第7章7.2ではこの mask 状態を最終状態としても維持する。
     */
    pic_master_mask = ARCH_PIC_ALL_MASKED;
    pic_slave_mask = ARCH_PIC_ALL_MASKED;
    arch_pic_write_masks();

    /*
     * legacy PIC の remap sequence。IRQ0 を vector 32 へ移すことで、CPU exception
     * vector 0-31 と IRQ vector の観測範囲を分離する。slave は master の IRQ2 に
     * cascade される構成だけを示し、SMP/APIC 系の配送モデルは扱わない。
     */
    /*
     * ICW1: master/slave PICを初期化モードへ入れる。
     * ここからICW4までの連続したcommand sequenceでremap設定を投入する。
     */
    arch_pic_outb(
        (arch_pic_u16_t)ARCH_PIC_MASTER_COMMAND,
        (arch_pic_u8_t)(ARCH_PIC_ICW1_INIT | ARCH_PIC_ICW1_EXPECT_ICW4)
    );
    arch_pic_io_wait();
    arch_pic_outb(
        (arch_pic_u16_t)ARCH_PIC_SLAVE_COMMAND,
        (arch_pic_u8_t)(ARCH_PIC_ICW1_INIT | ARCH_PIC_ICW1_EXPECT_ICW4)
    );
    arch_pic_io_wait();

    /*
     * ICW2: IRQ vector baseを設定する。masterを32に置くことで、
     * CPU exception 0-31とIRQ0以降をserial log上でも区別しやすくする。
     */
    arch_pic_outb((arch_pic_u16_t)ARCH_PIC_MASTER_DATA, ARCH_PIC_MASTER_VECTOR_BASE);
    arch_pic_io_wait();
    arch_pic_outb((arch_pic_u16_t)ARCH_PIC_SLAVE_DATA, ARCH_PIC_SLAVE_VECTOR_BASE);
    arch_pic_io_wait();

    /*
     * ICW3: master/slaveのcascade関係を明示する。
     * legacy PIC構成ではslaveがmaster IRQ2に接続される前提だけを扱う。
     */
    arch_pic_outb((arch_pic_u16_t)ARCH_PIC_MASTER_DATA, ARCH_PIC_MASTER_HAS_SLAVE_ON_IRQ2);
    arch_pic_io_wait();
    arch_pic_outb((arch_pic_u16_t)ARCH_PIC_SLAVE_DATA, ARCH_PIC_SLAVE_CASCADE_ID);
    arch_pic_io_wait();

    /*
     * ICW4: x86向けの8086/88 modeを選ぶ。ここではlegacy PICの最小利用に留め、
     * APIC/IOAPIC/LAPICや高度な割り込み配送modelは導入しない。
     */
    arch_pic_outb((arch_pic_u16_t)ARCH_PIC_MASTER_DATA, ARCH_PIC_ICW4_8086_MODE);
    arch_pic_io_wait();
    arch_pic_outb((arch_pic_u16_t)ARCH_PIC_SLAVE_DATA, ARCH_PIC_ICW4_8086_MODE);
    arch_pic_io_wait();

    /*
     * remap完了後も全IRQ maskedを維持する。第7章7.4では通常bootでtimer IRQ観測logを
     * 出さないことが重要なので、validation入口だけが必要なIRQを個別にunmaskする。
     */
    arch_pic_write_masks();

    hal_console_write("[pic] init done: master_base=32 slave_base=40 irqs=masked\n");
    return 0;
}

/**
 * @brief 指定したlegacy PIC IRQ lineをmaskする。
 *
 * @details
 * C側のmask mirrorを更新してからPIC data portへ反映する。範囲外IRQは無視し、
 * 既存のmask状態を変えない。master/slaveのどちらに属するかだけをここで判定し、
 * port番号の詳細はこのmodule内へ閉じ込める。
 *
 * @param irq maskするIRQ line。0-15のみ有効。
 */
void arch_pic_mask_irq(unsigned int irq)
{
    arch_pic_u8_t bit;

    if (!arch_pic_irq_is_valid(irq)) {
        /*
         * 範囲外IRQを呼び出し側のエラーとして扱わず、状態を変えずに戻る。
         * 学習用の最小APIとして、異常系logやpanicはまだ導入しない。
         */
        return;
    }

    if (irq < ARCH_PIC_IRQS_PER_CHIP) {
        /*
         * IRQ0-7はmaster PICに属する。該当bitを1にして配送を止める。
         */
        bit = (arch_pic_u8_t)(1U << irq);
        pic_master_mask = (arch_pic_u8_t)(pic_master_mask | bit);
    } else {
        /*
         * IRQ8-15はslave PICに属する。slave側のbit位置へ変換してmaskする。
         */
        bit = (arch_pic_u8_t)(1U << (irq - ARCH_PIC_IRQS_PER_CHIP));
        pic_slave_mask = (arch_pic_u8_t)(pic_slave_mask | bit);
    }

    /*
     * mirror更新後にhardwareへ反映する。mask変更はこの1箇所に集約する。
     */
    arch_pic_write_masks();
}

/**
 * @brief 指定したlegacy PIC IRQ lineをunmaskする。
 *
 * @details
 * 明示validationなど、限定された観測で必要なIRQだけを開くためのAPIである。
 * 範囲外IRQは無視し、既存のmask状態を変えない。通常bootではIRQ0をunmaskしないため、
 * timer IRQ観測は `VALIDATE_TIMER_IRQ_ENTRY=1` の経路に限定される。
 *
 * @param irq unmaskするIRQ line。0-15のみ有効。
 */
void arch_pic_unmask_irq(unsigned int irq)
{
    arch_pic_u8_t bit;

    if (!arch_pic_irq_is_valid(irq)) {
        /*
         * 無効なIRQ番号ではmask mirrorにもhardwareにも触れない。
         * これにより誤った呼び出しが別IRQの配送状態を壊さない。
         */
        return;
    }

    if (irq < ARCH_PIC_IRQS_PER_CHIP) {
        /*
         * IRQ0-7はmaster PIC側のbitを0にして配送を許可する。
         */
        bit = (arch_pic_u8_t)(1U << irq);
        pic_master_mask = (arch_pic_u8_t)(pic_master_mask & (arch_pic_u8_t)~bit);
    } else {
        /*
         * IRQ8-15はslave PIC側のbitを0にする。master IRQ2の扱いは
         * この章では高度化せず、legacy PICの最小観測に留める。
         */
        bit = (arch_pic_u8_t)(1U << (irq - ARCH_PIC_IRQS_PER_CHIP));
        pic_slave_mask = (arch_pic_u8_t)(pic_slave_mask & (arch_pic_u8_t)~bit);
    }

    /*
     * 更新したmirrorをPICへ書き戻し、以降のIRQ配送可否に反映する。
     */
    arch_pic_write_masks();
}

/**
 * @brief 指定したlegacy PIC IRQ lineの処理完了をPICへ通知する。
 *
 * @details
 * IRQ handlerが最小観測を終えた後に呼ぶarch-local APIである。EOIは割り込み配送の
 * 完了通知であり、mask mirrorを変更しない。IRQ8-15ではslaveへ通知してから
 * cascade元のmasterへ通知する。IRQ0のtimer entry validationではmaster PICへだけ
 * EOIを送る。
 *
 * @param irq EOIを送るIRQ line。0-15のみ有効。
 */
void arch_pic_send_eoi(unsigned int irq)
{
    if (!arch_pic_irq_is_valid(irq)) {
        /*
         * 無効なIRQ番号ではEOIを送らない。誤ったcommand port書き込みを避け、
         * unrelatedなPIC状態を変えないためである。
         */
        return;
    }

    /*
     * slave側IRQはcascade元のmaster IRQ2にも完了通知が必要である。
     * mask mirrorは割り込み許可状態のsource of truthなので、EOIでは変更しない。
     */
    if (irq >= ARCH_PIC_IRQS_PER_CHIP) {
        /*
         * slave IRQでは先にslave PICへEOIを送る。未完了状態をslave側に残したまま
         * masterだけを完了扱いにしないための順序である。
         */
        arch_pic_outb((arch_pic_u16_t)ARCH_PIC_SLAVE_COMMAND, ARCH_PIC_EOI);
    }

    /*
     * master PICへEOIを送る。IRQ0-7ではこれだけで完了し、IRQ8-15ではcascade元の
     * master IRQ2に対する完了通知にもなる。
     */
    arch_pic_outb((arch_pic_u16_t)ARCH_PIC_MASTER_COMMAND, ARCH_PIC_EOI);
}
