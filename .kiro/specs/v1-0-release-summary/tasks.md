# Implementation Plan

- [x] 1. READMEをv1.0向けに整理する
  - v1.0が学習用μITRON風RTOSとしての到達点であり、完全互換やproduction readyを意味しないことを英語で明記する
  - v1.0 scope、non-goals、validation、documentation policyをREADMEから追えるようにする
  - Development Progress、Roadmap、Zenn Articles表へv1.0まとめを追加する
  - _Requirements: 1, 2, 4_
  - _Boundary: README v1.0 Sections_

- [x] 2. v1.0まとめ記事を追加する
  - `articles/ch15-3-v1-0-release-summary.md` を追加する
  - これまでの章の流れ、v1.0の範囲、非目標、検証結果、Doxygen生成物を公開しない方針を説明する
  - 今回は機能追加ではなくrelease summaryであり、既存RTOSコードの参照・コピー・流用はしていないことを明記する
  - _Requirements: 3, 4_
  - _Boundary: Zenn Article 15.3_

- [x] 3. v1.0タグ前の検証を実行する
  - `.kiro/specs/v1-0-release-summary/` が `requirements.md`、`design.md`、`tasks.md` の3ファイルだけであることを確認する
  - `make`、`make run`、`make run VALIDATE_TIMER_IRQ_ENTRY=1`、`doxygen Doxyfile` を実行して成功を確認する
  - kernel挙動変更やAPI仕様変更を含む差分がないことを確認する
  - _Requirements: 1, 2, 3, 4_
  - _Boundary: Validation_
