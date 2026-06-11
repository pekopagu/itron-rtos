# Implementation Plan

- [x] 1. Doxygen生成設定を追加する
  - リポジトリルートから `doxygen Doxyfile` で生成できる設定ファイルを追加する
  - 入力は `kernel/include`、出力は `docs/doxygen`、HTML生成あり、LaTeX生成なしにする
  - `PROJECT_NAME = itron-rtos` と `EXTRACT_ALL = YES` を含め、APIリファレンス入口として使える状態にする
  - _Requirements: 1, 3, 5_
  - _Boundary: Doxyfile_

- [x] 2. READMEにDoxygen生成導線と文書構成を追加する
  - Documentationセクションに `doxygen Doxyfile` と `docs/doxygen/html/index.html` を追加する
  - DoxygenはAPIリファレンス生成と設計情報の入口であり、仕様書の代替ではないことを明記する
  - 学習用μITRON風RTOS、x86_64 + QEMU、Codex + cc-sdd、ソースコード・記事・APIドキュメントを併用する文書構成を説明する
  - Development Progress、Roadmap、Zenn Articles表へ15.2の文書化基盤整備を反映する
  - _Requirements: 2, 4, 5_
  - _Boundary: README Documentation_

- [x] 3. 15.2 Zenn記事を追加する
  - `articles/ch15-2-doxygen-documentation-foundation.md` を指定front matterと見出し構成で追加する
  - 15.1までの到達点、Doxygen導入理由、Doxyfile追加、README更新、検証したことを説明する
  - 今回は機能追加ではなく文書化基盤整備であり、既存RTOSコードの参照・コピー・流用はしていないことを明記する
  - _Requirements: 4, 5_
  - _Boundary: Zenn Article 15.2_

- [x] 4. 文書化導線と既存kernel挙動を検証する
  - `Doxyfile` とREADMEのDoxygen手順・出力方針を確認する
  - `.kiro/specs/doxygen-documentation-foundation/` を `requirements.md`、`design.md`、`tasks.md` の3ファイルだけにする
  - `make`、`make run`、`make run VALIDATE_TIMER_IRQ_ENTRY=1` を実行して成功を確認する
  - kernel挙動変更やAPI仕様変更を含む差分がないことを確認する
  - _Requirements: 1, 2, 3, 4, 5_
  - _Boundary: Validation_
