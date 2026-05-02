# Requirements Document

## Introduction
この仕様は、学習・実験目的の μITRON 風 RTOS プロジェクトにおける第4回 feature として、x86_64 + QEMU 上で最小カーネルを起動し、C 言語の `kernel_main` から文字出力できる状態を定義する。現時点では boot 処理、freestanding C カーネル、linker script、Makefile、QEMU 起動手順、実行ログ保存までを対象とし、RTOS 機能は実装しない。

## Boundary Context
- **In scope**: Windows + PowerShell での `make` 実行、NASM による起動用アセンブリのビルド、Clang による freestanding C カーネルのビルド、LLD による `kernel.elf` 生成、QEMU による起動確認、`kernel_main` 到達を示す文字出力、`build/` への成果物出力、README 手順追記、`docs/logs/` への実行確認ログ保存。
- **Out of scope**: タスク生成、コンテキストスイッチ、割り込み、タイマ、セマフォ、イベントフラグ、μITRON 風 API、実 ITRON/T-Kernel コードの参照・流用、x86_64 以外のアーキテクチャ対応。
- **Adjacent expectations**: この feature は、後続のタスク管理、スケジューラ、割り込み、タイマ、同期機能、μITRON 風 API 実装の前提となる最小起動基盤を提供するが、それらの RTOS 機能自体の振る舞いは定義しない。

## Requirements

### Requirement 1: Windows PowerShell でのビルド実行
**Objective:** As a 開発者, I want Windows + PowerShell から `make` を実行できる, so that 最小カーネルを一貫した手順でビルドできる

#### Acceptance Criteria
1. When 開発者が Windows + PowerShell で `make` を実行する, the x86_64 QEMU minimal boot feature shall ビルド処理を開始する
2. When `make` によるビルドが完了する, the x86_64 QEMU minimal boot feature shall 成功終了として確認できる状態になる
3. If 必要なビルド入力が不足している, the x86_64 QEMU minimal boot feature shall 失敗したことを開発者が確認できる状態にする

### Requirement 2: 最小カーネルのビルド成果物生成
**Objective:** As a 開発者, I want 起動用アセンブリと freestanding C カーネルから `kernel.elf` を生成できる, so that QEMU で起動できる最小カーネルを得られる

#### Acceptance Criteria
1. When `make` が起動用アセンブリをビルドする, the x86_64 QEMU minimal boot feature shall NASM を使ったビルド結果を生成する
2. When `make` が C カーネルをビルドする, the x86_64 QEMU minimal boot feature shall Clang を使った freestanding C カーネルのビルド結果を生成する
3. When `make` がリンク処理を実行する, the x86_64 QEMU minimal boot feature shall LLD により `kernel.elf` を生成する
4. When ビルド成果物が生成される, the x86_64 QEMU minimal boot feature shall 成果物を `build/` 以下に出力する
5. When ビルドが成功する, the x86_64 QEMU minimal boot feature shall `build/kernel.elf` が存在する状態にする

### Requirement 3: QEMU による最小カーネル起動確認
**Objective:** As a 開発者, I want `kernel.elf` を QEMU で起動できる, so that boot 処理から `kernel_main` まで到達したことを確認できる

#### Acceptance Criteria
1. When 開発者が QEMU 起動手順を実行する, the x86_64 QEMU minimal boot feature shall `qemu-system-x86_64` で `build/kernel.elf` を起動できる状態にする
2. When QEMU 上で最小カーネルが起動する, the x86_64 QEMU minimal boot feature shall boot 処理から C 言語の `kernel_main` へ制御が到達する状態にする
3. When `kernel_main` に到達する, the x86_64 QEMU minimal boot feature shall 画面表示またはシリアル出力で `kernel_main reached` などの到達確認文字列を表示する
4. If QEMU 起動手順が実行できない, the x86_64 QEMU minimal boot feature shall 起動確認が未完了であることを開発者が判断できる状態にする

### Requirement 4: README による手順の明示
**Objective:** As a 開発者, I want README でビルド手順と実行手順を確認できる, so that 同じ環境で最小起動確認を再現できる

#### Acceptance Criteria
1. The x86_64 QEMU minimal boot feature shall README に学習・実験目的のプロジェクトであることを明記する
2. The x86_64 QEMU minimal boot feature shall README に Windows + PowerShell でのビルド手順を記載する
3. The x86_64 QEMU minimal boot feature shall README に QEMU での実行手順を記載する
4. The x86_64 QEMU minimal boot feature shall README に実行確認ログの保存先を記載する

### Requirement 5: 実行確認ログの保存
**Objective:** As a 開発者, I want QEMU 起動確認のログを保存できる, so that `kernel_main` 到達を後から確認できる

#### Acceptance Criteria
1. When 開発者が実行確認を行う, the x86_64 QEMU minimal boot feature shall `docs/logs/` 以下に実行確認ログを残せる状態にする
2. When 実行確認ログが保存される, the x86_64 QEMU minimal boot feature shall `kernel_main` 到達を示す文字列をログで確認できる状態にする
3. If 実行確認ログが存在しない, the x86_64 QEMU minimal boot feature shall 受け入れ確認が未完了であることを開発者が判断できる状態にする

### Requirement 6: RTOS 機能を含めない最小範囲
**Objective:** As a 開発者, I want この feature が最小起動基盤に限定される, so that 後続の RTOS 機能実装と責務が混ざらない

#### Acceptance Criteria
1. The x86_64 QEMU minimal boot feature shall タスク生成を実装しない
2. The x86_64 QEMU minimal boot feature shall コンテキストスイッチを実装しない
3. The x86_64 QEMU minimal boot feature shall 割り込みを使わない
4. The x86_64 QEMU minimal boot feature shall タイマを使わない
5. The x86_64 QEMU minimal boot feature shall セマフォを実装しない
6. The x86_64 QEMU minimal boot feature shall μITRON 風 API を実装しない
