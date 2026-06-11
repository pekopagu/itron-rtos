# Requirements Document

## Project Description (Input)
μITRON風RTOSの第15章15.2として、v1.0整理編の一環でDoxygenによるAPIドキュメント生成導線を整備する。現在は起動基盤、serial/HAL、task table/scheduler、task状態、task entry実行モデル、context switch境界、timer/interrupt/dispatch pending、preemption判定、semaphore wait queue、delay queue、timeout付きsemaphore待ち、tick到達READY復帰、μITRON風API層、共通エラーコード体系まで実装済みである。今回は新しいkernel機能を追加せず、README、Doxyfile、APIリファレンス生成入口、今後の文書化方針、15.2用Zenn記事を整備する。

## Requirements

### Requirement 1: DoxygenによるAPIドキュメント生成導線を利用できる
**Objective:** As a 読み手, I want DoxygenでAPIリファレンスを生成できる, so that ソースコードと記事に加えて公開APIの入口を参照できる

#### Acceptance Criteria
1. When 開発者がリポジトリルートでDoxygenを実行する, the documentation tooling shall `Doxyfile` を入口としてAPIドキュメント生成設定を提供する
2. When Doxygen設定を確認する, the Doxyfile shall `PROJECT_NAME = itron-rtos`、`INPUT = kernel/include`、`RECURSIVE = YES`、`GENERATE_HTML = YES`、`GENERATE_LATEX = NO`、`OUTPUT_DIRECTORY = docs/doxygen`、`EXTRACT_ALL = YES` を含む
3. When DoxygenがHTMLを生成する, the documentation tooling shall 出力先を `docs/doxygen/html/index.html` として案内する
4. The documentation tooling shall kernel/include配下を主な入力対象にし、public APIを中心に参照できる入口を提供する

### Requirement 2: READMEからDoxygen生成手順と文書構成を追える
**Objective:** As a 読み手, I want READMEからDoxygen生成方法と文書の位置づけを理解できる, so that APIリファレンスを迷わず生成し、仕様書との役割分担を誤解しない

#### Acceptance Criteria
1. When 読み手がREADMEのDocumentationセクションを読む, the README shall このRTOSが学習用のμITRON風RTOSであり、x86_64 + QEMUで動作し、Codex + cc-sddで開発していることを文書構成の前提として説明する
2. When 読み手がDoxygen生成手順を読む, the README shall `doxygen Doxyfile` と `docs/doxygen/html/index.html` を明示する
3. The README shall DoxygenがAPIリファレンス生成と設計情報の入口であり、仕様書の代替ではないことを明示する
4. The README shall 今後各モジュールへDoxygenコメントを追加していく方針を記載する

### Requirement 3: Doxygenコメント整備は最小限に留める
**Objective:** As a 開発者, I want 今回のDoxygenコメント追加範囲が限定されている, so that kernel挙動やAPI仕様を変えずに文書化基盤だけを整えられる

#### Acceptance Criteria
1. Where 既存ヘッダにDoxygenコメントが不足している場合, the documentation update shall `kernel/include/itron_api.h`、`kernel/include/task.h`、`kernel/include/semaphore.h`、`kernel/include/delay_queue.h` に限定して最低限のコメントだけを追加できる
2. If 既存ヘッダにDoxygen生成入口として十分なコメントがある, then the documentation update shall 全面的なコメント追加を行わない
3. The documentation update shall kernel仕様、scheduler、dispatcher、semaphore、delay queue、API仕様を変更しない
4. The documentation update shall UML生成、call graph生成、class diagram生成、本格的なドキュメントサイト構築を含まない

### Requirement 4: 15.2記事とspec成果物が文書化基盤整備として整合している
**Objective:** As a 読者, I want 15.2記事でDoxygen導入の理由と検証結果を理解できる, so that v1.0に向けた文書化基盤整備の位置づけを追える

#### Acceptance Criteria
1. When 15.2記事を作成する, the article shall 指定されたfront matterと見出し構成を持つ
2. When 15.2記事のはじめを読む, the article shall これまでの章の流れを説明し、今回は機能追加ではなく文書化基盤整備であることを明示する
3. The article shall Doxygenはドキュメント生成の入口であり、既存RTOSコードの参照・コピー・流用は行っていないことを明記する
4. The spec directory shall 最終的に `requirements.md`、`design.md`、`tasks.md` の3ファイルだけを含む

### Requirement 5: 既存kernel挙動と検証導線を維持する
**Objective:** As a 開発者, I want 文書化導線追加後も既存kernelが同じように動く, so that v1.0整理編で実装済み挙動を壊さない

#### Acceptance Criteria
1. When 通常buildを実行する, the build shall 成功する
2. When `make run` を実行する, the smoke run shall 成功する
3. When `make run VALIDATE_TIMER_IRQ_ENTRY=1` を実行する, the timer IRQ entry validation run shall 成功する
4. When 実装差分を確認する, the repository shall kernel挙動変更またはAPI仕様変更を含まない
