/*
 * SPDX-License-Identifier: MIT
 */

/**
 * @file serial.c
 * @brief COM1シリアル出力実装（第5回）
 *
 * @details
 * x86_64のI/Oポート命令を使い、COM1へ文字を送信する。
 * QEMU `-serial stdio` はCOM1の出力をホスト標準入出力へ接続するため、
 * 最小カーネルの起動ログ確認に使う。
 *
 * このファイルは arch(x86_64) → serial → COM1 の層に位置する。
 * 第6回のHAL境界により、kernel共通部はこの実装を直接呼ばない。
 */

#include "serial.h"

#define COM1_PORT 0x3F8

/**
 * @brief I/Oポートへ1バイトを書き込む。
 *
 * @details
 * x86_64の `outb` 命令を薄く包む内部関数。
 * COM1レジスタの設定と送信に使う。
 *
 * @param port 書き込み先I/Oポート番号。
 * @param value 書き込む8ビット値。
 * @return なし。
 * @note arch依存処理なのでHALより下の層に閉じ込める。
 */
static void outb(unsigned short port, unsigned char value)
{
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

/**
 * @brief I/Oポートから1バイトを読み取る。
 *
 * @details
 * x86_64の `inb` 命令を薄く包む内部関数。
 * COM1のラインステータス確認に使う。
 *
 * @param port 読み取り元I/Oポート番号。
 * @return 読み取った8ビット値。
 * @note arch依存のI/O命令をserial層の内部に限定する。
 */
static unsigned char inb(unsigned short port)
{
    unsigned char value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

/**
 * @brief COM1シリアルポートを初期化する。
 *
 * @details
 * ボーレート、ライン制御、FIFO、モデム制御を設定する。
 * COM1を使う理由は、QEMU `-serial stdio` で最小構成のログ確認がしやすく、
 * 追加デバイスや割り込みなしでポーリング出力できるためである。
 *
 * @param なし。
 * @return なし。
 * @note 割り込み、タイマ、入力処理は追加しない。
 */
void serial_init(void)
{
    outb(COM1_PORT + 1, 0x00);
    outb(COM1_PORT + 3, 0x80);
    outb(COM1_PORT + 0, 0x03);
    outb(COM1_PORT + 1, 0x00);
    outb(COM1_PORT + 3, 0x03);
    outb(COM1_PORT + 2, 0xC7);
    outb(COM1_PORT + 4, 0x0B);
}

/**
 * @brief COM1が送信可能か確認する。
 *
 * @details
 * ラインステータスレジスタのTHR Emptyビットを確認する。
 * 割り込みを使わず、送信可能になるまで待つポーリング方式の根拠になる。
 *
 * @param なし。
 * @return 送信可能なら非0、まだ送信できなければ0。
 * @note 第5回の最小シリアル出力として、単純なポーリングに限定する。
 */
static int serial_can_transmit(void)
{
    return (inb(COM1_PORT + 5) & 0x20) != 0;
}

/**
 * @brief 変換なしで1文字をCOM1へ送信する。
 *
 * @details
 * 送信可能になるまで待ってからCOM1のデータレジスタへ書き込む。
 * 改行変換は上位の `serial_putc()` で行う。
 *
 * @param value 送信する文字。
 * @return なし。
 * @note busy waitを使うが、タイマや割り込みは導入しない。
 */
static void serial_write_raw_char(char value)
{
    while (!serial_can_transmit()) {
    }
    outb(COM1_PORT, (unsigned char)value);
}

/**
 * @brief 1文字をCOM1へ送信する。
 *
 * @details
 * `\n` を受け取った場合は先に `\r` を送信し、QEMU `-serial stdio` の
 * ターミナル表示で行頭が揃うようにする。
 *
 * @param c 送信する文字。
 * @return なし。
 * @note 文字出力だけを行い、シリアル入力やバッファリングは扱わない。
 */
void serial_putc(char c)
{
    if (c == '\n') {
        serial_write_raw_char('\r');
    }

    serial_write_raw_char(c);
}

/**
 * @brief NULL終端文字列をCOM1へ送信する。
 *
 * @details
 * NULLでなければ文字列終端まで1文字ずつ `serial_putc()` に渡す。
 * kernel → HAL → arch(x86_64) → serial → COM1 の最下層で実際の送信を行う。
 *
 * @param message 送信するNULL終端文字列。NULLの場合は何もしない。
 * @return なし。
 * @note printfやフォーマッタは導入せず、最小の文字列出力に限定する。
 */
void serial_write(const char *message)
{
    if (message == 0) {
        return;
    }

    while (*message != '\0') {
        serial_putc(*message);
        message++;
    }
}
