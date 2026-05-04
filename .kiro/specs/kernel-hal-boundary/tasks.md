# Implementation Plan

- [x] 1. 現状依存を確認する
  - 目的: kernel 共通部が現在 x86_64 serial header に直接依存していることを実装前に確認する。
  - 対象ファイル: `kernel/kernel.c`, `arch/x86_64/serial.h`, `arch/x86_64/serial.c`
  - 作業内容: `kernel/kernel.c` の include と `serial_init` / `serial_putc` / `serial_write` の利用可能性を確認する。
  - 完了条件: `kernel/kernel.c` の直接依存箇所と、既存 serial API の宣言・実装位置が明確になっている。
  - 注意点: このタスクではファイルを変更しない。serial の COM1、ポーリング送信、改行変換ロジックを変更しない。
  - _Requirements: 1.1, 2.4, 2.5, 2.6, 3.5_

- [x] 2. HAL console インターフェースヘッダを追加する
  - 目的: kernel 共通部が参照する console 用 HAL 契約を追加する。
  - 対象ファイル: `kernel/include/hal/console.h`
  - 作業内容: `kernel/include/hal/console.h` を作成し、`hal_console_init` / `hal_console_putc` / `hal_console_write` を宣言する。
  - 完了条件: header に `SPDX-License-Identifier: MIT`、include guard、3つの HAL API 宣言が存在する。
  - 注意点: この header は `arch/x86_64/serial.h`、COM1、port I/O、`ARCH_X86_64` を公開しない。
  - _Requirements: 1.2, 1.3, 1.4, 2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 6.2_
  - _Boundary: HAL Console Contract_

- [x] 3. x86_64 HAL console 実装を追加する
  - 目的: x86_64 側で HAL console API を既存 serial API へ接続する。
  - 対象ファイル: `arch/x86_64/hal_console.c`, `arch/x86_64/serial.h`
  - 作業内容: `arch/x86_64/hal_console.c` を作成し、`hal_console_init` は `serial_init`、`hal_console_putc` は `serial_putc`、`hal_console_write` は `serial_write` へ委譲する。
  - 完了条件: HAL 実装ファイルに `SPDX-License-Identifier: MIT` があり、`hal_console_*` が `serial_*` へ単純委譲している。
  - 注意点: HAL 側では改行変換、NULL 判定、buffering、formatting、log level を追加しない。`arch/x86_64/serial.c` のロジックは変更しない。
  - _Requirements: 1.3, 1.4, 3.1, 3.2, 3.3, 3.4, 3.5, 6.2, 6.6_
  - _Boundary: x86_64 HAL Console_

- [x] 4. `kernel_main` を HAL 経由の console 出力に変更する
  - 目的: kernel 共通部から x86_64 serial header への直接依存をなくす。
  - 対象ファイル: `kernel/kernel.c`
  - 作業内容: `../arch/x86_64/serial.h` の include を削除し、`hal/console.h` を include する。`serial_init` を `hal_console_init`、`serial_write` を `hal_console_write` に置き換える。
  - 完了条件: `kernel/kernel.c` が `hal_console_init()` と `hal_console_write()` を使い、出力文言は `itron-rtos booting...\n` と `kernel_main reached\n` のまま維持されている。
  - 注意点: kernel 側に `serial_*`、COM1、port I/O、`ARCH_X86_64` を残さない。既存の `hlt` loop は変更しない。
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 4.1, 4.2, 4.3, 4.4, 4.5_
  - _Boundary: Kernel Boot Caller_

- [x] 5. Makefile を最小変更で HAL 実装に対応させる
  - 目的: 追加した HAL 実装を既存 kernel image に link する。
  - 対象ファイル: `Makefile`
  - 作業内容: `arch/x86_64/hal_console.c` の object を build 対象に追加し、`hal/console.h` を include できるように必要なら `kernel/include` を include path に追加する。
  - 完了条件: `make` が HAL object を compile/link 対象に含め、既存の boot/kernel/serial object の build rule が維持されている。
  - 注意点: `boot/boot.asm` と `linker.ld` の扱いを変更しない。Makefile 変更は include path、object、依存関係、compile rule に限定する。
  - _Requirements: 2.4, 2.5, 2.6, 5.1, 5.2, 6.7, 7.1, 7.2_
  - _Boundary: Build Integration_

