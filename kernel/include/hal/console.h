/*
 * SPDX-License-Identifier: MIT
 */

/**
 * @file console.h
 * @brief kernel共通部向けHAL console API（第2章2.3）
 *
 * @details
 * kernelがログ出力を行うための抽象境界を定義する。
 * kernel共通部はこのヘッダだけを使い、arch(x86_64)やserial実装を直接呼ばない。
 *
 * 層構造は kernel → HAL → arch(x86_64) → serial → COM1 である。
 */

#ifndef ITRON_RTOS_HAL_CONSOLE_H
#define ITRON_RTOS_HAL_CONSOLE_H

/**
 * @brief console出力を初期化する。
 *
 * @details
 * 実際の初期化処理は各archのHAL実装へ委譲される。
 *
 * @param なし。
 * @return なし。
 * @note kernelはserial初期化を直接呼ばず、この関数を使う。
 */
void hal_console_init(void);

/**
 * @brief consoleへ1文字を出力する。
 *
 * @details
 * 実際の送信先はHAL実装が選択する。x86_64ではCOM1へ出力される。
 *
 * @param c 出力する文字。
 * @return なし。
 * @note arch依存のI/O命令をkernelから隠すためのAPIである。
 */
void hal_console_putc(char c);

/**
 * @brief consoleへ文字列を出力する。
 *
 * @details
 * kernel起動ログとタスク管理ログはこのAPIを通じて出力される。
 *
 * @param message 出力するNULL終端文字列。
 * @return なし。
 * @note 第2章2.3 HAL境界の中心となる文字列出力APIである。
 */
void hal_console_write(const char *message);

#endif
