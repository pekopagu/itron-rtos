# 実装計画

- [x] 1. 対象ファイルの存在を確認する
  - 目的: SPDX 追加作業の対象を6ファイルに固定し、対象外ファイルへ作業が広がらないようにする。
  - 対象ファイル: `boot/boot.asm`, `kernel/kernel.c`, `arch/x86_64/serial.c`, `arch/x86_64/serial.h`, `linker.ld`, `Makefile`
  - 作業内容: 各ファイルがリポジトリ内に存在することを確認し、欠落がある場合は実装へ進まず設計または要件へ戻す。
  - 完了条件: 6ファイルすべての存在が確認され、変更対象がこの6ファイルだけであることを作業者が明確に把握している。
  - 注意点: `README.md`, `LICENSE`, `.gitignore`, `.kiro/settings`, `docs/logs`, `build`, `articles`, `prompts`, `checkList` は変更対象外として扱う。
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 3.6_

- [x] 2. 既存SPDX表記の有無を確認する
  - 目的: `SPDX-License-Identifier` の重複追加を防ぎ、既存表記があるファイルを壊さない。
  - 対象ファイル: `boot/boot.asm`, `kernel/kernel.c`, `arch/x86_64/serial.c`, `arch/x86_64/serial.h`, `linker.ld`, `Makefile`
  - 作業内容: 対象6ファイル内で `SPDX-License-Identifier` を検索し、すでに存在するファイルには新しい SPDX コメントを追加しない方針を確認する。
  - 完了条件: 対象6ファイルごとに SPDX 表記の有無が確認され、追加対象は SPDX 表記が未存在のファイルだけに限定されている。
  - 注意点: 既存の SPDX 表記がある場合は、そのファイルのコメント形式や位置を勝手に変更しない。
  - _Requirements: 2.6, 5.1_

- [x] 3. CソースとCヘッダへSPDXコメントを追加する
  - 目的: C系ファイル単位で MIT License を識別できるようにする。
  - 対象ファイル: `kernel/kernel.c`, `arch/x86_64/serial.c`, `arch/x86_64/serial.h`
  - 作業内容: SPDX 未存在のファイルに限り、ファイル先頭付近へ C コメント形式の SPDX ブロックを追加する。
  - 完了条件: 各対象ファイルに `/* ... */` 形式で `SPDX-License-Identifier: MIT` が1回だけ存在し、`#include` または include guard より前に配置されている。
  - 注意点: 既存コメント、include、include guard、処理ロジック、serial API は変更しない。
  - _Requirements: 1.2, 1.3, 1.4, 2.1, 2.2, 2.6, 4.1, 4.6, 5.1_

- [x] 4. NASMアセンブリへSPDXコメントを追加する
  - 目的: NASMファイル単位で MIT License を識別できるようにする。
  - 対象ファイル: `boot/boot.asm`
  - 作業内容: SPDX 未存在の場合に限り、ファイル先頭付近へ `; SPDX-License-Identifier: MIT` を追加する。
  - 完了条件: `boot/boot.asm` に NASM コメント形式の SPDX 表記が1回だけ存在し、アセンブリ命令や既存コメントは変更されていない。
  - 注意点: boot処理、エントリ、セクション、命令列は変更しない。
  - _Requirements: 1.1, 2.3, 2.6, 4.1, 5.1_

- [x] 5. linker.ldへSPDXコメントを追加する
  - 目的: linker script 単位で MIT License を識別できるようにする。
  - 対象ファイル: `linker.ld`
  - 作業内容: SPDX 未存在の場合に限り、ファイル先頭付近へ C コメント形式の SPDX ブロックを追加する。
  - 完了条件: `linker.ld` に `/* ... */` 形式で `SPDX-License-Identifier: MIT` が1回だけ存在し、`OUTPUT_FORMAT` やリンク設定より前に配置されている。
  - 注意点: リンク設定、セクション配置、シンボル定義は変更しない。
  - _Requirements: 1.5, 2.4, 2.6, 4.3, 5.1_

- [x] 6. MakefileへSPDXコメントを追加する
  - 目的: Makefile 単位で MIT License を識別できるようにする。
  - 対象ファイル: `Makefile`
  - 作業内容: SPDX 未存在の場合に限り、ファイル先頭付近へ `# SPDX-License-Identifier: MIT` を追加する。
  - 完了条件: `Makefile` に `#` コメント形式の SPDX 表記が1回だけ存在し、変数定義やターゲット定義より前に配置されている。
  - 注意点: ビルド手順、コンパイルオプション、リンク手順、ターゲット定義は変更しない。
  - _Requirements: 1.6, 2.5, 2.6, 4.2, 5.1_

- [x] 7. git diffで差分を確認する
  - 目的: 変更が SPDX コメント追加だけに限定されていることを確認する。
  - 対象ファイル: `boot/boot.asm`, `kernel/kernel.c`, `arch/x86_64/serial.c`, `arch/x86_64/serial.h`, `linker.ld`, `Makefile`
  - 作業内容: `git diff` を確認し、対象6ファイル以外の差分がないこと、各差分がコメント追加だけであることを確認する。
  - 完了条件: 実装ロジック、ビルド設定、リンク設定、ライセンス本文、README、対象外ファイルに変更がないことが diff 上で確認できている。
  - 注意点: コメント追加以外の差分が出た場合は、この feature の範囲外として取り除いてから次へ進む。
  - _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 4.1, 4.2, 4.3, 4.6, 5.1, 5.2_

- [x] 8. makeでビルドを確認する
  - 目的: SPDX コメント追加後も既存の最小カーネルビルドが成功することを確認する。
  - 対象ファイル: `Makefile`, `build/kernel.elf`
  - 作業内容: `make clean` ターゲットが存在する場合は実行し、その後 `make` を実行する。
  - 完了条件: `make` が成功し、`build/kernel.elf` が生成されている。
  - 注意点: QEMU 起動確認は必須にせず、ビルド確認と成果物確認に限定する。
  - _Requirements: 4.4, 4.5, 5.3, 5.4_

- [x] 9. 最終レビューで受け入れ条件と設計ルールを確認する
  - 目的: requirements.md と design.md の条件を満たした状態で実装を完了できるようにする。
  - 対象ファイル: `boot/boot.asm`, `kernel/kernel.c`, `arch/x86_64/serial.c`, `arch/x86_64/serial.h`, `linker.ld`, `Makefile`
  - 作業内容: requirements.md の受け入れ条件、design.md の挿入位置ルール、コメント形式ルール、重複防止ルール、対象外ファイル未変更を最終確認する。
  - 完了条件: 対象6ファイルそれぞれに正しい SPDX 表記が1回だけ存在し、対象外ファイルやロジック変更がなく、ビルド成果物確認まで完了している。
  - 注意点: `LICENSE` 本文の貼り付け、Copyright 追加、README 更新、serial API 整理、ビルド設定変更は実施しない。
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 4.1, 4.2, 4.3, 4.4, 4.5, 4.6, 5.1, 5.2, 5.3, 5.4_
