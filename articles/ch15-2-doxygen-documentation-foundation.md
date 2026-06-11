---
title: "CodexとSDDでμITRON風RTOSを作る 第15章 15.2: Doxygen生成導線を作る"
emoji: ""
type: "tech"
topics: ["rtos", "kernel", "c", "itron", "doxygen"]
published: false
---

## はじめに

この連載では、CodexとSDDを使いながら、学習用のμITRON風RTOSを少しずつ作っています。

第14章では、μITRON風API層を整理しました。
`cre_tsk()` / `sta_tsk()`、`slp_tsk()` / `wup_tsk()`、`wai_sem()` / `sig_sem()` / `pol_sem()` / `twai_sem()` を追加し、最後に `E_OK` / `E_ID` / `E_PAR` / `E_OBJ` / `E_TMOUT` などの共通エラーコード体系へ寄せました。

15.1では、kernelの新機能は追加せず、READMEとZenn記事一覧を整理しました。
起動基盤からμITRON風API層、共通エラーコード体系までをREADMEから追えるようにし、v1.0手前の現在地を文書として見える形にしました。

今回の15.2でも、kernel機能は追加しません。
Doxygenを使ってAPIドキュメントを生成するための導線を作り、ソースコード、Zenn記事、READMEに加えて、APIリファレンスも参照できる入口を整えます。

今回も、既存RTOSの実装コードは参照・コピー・流用していません。
本物のμITRON仕様を完全再現するのではなく、この学習用RTOSの現在の構造に合わせて、文書化基盤を整えます。

## 今回のゴール

今回のfeature名は次のとおりです。

```text
doxygen-documentation-foundation
```

tag候補は次のとおりです。

```text
v15.2-doxygen-documentation-foundation
```

今回のゴールは、DoxygenによるAPIドキュメント生成導線を作ることです。

具体的には、次を行います。

- `Doxyfile` を追加する
- Doxygenの入力対象を `kernel/include` にする
- Doxygenの出力先を `docs/doxygen/` に統一する
- READMEにDoxygen生成手順を追加する
- READMEにドキュメント構成とDoxygenの位置づけを追加する
- 15.2の記事として、なぜDoxygen導線を作るのかを整理する

重要なのは、今回はkernelの挙動を変えないことです。
task、scheduler、dispatcher、semaphore、delay queue、timer、interrupt、μITRON風APIの仕様は変更しません。

## 15.1までの到達点

15.1までで、この学習用RTOSは次の流れを持つようになっています。

```text
起動基盤
serial / HAL
task table / scheduler
RUNNING / READY / WAITING / DORMANT
task entry実行モデル
context switch境界
timer / interrupt / dispatch pending
preemption判定
semaphore wait queue
delay queue
timeout付きsemaphore待ち
tick到達READY復帰
μITRON風API層
共通エラーコード体系
READMEとZenn記事一覧による到達点整理
```

最初は、QEMU上で `kernel_main` に到達し、serial logを出すところから始めました。
その後、task table、scheduler、RUNNING current task、task entry呼び出し、task stack、context switch境界を追加しました。

さらに、timer tick、interrupt入口、dispatch pending、preemption判定を積み上げました。
semaphoreではWAITING遷移、wait queue、wakeup後のpreemption判定を整理しました。
delay queueでは `dly_tsk()`、timeout付き `twai_sem()`、tick到達によるREADY復帰を扱いました。

第14章では、それらの内部部品をμITRON風API層として読める形へ寄せました。
15.1では、ここまでの到達点をREADMEとZenn記事一覧から追えるように整理しました。

## なぜDoxygenを導入するのか

ここまでの開発では、READMEとZenn記事が主な説明の入口でした。
READMEは現在地を把握するための入口であり、Zenn記事は各章で何を考えて何を作ったかを説明するための記録です。

一方で、APIやヘッダ単位の情報を確認したいときには、ソースコードを直接読む必要があります。
`kernel/include/itron_api.h`、`kernel/include/task.h`、`kernel/include/semaphore.h`、`kernel/include/delay_queue.h` などにはDoxygen形式のコメントが入っていますが、生成されたリファレンスとして読む入口はまだありませんでした。

