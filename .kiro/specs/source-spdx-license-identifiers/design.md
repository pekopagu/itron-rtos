# Design Document

## Overview
この feature は、MIT License 方針をソースファイル単体でも判別できるようにするため、対象6ファイルへ `SPDX-License-Identifier: MIT` を追加する。利用者はメンテナとレビュアであり、公開前後のライセンス確認を短時間で一貫して行えるようにする。

この設計の最優先事項は、既存コード、ビルド手順、リンク設定、実行動作に影響を与えないことである。変更は SPDX コメント追加のみに限定し、serial API 整理、README 方針変更、LICENSE 変更、実装ロジック変更は扱わない。

### Goals
- 対象6ファイルに `SPDX-License-Identifier: MIT` を追加する。
- ファイル種別ごとに正しいコメント形式を使う。
- `make` が成功し、`build/kernel.elf` が生成される状態を維持する。

### Non-Goals
- `LICENSE` 本文、ライセンス種別、Copyright 表記方針を変更しない。
- `README.md`、`.gitignore`、`.kiro/specs/`、`build/`、`docs/logs/`、`articles/`、`prompts/`、`checkList/` を変更しない。
- serial API 整理、ロジック変更、ビルド手順変更、リンク設定変更を行わない。
- SPDX 挿入や検査の自動スクリプトは作らない。

## Boundary Commitments

### This Spec Owns
- 対象6ファイルへの SPDX コメント追加。
- ファイル種別ごとのコメント形式ルール。
- 挿入位置ルールと重複防止ルール。
- `make` によるビルド影響確認。

### Out of Boundary
- ライセンス本文全文の貼り付け。
- Copyright 表記追加。
- `LICENSE`、README、`.gitignore`、spec、ログ、記事、プロンプト、チェックリストの変更。
- C、ASM、linker script、Makefile の処理内容変更。
- serial API 整理や HAL 抽象化。
- QEMU 起動確認の必須化。

### Allowed Dependencies
- 既存の `x86_64-qemu-minimal-boot` 実装ファイル。
- ルートの `LICENSE` が MIT License であること。
- 既存の `make` ビルド経路。

### Revalidation Triggers
- 対象ファイル一覧を変更する場合。
- SPDX フォーマットやコメント形式を変更する場合。
- コメント追加以外の差分が必要になった場合。
- `Makefile`、`linker.ld`、C/ASM の処理内容へ変更が入る場合。

## Architecture

### Existing Architecture Analysis
対象ファイルは第4回の最小起動基盤で作成された。`boot/boot.asm`、`kernel/kernel.c`、`arch/x86_64/serial.c`、`arch/x86_64/serial.h`、`linker.ld`、`Makefile` が存在し、現在 `SPDX-License-Identifier` は含まれていない。

### Architecture Pattern & Boundary Map
**Architecture Integration**:
- Selected pattern: 最小コメント追加。各対象ファイルの先頭付近に SPDX コメントを追加し、コード構造は変更しない。
- Domain/feature boundaries: この feature はライセンス識別だけを扱い、boot、kernel、serial、build の責務には介入しない。
- Existing patterns preserved: 既存ファイル配置、既存ビルド経路、既存 `kernel_main` 動作。
- New components rationale: 新規コンポーネントは追加しない。
- Steering compliance: `.kiro/steering/` は未作成のため、AGENTS.md と requirements の制約に従う。

### Technology Stack

| Layer | Choice / Version | Role in Feature | Notes |
|-------|------------------|-----------------|-------|
| Source comments | SPDX-License-Identifier: MIT | ファイル単体のライセンス識別 | ライセンス本文全文は貼り付けない |
| Build CLI | GNU Make | 変更後のビルド確認 | 既存 `make` を変更しない |

## File Structure Plan

### Modified Files
- `boot/boot.asm` — 既存先頭 NASM コメントの直後に `; SPDX-License-Identifier: MIT` を追加する。
- `kernel/kernel.c` — ファイル最上部、`#include` より前に C コメント形式の SPDX ブロックを追加する。
- `arch/x86_64/serial.c` — ファイル最上部、`#include` より前に C コメント形式の SPDX ブロックを追加する。
- `arch/x86_64/serial.h` — ファイル最上部、include guard より前に C コメント形式の SPDX ブロックを追加する。
- `linker.ld` — ファイル最上部、`OUTPUT_FORMAT` より前に C コメント形式の SPDX ブロックを追加する。
- `Makefile` — ファイル最上部、変数定義より前に `# SPDX-License-Identifier: MIT` を追加する。

### Unchanged Files
- `README.md`
- `LICENSE`
- `.gitignore`
- `.kiro/specs/`
- `build/`
- `docs/logs/`
- `articles/`
- `prompts/`
- `checkList/`

## SPDX Comment Rules

### コメント形式ルール
| File type | Files | Format |
|-----------|-------|--------|
| C source | `kernel/kernel.c`, `arch/x86_64/serial.c` | `/* ... */` |
| C header | `arch/x86_64/serial.h` | `/* ... */` |
| linker script | `linker.ld` | `/* ... */` |
| NASM assembly | `boot/boot.asm` | `;` |
| Makefile | `Makefile` | `#` |

