# Requirements Document

## Project Description (Input)
μITRON風RTOSを学習用RTOSとしてv1.0にまとめる。現在は起動基盤、serial/HAL、task table/scheduler、RUNNING/READY/WAITING/DORMANT、task entry実行モデル、context switch境界、timer/interrupt/dispatch pending、preemption判定、semaphore wait queue、delay queue、timeout付きsemaphore待ち、tick到達READY復帰、μITRON風API層、共通エラーコード体系、README/Zenn整理、Doxygen生成導線まで実装済みである。今回は新しいkernel機能を追加せず、v1.0の意味、範囲、非目標、検証結果、文書構成をREADMEとZenn記事で固定し、v1.0タグに向けた公開単位を整理する。

## Requirements

### Requirement 1: READMEでv1.0の意味と範囲を把握できる
**Objective:** As a 読み手, I want READMEからv1.0の到達点を理解できる, so that このRTOSが何を満たし、何を満たさないかを誤解せずに読める

#### Acceptance Criteria
1. When 読み手がREADMEを読む, the README shall v1.0が学習用μITRON風RTOSとしての到達点であり、完全なμITRON互換やproduction readyを意味しないことを明示する
2. When 読み手がv1.0 scopeを読む, the README shall 起動、serial/HAL、task、scheduler、context switch境界、timer/interrupt/preemption、semaphore、delay queue、μITRON風API層、共通エラーコード、Doxygen生成導線をv1.0範囲として示す
3. When 読み手がv1.0 non-goalsを読む, the README shall 未実装機能とproduction用途ではないことを明示する
4. The README shall 英語で記載される

### Requirement 2: v1.0検証と文書構成が固定されている
**Objective:** As a 開発者, I want v1.0の検証方法と文書構成が明確である, so that タグ作成前後に同じ基準で再確認できる

#### Acceptance Criteria
1. When READMEのv1.0 validationを読む, the README shall `make`、`make run`、`make run VALIDATE_TIMER_IRQ_ENTRY=1`、`doxygen Doxyfile` を検証コマンドとして示す
2. The README shall Doxygen生成物 `docs/doxygen/` をコミット・公開しない方針を維持する
3. The README shall Development Progress、Roadmap、Zenn Articles表からv1.0まとめを追える状態にする
4. The repository shall v1.0整理でkernel挙動やAPI仕様を変更しない

### Requirement 3: v1.0まとめ記事を追加する
**Objective:** As a 読者, I want v1.0まとめ記事でここまでの流れと検証結果を読める, so that 連載上の区切りとしてv1.0の意味を理解できる

#### Acceptance Criteria
1. When v1.0まとめ記事を作成する, the article shall これまでの章の流れ、v1.0の範囲、非目標、検証結果、今後の扱いを説明する
2. The article shall 今回が機能追加ではなくrelease summaryであることを明示する
3. The article shall 既存RTOSコードの参照・コピー・流用は行っていないことを明記する
4. The article shall 日本語で記載される

### Requirement 4: v1.0タグに向けた成果物が整っている
**Objective:** As a 保守者, I want v1.0タグを打てる状態に整理されている, so that 学習用RTOSとしての到達点を固定できる

#### Acceptance Criteria
1. When spec directoryを確認する, the spec directory shall `requirements.md`、`design.md`、`tasks.md` の3ファイルだけを含む
2. When 検証コマンドを実行する, the build and smoke checks shall 成功する
3. When Doxygenを実行する, the documentation generation shall 成功する
4. When 差分を確認する, the implementation shall README、記事、spec、必要な文書設定だけを含み、kernel挙動変更を含まない