そこで15.2では、Doxygenを導入します。
ただし、Doxygenを仕様書の代替にはしません。
Doxygenは、設計情報やAPIの入口です。
requirements、design、tasks、README、Zenn記事を置き換えるものではなく、ソースコードを読むときの補助線として扱います。

## Doxygen生成導線の設計

今回のDoxygen導線は、かなり小さく始めます。

入力対象は次に絞ります。

```text
kernel/include
```

出力先は次に統一します。

```text
docs/doxygen
```

HTMLの入口は次です。

```text
docs/doxygen/html/index.html
```

public APIを中心に読むため、まずは `kernel/include` 配下を主な入力対象にします。
今後、各モジュールのコメントを少しずつ追加していく予定ですが、今回は全面的なコメント整備は行いません。

また、UML生成、call graph生成、class diagram生成、本格的なドキュメントサイト構築も今回の範囲外にしています。
15.2では、まず「生成できる入口」を作ることを優先します。

## Doxyfileの追加

リポジトリルートに `Doxyfile` を追加しました。

主な設定は次のとおりです。

```text
PROJECT_NAME           = itron-rtos
INPUT                  = kernel/include
RECURSIVE              = YES
GENERATE_HTML          = YES
GENERATE_LATEX         = NO
OUTPUT_DIRECTORY       = docs/doxygen
EXTRACT_ALL            = YES
```

これにより、リポジトリルートで次を実行するとAPIリファレンスを生成できます。

```sh
doxygen Doxyfile
```

生成物は `docs/doxygen/html/index.html` から確認します。

生成物はソースではなく成果物なので、`docs/doxygen/` はgit管理対象から外す方針にしました。
また、今回の方針として、生成されたHTML一式は公開しません。
必要な人がローカルで `doxygen Doxyfile` を実行し、自分の環境で確認する扱いにします。

## README更新

READMEのDocumentationセクションに、Doxygen生成手順を追加しました。

READMEでは、次の点を明記しています。

- このRTOSは学習用のμITRON風RTOSである
- x86_64 + QEMUで動作する
- Codex + cc-sddで開発している
- APIリファレンス生成にDoxygenを利用する
- ソースコード、Zenn記事、README、APIドキュメントを併用する
- Doxygenは設計情報の入口であり、仕様書の代替ではない
- 今後各モジュールへDoxygenコメントを追加していく

READMEのDevelopment Progress、Roadmap、Zenn Articles表にも15.2を追加しました。
これで、15.1の到達点整理に続いて、15.2のDoxygen生成導線もREADMEから追えるようになりました。

## 検証したこと

今回確認したいことは、文書化導線が追加されてもkernelの挙動が変わらないことです。

そのため、次を検証します。

```text
make
make run
make run VALIDATE_TIMER_IRQ_ENTRY=1
```

また、次も確認します。

- `Doxyfile` が追加されていること
- READMEに `doxygen Doxyfile` が記載されていること
- READMEに `docs/doxygen/html/index.html` が記載されていること
- `docs/doxygen/` がDoxygen出力先として整理されていること
- `docs/doxygen/` はローカル生成物として扱い、コミット・公開対象にしないこと
- kernel挙動変更やAPI仕様変更の差分がないこと
- `.kiro/specs/doxygen-documentation-foundation/` が `requirements.md` / `design.md` / `tasks.md` の3ファイルだけになっていること

## 今回のまとめ

今回は、第15章15.2としてDoxygen生成導線を整備しました。

15.1では、READMEとZenn記事一覧を整理し、v1.0手前の到達点を読めるようにしました。
15.2では、その次の文書化基盤として、APIリファレンス生成の入口を追加しました。

今回追加したのは、`Doxyfile`、READMEのDoxygen生成手順、15.2記事です。
kernel機能、scheduler、dispatcher、semaphore、delay queue、API仕様は変更していません。

Doxygenは、ソースコードを読むための入口です。
仕様書や記事の代替ではありません。
今後は、各モジュールへDoxygenコメントを少しずつ追加しながら、v1.0に向けて読めるkernelとしての基盤も整えていきます。