### 統一フォーマット
C 系、ヘッダ、linker script:

```c
/*
 * SPDX-License-Identifier: MIT
 */
```

NASM:

```asm
; SPDX-License-Identifier: MIT
```

Makefile:

```makefile
# SPDX-License-Identifier: MIT
```

### 挿入位置ルール
1. ファイル先頭にコメントが存在する場合、その既存コメントを保持し、直後に SPDX コメントを追加する。
2. 先頭にコメントが存在しない場合、ファイル最上部に SPDX コメントを追加する。
3. `#include`、include guard、`OUTPUT_FORMAT`、変数定義、コードより前に配置する。
4. 既存コメントブロックを編集、削除、分割しない。
5. shebang や特殊ディレクティブがある場合はそれを保持し、その直後に追加する。ただし今回の対象ファイルには該当なし。

### 冪等性の扱い
- 対象ファイルに `SPDX-License-Identifier` が既に存在する場合は追加しない。
- 実装前に対象6ファイルを検索し、重複の有無を確認する。
- 実装後に対象6ファイルそれぞれで `SPDX-License-Identifier: MIT` が1回だけ存在することを確認する。

## Requirements Traceability

| Requirement | Summary | Components | Interfaces | Flows |
|-------------|---------|------------|------------|-------|
| 1.1 | `boot/boot.asm` へ追加 | SPDX Comment Set | File edit | なし |
| 1.2 | `kernel/kernel.c` へ追加 | SPDX Comment Set | File edit | なし |
| 1.3 | `arch/x86_64/serial.c` へ追加 | SPDX Comment Set | File edit | なし |
| 1.4 | `arch/x86_64/serial.h` へ追加 | SPDX Comment Set | File edit | なし |
| 1.5 | `linker.ld` へ追加 | SPDX Comment Set | File edit | なし |
| 1.6 | `Makefile` へ追加 | SPDX Comment Set | File edit | なし |
| 2.1 | C source コメント形式 | Comment Format Rules | File edit | なし |
| 2.2 | Header コメント形式 | Comment Format Rules | File edit | なし |
| 2.3 | NASM コメント形式 | Comment Format Rules | File edit | なし |
| 2.4 | linker script コメント形式 | Comment Format Rules | File edit | なし |
| 2.5 | Makefile コメント形式 | Comment Format Rules | File edit | なし |
| 2.6 | 先頭付近配置 | Insertion Rules | File edit | なし |
| 3.1 | `LICENSE` 不変 | Scope Guard | Review check | なし |
| 3.2 | ライセンス種別不変 | Scope Guard | Review check | なし |
| 3.3 | Copyright 追加なし | Scope Guard | Review check | なし |
| 3.4 | ライセンス全文貼り付けなし | Scope Guard | Review check | なし |
| 3.5 | README 大幅変更なし | Scope Guard | Review check | なし |
| 3.6 | 対象外ファイル変更なし | Scope Guard | Review check | なし |
| 4.1 | 処理内容不変 | Scope Guard | Review check | なし |
| 4.2 | ビルド手順不変 | Scope Guard | Review check | なし |
| 4.3 | リンク設定不変 | Scope Guard | Review check | なし |
| 4.4 | `make` 成功 | Build Verification | CLI | なし |
| 4.5 | `build/kernel.elf` 生成維持 | Build Verification | Artifact | なし |
| 4.6 | serial API 整理なし | Scope Guard | Review check | なし |
| 5.1 | 差分は SPDX 追加のみ | Scope Guard | Diff review | なし |
| 5.2 | 追加変更は範囲外 | Scope Guard | Review check | なし |
| 5.3 | QEMU 起動確認は必須ではない | Build Verification | Validation choice | なし |
| 5.4 | 少なくとも `make` 実施 | Build Verification | CLI | なし |

## Components and Interfaces

| Component | Domain/Layer | Intent | Req Coverage | Key Dependencies | Contracts |
|-----------|--------------|--------|--------------|------------------|-----------|
| SPDX Comment Set | Source metadata | 対象6ファイルに MIT SPDX 識別子を追加する | 1.1, 1.2, 1.3, 1.4, 1.5, 1.6 | 対象ファイル P0 | File |
| Comment Format Rules | Source metadata | ファイル種別に応じたコメント形式を固定する | 2.1, 2.2, 2.3, 2.4, 2.5, 2.6 | SPDX Comment Set P0 | Rule |
| Insertion Rules | Source metadata | 既存コメントを壊さず先頭付近へ配置する | 2.6 | 対象ファイル P0 | Rule |
| Scope Guard | Review | 対象外変更とロジック変更を防ぐ | 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 4.1, 4.2, 4.3, 4.6, 5.1, 5.2 | git diff P0 | Review |
| Build Verification | Validation | SPDX 追加後もビルド可能であることを確認する | 4.4, 4.5, 5.3, 5.4 | GNU Make P0 | Batch |