- [x] 6. ビルドを確認する
  - 目的: HAL 境界導入後も kernel image が生成できることを確認する。
  - 対象ファイル: `Makefile`, `build/kernel.elf`
  - 作業内容: `make clean` があれば実行し、続けて `make` を実行する。`build/kernel.elf` の生成を確認する。
  - 完了条件: `make` が成功し、`build/kernel.elf` が存在する。
  - 注意点: `build/` は生成物として扱い、ソース変更対象にしない。ビルド失敗時は HAL include path と object link を優先して確認する。
  - _Requirements: 5.1, 5.2, 7.6_
  - _Boundary: Build Integration_

- [x] 7. QEMU serial 出力を確認する
  - 目的: 第5回と同等の起動ログが HAL 経由でも観測できることを確認する。
  - 対象ファイル: `build/kernel.elf`, `docs/logs/qemu-serial.log`
  - 作業内容: `qemu-system-x86_64 -kernel build/kernel.elf -serial stdio` 相当で起動し、serial output を確認する。
  - 完了条件: 出力に `itron-rtos booting...` と `kernel_main reached` が含まれている。
  - 注意点: 出力内容は第5回と同等に保つ。大量ログや環境依存情報を残さない。
  - _Requirements: 3.4, 5.3, 5.4, 5.5, 5.6_
  - _Boundary: Runtime Validation_

- [x] 8. README に HAL 境界の概要を追記する
  - 目的: 第6回で kernel と arch の console 境界を整理したことを簡潔に記録する。
  - 対象ファイル: `README.md`
  - 作業内容: HAL 導入の目的と、kernel が arch serial header を直接参照しない構成になったことを追記する。
  - 完了条件: README に console HAL 境界の概要があり、QEMU 実行手順は実装と矛盾していない。
  - 注意点: QEMU 実行手順と出力例は必要がなければ変更しない。HAL の範囲を console のみに限定して説明する。
  - _Requirements: 6.1, 6.2, 6.3, 6.4, 6.5, 6.6_
  - _Boundary: Documentation_

- [x] 9. QEMU serial log の更新要否を判断する
  - 目的: 第6回の確認結果を必要最小限のログ evidence として扱う。
  - 対象ファイル: `docs/logs/qemu-serial.log`
  - 作業内容: QEMU 出力が第5回と同等なら不要な更新を避け、記録が必要な場合だけ関連する2行を最小限で反映する。
  - 完了条件: `docs/logs/qemu-serial.log` が更新不要と判断されるか、`itron-rtos booting...` と `kernel_main reached` に限定された簡潔な内容になっている。
  - 注意点: 環境依存情報、大量ログ、無関係な実行ログを残さない。
  - _Requirements: 5.4, 5.5, 5.6_
  - _Boundary: Runtime Validation_

- [x] 10. 最終レビューを行う
  - 目的: requirements と design の境界条件を満たしていることを実装後に確認する。
  - 対象ファイル: `kernel/kernel.c`, `kernel/include/hal/console.h`, `arch/x86_64/hal_console.c`, `arch/x86_64/serial.c`, `arch/x86_64/serial.h`, `Makefile`, `README.md`, `docs/logs/qemu-serial.log`
  - 作業内容: `kernel/kernel.c` に `arch/x86_64/serial.h`、`serial_*`、COM1、port I/O、`ARCH_X86_64` が残っていないことを確認する。`hal_console_*` が `serial_*` に委譲していることを確認する。
  - 完了条件: `git diff` で `boot/boot.asm`、`linker.ld`、割り込み、タイマ、タスク管理、スケジューラ、対象外 directory に変更がないことを確認し、受け入れ条件を満たしている。
  - 注意点: serial ロジックの変更、console 以外の HAL 追加、`printf` や log level の追加がないことを明示的に確認する。
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 3.1, 3.2, 3.3, 3.4, 3.5, 4.1, 4.2, 4.3, 4.4, 4.5, 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 6.1, 6.2, 6.3, 6.4, 6.5, 6.6, 6.7, 7.1, 7.2, 7.3, 7.4, 7.5, 7.6_
  - _Boundary: Scope Guard_
