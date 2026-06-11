# Requirements Document

## Project Description (Input)
μITRON風RTOSの第15章 15.1として、README、articles配下の記事群、Zenn Articles表、開発ロードマップの整合性を確認し、v1.0手前の到達点を読み手が把握できるように整理する。現在は第1章から第14章までのZenn記事と、14.1から14.4で整備したμITRON風API層が存在する。今回は新機能を追加せず、READMEと記事一覧の現在地を整理し、第15章開始時点の成果を文書として追える状態にする。

## Requirements

### Requirement 1: READMEでv1.0手前の到達点を把握できる
**Objective:** As a 読み手, I want READMEだけで第15章開始時点の到達点を把握できる, so that 起動基盤からμITRON風API層までの積み上げを短時間で追える

#### Acceptance Criteria
1. When 読み手がREADMEを読む, the README shall このRTOSが学習用のμITRON風RTOSであり、x86_64 + QEMU上で動作し、Codex + cc-sddでspec-drivenに進めていることを明示する
2. When 読み手が到達点セクションを読む, the README shall 起動基盤、serial/HAL、task table/scheduler、RUNNING/READY/WAITING/DORMANT、task entry実行モデル、context switch境界、timer/interrupt/dispatch pending、preemption判定、semaphore wait queue、delay queue、timeout付きsemaphore待ち、tick到達READY復帰、μITRON風API層、共通エラーコード体系を段階的に説明する
3. When 読み手が第14章の説明を読む, the README shall `cre_tsk()` / `sta_tsk()`、`slp_tsk()` / `wup_tsk()`、`wai_sem()` / `sig_sem()` / `pol_sem()` / `twai_sem()`、`E_OK` / `E_ID` / `E_PAR` / `E_OBJ` / `E_TMOUT` などの共通エラーコード体系を第14章の到達点として明示する
4. The README shall 15.1が新機能追加ではなく、v1.0に向けたREADMEとZenn記事一覧の到達点整理であることを明示する

### Requirement 2: articles配下の記事とREADMEの章構成・tagが整合している
**Objective:** As a 保守者, I want articles配下の記事とREADMEの一覧が一致している, so that 記事・tag・章構成の現在地を間違えずに管理できる

#### Acceptance Criteria
1. When articles配下の第1章から第14章の記事を照合する, the README shall 実在する記事ファイルに対応する章・節・topic・tag候補を矛盾なく掲載する
2. If READMEのZenn Articles表に欠落または不整合がある, then the README shall 実際の記事ファイルと14.1から14.4のtag候補に合わせて補正される
3. Where 15.1記事を追加する場合, the README shall `v15.1-readme-zenn-milestone-summary` のtag候補をZenn Articles表で追えるようにする
4. The README shall articles配下の記事数とREADME上の第1章から第15章15.1までの一覧に矛盾がない状態にする

### Requirement 3: 15.1の記事は到達点整理として過去記事と文体を揃える
**Objective:** As a 読者, I want 15.1の記事で整理の理由と結果を理解できる, so that 第14章までの実装とREADME整理の関係を追える

#### Acceptance Criteria
1. When 15.1記事を作成する, the article shall 過去記事と同じfront matter、段階的な説明、検証したこと、今回のまとめを含む構成にする
2. When 15.1記事のはじめを読む, the article shall これまでの章で積み上げた内容を短く説明し、今回は機能追加ではなく到達点整理であることを明示する
3. The article shall READMEを整理する理由、Zenn Articles表を整理する理由、v1.0手前の到達点、実装概要、検証したことを説明する
4. The article shall 既存RTOSの実装コード参照・コピー・流用はしていないことを明記する

### Requirement 4: 既存kernel挙動を変更しない
**Objective:** As a 開発者, I want 文書整理でkernel挙動が変わらない, so that 第14章までのAPI層説明と実装結果を壊さずにv1.0整理へ進める

#### Acceptance Criteria
1. When 実装差分を確認する, the repository shall scheduler、dispatcher、task、semaphore、delay queue、kernel APIの挙動を変更するソース差分を含まない
2. When 通常buildを実行する, the build shall 成功する
3. When `make run` を実行する, the smoke run shall 成功する
4. When `make run VALIDATE_TIMER_IRQ_ENTRY=1` を実行する, the timer IRQ entry validation run shall 成功する
5. The spec directory shall 最終的に `requirements.md`、`design.md`、`tasks.md` の3ファイルだけを含む
