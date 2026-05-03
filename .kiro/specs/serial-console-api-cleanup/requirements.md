# Requirements Document

## Introduction
この feature は、第5回の一部として既存の COM1 シリアル出力 API を整理するための要求を定義する。第4回で x86_64 + QEMU の最小カーネル起動と、`kernel_main` からの COM1 経由シリアル出力は動作済みである。

現在の公開 API は `serial_write_string` のみであり、今後の RTOS 開発におけるデバッグ出力としては初期化、1文字出力、文字列出力の責務が見えにくい。この feature では、既存のシリアル出力を壊さず、最小限で扱いやすいシリアルコンソール API へ整理する。

## Boundary Context
- **In scope**: `serial_init`、`serial_putc`、`serial_write` の公開 API 化、改行コードの端末向け出力整理、`kernel_main` の起動ログ整理、README のシリアル出力確認手順更新、QEMU 上でのシリアルログ確認。
- **Out of scope**: 割り込み駆動 UART、シリアル入力、リングバッファ、`printf`、ログレベル、可変長フォーマット出力、HAL 完全抽象化、タスク管理、タイマ、スケジューラ、μITRON 風 API。
- **Adjacent expectations**: `x86_64-qemu-minimal-boot` と `source-spdx-license-identifiers` が完了していることを前提とし、最小カーネル起動、COM1 ポート、ポーリング送信方式、既存ライセンス表記を維持する。

## Requirements

### Requirement 1: シリアルコンソール最小APIの提供
**Objective:** As a カーネル開発者, I want シリアル初期化、1文字出力、文字列出力を明示的に呼び出せる, so that 今後のデバッグ出力を小さく一貫した API で扱える

#### Acceptance Criteria
1. The serial console API cleanup shall expose `serial_init` as a public serial console API.
2. The serial console API cleanup shall expose `serial_putc` as a public serial console API.
3. The serial console API cleanup shall expose `serial_write` as a public serial console API.
4. The serial console API cleanup shall make `serial_init`, `serial_putc`, and `serial_write` available from the public serial header.
5. The serial console API cleanup shall not require callers to use `serial_write_string` as the primary string output API.

### Requirement 2: 文字列出力と改行の扱い
**Objective:** As a カーネル開発者, I want 文字列出力が NULL と改行を安全に扱う, so that デバッグログ出力で不要なクラッシュや端末表示崩れを避けられる

#### Acceptance Criteria
1. If `serial_write` receives `NULL`, then the serial console API cleanup shall avoid crashing the kernel.
2. When `serial_write` receives a string containing `\n`, the serial console API cleanup shall output it as `\r\n` for serial terminal readability.
3. When `serial_write` receives a string without `\n`, the serial console API cleanup shall output the string content in order.
4. When `serial_putc` outputs a single character, the serial console API cleanup shall preserve the existing serial transmission behavior.
5. The serial console API cleanup shall define the compatibility treatment of `serial_write_string` during design, either as removal or as a thin compatibility wrapper around `serial_write`.

### Requirement 3: 起動ログ出力の整理
**Objective:** As a 開発者, I want 起動時ログが整理されたシリアル API で出力される, so that QEMU 起動確認時にカーネル到達点を明確に確認できる

#### Acceptance Criteria
1. When `kernel_main` starts, the serial console API cleanup shall initialize the serial console through `serial_init`.
2. When `kernel_main` emits boot logs, the serial console API cleanup shall use `serial_write` instead of `serial_write_string`.
3. When the kernel boots, the serial console API cleanup shall output at least `itron-rtos booting...`.
4. When the kernel reaches `kernel_main`, the serial console API cleanup shall output at least `kernel_main reached`.
5. While the kernel remains in its existing halt loop, the serial console API cleanup shall preserve the existing minimal boot behavior.

### Requirement 4: 既存COM1シリアル動作の維持
**Objective:** As a メンテナ, I want 第4回で動作した COM1 シリアル出力を維持する, so that API 整理後も既存の QEMU 確認手順が壊れない

#### Acceptance Criteria
1. The serial console API cleanup shall continue to use COM1 port `0x3F8` for serial output.
2. The serial console API cleanup shall continue to use polling transmission for serial output.
3. The serial console API cleanup shall preserve the existing COM1 initialization policy.
4. The serial console API cleanup shall not introduce interrupt-driven UART behavior.
5. The serial console API cleanup shall not introduce serial input behavior.
6. The serial console API cleanup shall not introduce ring buffer, timer, scheduler, task management, or μITRON-style API behavior.

### Requirement 5: ビルドとQEMU確認
**Objective:** As a レビューア, I want API 整理後もビルドと QEMU 上のシリアルログを確認できる, so that 変更が最小カーネル起動を壊していないことを判断できる

#### Acceptance Criteria
1. When a developer runs `make`, the serial console API cleanup shall build successfully.
2. When `make` succeeds, the serial console API cleanup shall produce `build/kernel.elf`.
3. When the kernel is run on QEMU with `-serial stdio`, the serial console API cleanup shall make the boot log observable on the serial output.
4. When QEMU serial output is captured, the serial console API cleanup shall include `itron-rtos booting...` in the output log.
5. When QEMU serial output is captured, the serial console API cleanup shall include `kernel_main reached` in the output log.
6. When QEMU serial output is displayed in a terminal, the serial console API cleanup shall keep line breaks readable.

### Requirement 6: READMEと変更範囲の限定
**Objective:** As a メンテナ, I want 確認手順と変更範囲が明確に保たれる, so that API 整理が周辺機能の実装変更に広がらない

#### Acceptance Criteria
1. The serial console API cleanup shall document or update the README instructions for verifying serial output.
2. The serial console API cleanup shall keep changes focused on serial API cleanup, boot log output, README verification instructions, and serial log evidence.
3. The serial console API cleanup shall not change `boot/boot.asm`.
4. The serial console API cleanup shall not change `linker.ld`.
5. The serial console API cleanup shall not change `Makefile`.
6. The serial console API cleanup shall not change `LICENSE`, `.gitignore`, `.agents/`, `.codex/`, `.kiro/settings/`, `articles/`, `prompts/`, `checkList/`, or `build/`.

