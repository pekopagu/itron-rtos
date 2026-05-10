/*
 * SPDX-License-Identifier: MIT
 */

/**
 * @file hal_console.c
 * @brief x86_64向けHAL console実装（第2章2.3）
 *
 * @details
 * kernel共通部がarch依存のserial実装を直接呼ばないよう、
 * HAL console APIをx86_64のCOM1シリアルAPIへ接続する。
 * これにより、kernel → HAL → arch(x86_64) → serial → COM1 の依存方向を保つ。
 */

#include "hal/console.h"
#include "serial.h"

/**
 * @brief HAL consoleを初期化する。
 *
 * @details
 * x86_64実装ではCOM1シリアル初期化へ委譲する。
 * kernelはこのHAL APIだけを呼び、arch依存コードを直接参照しない。
 *
 * @param なし。
 * @return なし。
 * @note 第2章2.3のHAL境界により、移植時はこの層を差し替える。
 */
void hal_console_init(void)
{
    /*
     * kernel共通部から見える初期化入口はHALに限定する。
     * 現在のx86_64実装ではCOM1 serialを初期化するだけだが、移植時はここを差し替える。
     */
    serial_init();
}

/**
 * @brief HAL consoleへ1文字を出力する。
 *
 * @details
 * x86_64実装ではCOM1シリアルの1文字送信へ委譲する。
 * kernel共通部から見たI/O境界をこの関数に集約する。
 *
 * @param c 出力する文字。
 * @return なし。
 * @note 割り込みやタイマを使わず、serial層のポーリング出力に従う。
 */
void hal_console_putc(char c)
{
    /*
     * 1文字出力もHAL境界を経由させることで、kernel側がserialのI/O port詳細へ
     * 依存しないようにする。
     */
    serial_putc(c);
}

/**
 * @brief HAL consoleへ文字列を出力する。
 *
 * @details
 * x86_64実装ではCOM1シリアルの文字列出力へ委譲する。
 * kernelのログ出力はこのAPIを経由し、serial実装への直接依存を避ける。
 *
 * @param message 出力するNULL終端文字列。NULL時の扱いはserial層に従う。
 * @return なし。
 * @note 第3章3.1のタスク管理ログもこのHAL境界を通る。
 */
void hal_console_write(const char *message)
{
    /*
     * 文字列出力をserial実装へ委譲する。
     * NULL処理や改行処理はserial層に集約し、HALは依存方向だけを保つ。
     */
    serial_write(message);
}
