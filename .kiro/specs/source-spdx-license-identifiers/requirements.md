# Requirements Document

## Introduction
この仕様は、第5回の一部として、既存ソースファイルへ `SPDX-License-Identifier: MIT` を追加するための要求を定義する。本プロジェクトはルートの `LICENSE` により MIT License として公開する方針であり、第4回で追加された最小カーネル起動用ファイルにも、ファイル単体でライセンスを判別できる識別子を付与する。

この feature は公開品質の整備を目的とし、コードの処理内容、ビルド手順、リンク設定、ライセンス本文、README の方針を変更しない。SPDX 追加後も `make` が成功し、`build/kernel.elf` が生成されることを確認する。

## Boundary Context
- **In scope**: `boot/boot.asm`、`kernel/kernel.c`、`arch/x86_64/serial.c`、`arch/x86_64/serial.h`、`linker.ld`、`Makefile` への `SPDX-License-Identifier: MIT` 追加、および `make` によるビルド確認。
- **Out of scope**: `README.md`、`LICENSE`、`.gitignore`、`.kiro/specs/`、`build/`、`docs/logs/`、`articles/`、`prompts/`、`checkList/` の変更。ライセンス本文の変更、ライセンス種別の変更、著作権者表記方針の大改定、第三者コード追加、実装ロジック変更、serial API 整理。
- **Adjacent expectations**: `x86_64-qemu-minimal-boot` の実装が完了していることを前提とし、この feature はその動作を変えずにライセンス識別だけを追加する。

## Requirements

### Requirement 1: 対象ファイルへの SPDX 識別子追加
**Objective:** As a メンテナ, I want 対象ファイル単体で MIT License を判別できる, so that 公開前後のライセンス確認を一貫して行える

#### Acceptance Criteria
1. The source SPDX license identifiers feature shall `boot/boot.asm` に `SPDX-License-Identifier: MIT` を含める
2. The source SPDX license identifiers feature shall `kernel/kernel.c` に `SPDX-License-Identifier: MIT` を含める
3. The source SPDX license identifiers feature shall `arch/x86_64/serial.c` に `SPDX-License-Identifier: MIT` を含める
4. The source SPDX license identifiers feature shall `arch/x86_64/serial.h` に `SPDX-License-Identifier: MIT` を含める
5. The source SPDX license identifiers feature shall `linker.ld` に `SPDX-License-Identifier: MIT` を含める
6. The source SPDX license identifiers feature shall `Makefile` に `SPDX-License-Identifier: MIT` を含める

### Requirement 2: ファイル種別に合ったコメント形式
**Objective:** As a メンテナ, I want SPDX 識別子が各ファイル種別で妥当なコメント形式になっている, so that ビルドや解釈に影響せずにライセンスを示せる

#### Acceptance Criteria
1. When SPDX 識別子が C ソースファイルに追加される, the source SPDX license identifiers feature shall C コメント形式で表記する
2. When SPDX 識別子が C ヘッダファイルに追加される, the source SPDX license identifiers feature shall C コメント形式で表記する
3. When SPDX 識別子が NASM アセンブリファイルに追加される, the source SPDX license identifiers feature shall NASM コメント形式で表記する
4. When SPDX 識別子が linker script に追加される, the source SPDX license identifiers feature shall C コメント形式で表記する
5. When SPDX 識別子が Makefile に追加される, the source SPDX license identifiers feature shall `#` コメント形式で表記する
6. The source SPDX license identifiers feature shall SPDX 識別子を各対象ファイルの先頭付近に配置する

### Requirement 3: ライセンス方針と対象範囲の不変性
**Objective:** As a メンテナ, I want SPDX 追加がライセンス方針や対象外ファイルを変更しない, so that 公開方針と既存文書の意味を変えずに整備できる

#### Acceptance Criteria
1. The source SPDX license identifiers feature shall `LICENSE` 本文を変更しない
2. The source SPDX license identifiers feature shall ライセンス種別を MIT License から変更しない
3. The source SPDX license identifiers feature shall Copyright 表記を追加しない
4. The source SPDX license identifiers feature shall ライセンス本文全文を対象ファイルへ貼り付けない
5. The source SPDX license identifiers feature shall `README.md` の本文を大幅に変更しない
6. The source SPDX license identifiers feature shall 対象外ファイルを変更しない

### Requirement 4: 実装ロジックとビルド動作の不変性
**Objective:** As a 開発者, I want SPDX 追加後も既存の最小カーネルビルドが変わらず成功する, so that ライセンス整備が実装動作に影響していないことを確認できる

#### Acceptance Criteria
1. The source SPDX license identifiers feature shall 既存の処理内容を変更しない
2. The source SPDX license identifiers feature shall 既存のビルド手順を変更しない
3. The source SPDX license identifiers feature shall 既存のリンク設定を変更しない
4. When 開発者が `make` を実行する, the source SPDX license identifiers feature shall ビルドを成功させる
5. When `make` が成功する, the source SPDX license identifiers feature shall `build/kernel.elf` が生成される状態を維持する
6. The source SPDX license identifiers feature shall serial API 整理を扱わない

### Requirement 5: 差分の限定性
**Objective:** As a レビュア, I want 差分が SPDX コメント追加に限定されている, so that この feature のレビュー範囲を明確にできる

#### Acceptance Criteria
1. When レビュアが差分を確認する, the source SPDX license identifiers feature shall 対象ファイルの変更を SPDX コメント追加に限定する
2. If SPDX コメント追加以外の変更が必要になる, the source SPDX license identifiers feature shall この feature の範囲外として扱う
3. While この feature を実装している, the source SPDX license identifiers feature shall QEMU 起動確認を必須条件にしない
4. While この feature を検証している, the source SPDX license identifiers feature shall 少なくとも `make` によるビルド確認を実施する