### Source Metadata

#### SPDX Comment Set

| Field | Detail |
|-------|--------|
| Intent | 対象6ファイルへ `SPDX-License-Identifier: MIT` を追加する |
| Requirements | 1.1, 1.2, 1.3, 1.4, 1.5, 1.6 |

**Responsibilities & Constraints**
- 対象6ファイルだけを変更する。
- SPDX 識別子は1ファイル1回だけにする。
- ライセンス本文全文や Copyright 表記は追加しない。

**Dependencies**
- Inbound: メンテナ — 対象ファイルをレビューする (P1)
- Outbound: Build Verification — コメント追加後のビルド確認 (P0)

**Contracts**: Service [ ] / API [ ] / Event [ ] / Batch [ ] / State [ ]

#### Comment Format Rules

| Field | Detail |
|-------|--------|
| Intent | ファイル種別ごとの正しい SPDX コメント形式を固定する |
| Requirements | 2.1, 2.2, 2.3, 2.4, 2.5, 2.6 |

**Responsibilities & Constraints**
- C、header、linker script は C コメントブロックを使う。
- NASM は `;` コメントを使う。
- Makefile は `#` コメントを使う。
- 既存コメントやコードを削除しない。

### Review And Validation

#### Scope Guard

| Field | Detail |
|-------|--------|
| Intent | SPDX 追加以外の差分を防ぐ |
| Requirements | 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 4.1, 4.2, 4.3, 4.6, 5.1, 5.2 |

**Responsibilities & Constraints**
- git diff で対象外ファイル変更とロジック変更がないことを確認する。
- コメント追加以外が必要になった場合は、この feature では扱わない。

#### Build Verification

| Field | Detail |
|-------|--------|
| Intent | SPDX 追加後も第4回のビルド成果物生成が維持されることを確認する |
| Requirements | 4.4, 4.5, 5.3, 5.4 |

**Responsibilities & Constraints**
- `make` を実行し、成功を確認する。
- `build/kernel.elf` の生成を確認する。
- QEMU 起動確認は必須ではない。

**Contracts**: Service [ ] / API [ ] / Event [ ] / Batch [x] / State [ ]

##### Batch / Job Contract
- Trigger: `make`
- Input / validation: SPDX 追加後の対象ファイル、既存 `Makefile`
- Output / destination: `build/kernel.elf`
- Idempotency & recovery: SPDX が既にある場合は重複追加せず、再実行しても差分が増えない

## Error Handling

### Error Strategy
- SPDX が既に存在する場合は追加しない。
- コメント追加以外の変更が必要になった場合は実装を止め、feature 範囲外として扱う。
- `make` が失敗した場合は、SPDX コメント形式や挿入位置がビルドに影響していないかを確認する。

### Error Categories and Responses
- **Duplicate SPDX**: 同一ファイルに複数の `SPDX-License-Identifier` がある場合、1つだけ残す設計に戻す。
- **Invalid comment style**: ファイル種別に合わないコメントでビルドが壊れる場合、定義済みコメント形式へ修正する。
- **Scope spillover**: ロジックや対象外ファイルの差分が出た場合、その差分は取り除く。

### Monitoring
- 常時監視は行わない。
- 検証は SPDX 検索、git diff 確認、`make` の終了結果、`build/kernel.elf` の存在確認で行う。

## Testing Strategy

### Static Checks
- 対象6ファイルすべてに `SPDX-License-Identifier: MIT` が含まれることを確認する。対象: 1.1, 1.2, 1.3, 1.4, 1.5, 1.6。
- 対象6ファイルで `SPDX-License-Identifier` が重複していないことを確認する。対象: 2.6, 5.1。
- コメント形式が C、NASM、Makefile のルールに合っていることを確認する。対象: 2.1, 2.2, 2.3, 2.4, 2.5。

### Diff Review
- 変更対象が対象6ファイルに限定されていることを確認する。対象: 3.6, 5.1。
- 差分が SPDX コメント追加だけであり、処理ロジック、ビルド手順、リンク設定を変更していないことを確認する。対象: 4.1, 4.2, 4.3, 5.1。
- `LICENSE`、README、ライセンス種別、Copyright 表記、serial API に変更がないことを確認する。対象: 3.1, 3.2, 3.3, 3.4, 3.5, 4.6。

### Build Tests
- `make` が成功することを確認する。対象: 4.4, 5.4。
- `build/kernel.elf` が生成されることを確認する。対象: 4.5。
- QEMU 起動確認は必須にしない。対象: 5.3。

## Risks & Mitigations
- **Risk**: `linker.ld` や Makefile のコメント形式を誤り、ビルドが失敗する。  
  **Mitigation**: ファイル種別ごとのフォーマットを固定し、`make` で確認する。
- **Risk**: 既存先頭コメントを壊す。  
  **Mitigation**: 既存コメントは編集せず、直後または最上部に追加する。
- **Risk**: 対象外ファイルに差分が出る。  
  **Mitigation**: git diff で対象6ファイル以外の変更がないことを確認する。
