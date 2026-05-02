# Implementation Plan

- [x] 1. ディレクトリと空の保持ファイルを作成する
  - 目的: 最小起動基盤のファイル配置先とログ保存先を先に用意する。
  - 変更対象ファイル: `boot/`, `kernel/`, `arch/x86_64/`, `docs/logs/.gitkeep`, `.gitignore`
  - 完了条件: `boot/`, `kernel/`, `arch/x86_64/`, `docs/logs/` が存在し、`build/` が生成物用として git 管理対象外になる。
  - 依存関係: なし。
  - 境界: File Structure Plan, Run Log
  - _Requirements: 2.4, 5.1_

- [x] 2. boot コードを作成する
  - 目的: QEMU から開始される最小起動入口を用意し、C の `kernel_main` へ制御を渡す。
  - 変更対象ファイル: `boot/boot.asm`
  - 完了条件: `boot/boot.asm` に entry symbol が定義され、`kernel_main` を呼び出す最小 boot 経路が存在する。
  - 依存関係: 1。
  - 境界: Boot Entry
  - _Requirements: 2.1, 3.2, 6.2, 6.3, 6.4_

- [x] 3. `kernel_main` とシリアル出力を作成する
  - 目的: freestanding C の最小入口から `kernel_main reached` などの到達確認文字列を出力する。
  - 変更対象ファイル: `kernel/kernel.c`, `arch/x86_64/serial.c`, `arch/x86_64/serial.h`
  - 完了条件: `kernel_main` が存在し、標準ライブラリ、動的メモリ、割り込み、タイマに依存せずにシリアル出力へ到達確認文字列を送れる。
  - 依存関係: 1。
  - 境界: Kernel Main, Serial Driver
  - _Requirements: 2.2, 3.3, 5.2, 6.1, 6.3, 6.4, 6.5, 6.6_

- [x] 4. linker script を作成する
  - 目的: `kernel.elf` の entry point と section 配置を定義し、boot と C カーネルを1つのELFにまとめられるようにする。
  - 変更対象ファイル: `linker.ld`
  - 完了条件: LLD が参照できる linker script が存在し、boot entry と `kernel_main` を含む object を `kernel.elf` へリンクできる設計になっている。
  - 依存関係: 2, 3。
  - 境界: Linker Script
  - _Requirements: 2.3, 2.5_

- [x] 5. Makefile を作成する
  - 目的: Windows + PowerShell から GNU Make で build、run、clean を実行できる入口を用意する。
  - 変更対象ファイル: `Makefile`
  - 完了条件: `make` が NASM、Clang、LLD を順に呼び出して `build/kernel.elf` を生成するターゲットを持ち、`make run` と `make clean` のターゲットも存在する。
  - 依存関係: 2, 3, 4。
  - 境界: Makefile
  - _Requirements: 1.1, 1.2, 1.3, 2.1, 2.2, 2.3, 2.4, 2.5, 3.1, 5.1_

- [x] 6. ビルド確認を行う
  - 目的: Makefile と toolchain の連携で `build/kernel.elf` が生成されることを確認する。
  - 変更対象ファイル: `build/`, 必要に応じて `Makefile`, `boot/boot.asm`, `kernel/kernel.c`, `arch/x86_64/serial.c`, `arch/x86_64/serial.h`, `linker.ld`
  - 完了条件: Windows + PowerShell から `make` が成功し、`build/kernel.elf` が存在する。
  - 依存関係: 5。
  - 境界: Build Artifacts, Makefile
  - _Requirements: 1.1, 1.2, 1.3, 2.1, 2.2, 2.3, 2.4, 2.5_

- [x] 7. QEMU 起動確認を行う
  - 目的: `build/kernel.elf` を `qemu-system-x86_64` で起動し、boot から `kernel_main` まで到達することを確認する。
  - 変更対象ファイル: 必要に応じて `Makefile`, `boot/boot.asm`, `kernel/kernel.c`, `arch/x86_64/serial.c`, `linker.ld`
  - 完了条件: `make run` または README に記載予定の QEMU コマンドで起動し、`kernel_main reached` などの文字列が画面表示またはシリアル出力で確認できる。
  - 依存関係: 6。
  - 境界: Makefile, Boot Entry, Kernel Main, Serial Driver
  - _Requirements: 3.1, 3.2, 3.3, 3.4, 5.2_

- [x] 8. README を更新する
  - 目的: 開発者がビルド、起動、ログ確認を再現できる手順を README に残す。
  - 変更対象ファイル: `README.md`
  - 完了条件: README に学習・実験目的、Windows + PowerShell での `make` 手順、QEMU 実行手順、`docs/logs/` のログ確認手順が記載されている。
  - 依存関係: 6, 7。
  - 境界: README
  - _Requirements: 4.1, 4.2, 4.3, 4.4, 5.3_

- [x] 9. `docs/logs/` に実行確認ログを保存する
  - 目的: QEMU 起動確認の結果を後から確認できる証跡として保存する。
  - 変更対象ファイル: `docs/logs/`
  - 完了条件: `docs/logs/` 以下に実行確認ログが保存され、ログ内で `kernel_main reached` などの到達確認文字列を確認できる。
  - 依存関係: 7, 8。
  - 境界: Run Log
  - _Requirements: 5.1, 5.2, 5.3_

- [x] 10. 最終レビューを行う
  - 目的: 要求、設計、実装、ログ、README が第4回の最小起動スコープに収まっていることを確認する。
  - 変更対象ファイル: 必要に応じて `boot/boot.asm`, `kernel/kernel.c`, `arch/x86_64/serial.c`, `arch/x86_64/serial.h`, `linker.ld`, `Makefile`, `README.md`, `docs/logs/`
  - 完了条件: `make` 成功、`build/kernel.elf` 生成、QEMU 起動、到達文字列確認、README 手順、実行ログ保存が確認され、タスク生成、コンテキストスイッチ、割り込み、タイマ、セマフォ、μITRON 風 API が含まれていない。
  - 依存関係: 9。
  - 境界: Scope Boundary, Build Artifacts, README, Run Log
  - _Requirements: 1.1, 1.2, 2.5, 3.1, 3.2, 3.3, 4.1, 4.2, 4.3, 4.4, 5.1, 5.2, 5.3, 6.1, 6.2, 6.3, 6.4, 6.5, 6.6_
