/*
 * SPDX-License-Identifier: MIT
 */

/**
 * @file serial.h
 * @brief x86_64向けシリアル出力API定義（第2章2.2）
 *
 * @details
 * COM1を使用したシリアル出力のarch依存APIを宣言する。
 * このヘッダは arch(x86_64) 層に属し、HAL実装から利用される。
 * kernel共通部は第2章2.3のHAL境界により、このヘッダを直接参照しない。
 *
 * 層構造は kernel → HAL → arch(x86_64) → serial → COM1 である。
 */

#ifndef ITRON_RTOS_ARCH_X86_64_SERIAL_H
#define ITRON_RTOS_ARCH_X86_64_SERIAL_H

/**
 * @brief COM1シリアルポートを初期化する。
 *
 * @details
 * QEMUの `-serial stdio` で起動ログを確認できるよう、COM1の基本設定を行う。
 * 第2章2.2のシリアルAPIとして提供され、第2章2.3以降はHAL実装から呼び出される。
 *
 * @param なし。
 * @return なし。
 * @note kernel共通部からは直接呼ばず、HAL境界を経由する。
 */
void serial_init(void);

/**
 * @brief 1文字をCOM1へ送信する。
 *
 * @details
 * 改行文字は端末表示の読みやすさのためCRLFへ変換する。
 * QEMU `-serial stdio` の標準出力に文字を流す最下層の公開APIである。
 *
 * @param c 送信する文字。
 * @return なし。
 * @note 割り込み駆動ではなくポーリング送信を使う。
 */
void serial_putc(char c);

/**
 * @brief NULL終端文字列をCOM1へ送信する。
 *
 * @details
 * 文字列を1文字ずつ `serial_putc()` に渡す。
 * 第2章2.2ではprintfを導入せず、最小の文字列出力に限定している。
 *
 * @param message 送信するNULL終端文字列。NULLの場合は何もしない。
 * @return なし。
 * @note kernel共通部は第2章2.3のHAL console APIを通じて間接的に利用する。
 */
void serial_write(const char *message);

#endif
