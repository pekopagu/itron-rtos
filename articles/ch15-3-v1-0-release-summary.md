---
title: "CodexとSDDでμITRON風RTOSを作る 第15章 15.3: v1.0として到達点を固定する"
emoji: ""
type: "tech"
topics: ["rtos", "kernel", "c", "itron", "sdd"]
published: false
---

## はじめに

この連載では、CodexとSDDを使いながら、学習用のμITRON風RTOSを少しずつ作っています。

第14章では、μITRON風API層を整理しました。
`cre_tsk()` / `sta_tsk()`、`slp_tsk()` / `wup_tsk()`、`wai_sem()` / `sig_sem()` / `pol_sem()` / `twai_sem()` を追加し、最後に `E_OK` / `E_ID` / `E_PAR` / `E_OBJ` / `E_TMOUT` などの共通エラーコード体系へ寄せました。

第15章では、v1.0に向けた整理を進めました。
15.1ではREADMEとZenn記事一覧を整理し、15.2ではDoxygen生成導線を追加しました。

今回の15.3では、新しいkernel機能は追加しません。
ここまでの到達点を、学習用μITRON風RTOSとしての `v1.0` として固定します。

今回も、既存RTOSの実装コードは参照・コピー・流用していません。
本物のμITRON仕様を完全再現するのではなく、この学習用RTOSの現在の構造に合わせて、release summaryとして区切ります。

## 今回のゴール

今回のfeature名は次のとおりです。

```text
v1-0-release-summary
```

今回のtagは次のとおりです。

```text
v1.0
```

今回のゴールは、v1.0を「完成品宣言」ではなく、学習用μITRON風RTOSとして一通り読める・動かせる・検証できる到達点として固定することです。

具体的には、次を行います。

- READMEにv1.0の範囲を明記する
- READMEにv1.0の非目標を明記する
- READMEにv1.0の検証コマンドを明記する
- READMEにDoxygen生成物を公開しない方針を維持する
- v1.0まとめ記事を追加する
- `v1.0` タグを付ける

## v1.0までの到達点

v1.0までで、この学習用RTOSは次の流れを持つようになりました。

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
Doxygen生成導線
```

最初は、QEMU上で `kernel_main` に到達し、serial logを出すだけの小さなkernelでした。
そこから、task table、scheduler、RUNNING current task、task entry呼び出し、task stack、context switch境界を追加しました。

その後、timer tick、interrupt入口、dispatch pending、preemption判定を段階的に積みました。
semaphoreではWAITING遷移、wait queue、wakeup後のpreemption判定を整理しました。
delay queueでは `dly_tsk()`、timeout付き `twai_sem()`、tick到達READY復帰を扱いました。

第14章では、これらをμITRON風API層として読める形へ寄せました。
第15章では、README、Zenn記事一覧、Doxygen生成導線を整理し、v1.0として追える文書化基盤を作りました。

## v1.0の範囲

このリポジトリのv1.0は、学習用μITRON風RTOSとしての到達点です。

v1.0に含める範囲は次のとおりです。

- x86_64 + QEMUで起動する
- `kernel_main` に到達する
- serial / HAL境界を持つ
- 静的task tableを持つ
- priority schedulerを持つ
- RUNNING / READY / WAITING / DORMANTを扱う
- task entry実行モデルを持つ
- context switch境界を観測できる
- timer tickとinterrupt入口を扱う
- dispatch pendingとpreemption判定を持つ
- semaphore wait queueを持つ
- delay queueを持つ
- timeout付きsemaphore待ちを扱う
- tick到達READY復帰を扱う
- μITRON風API層を持つ
- 共通エラーコード体系を持つ
- READMEとZenn記事から到達点を追える
- DoxygenでAPIリファレンスをローカル生成できる

ここで重要なのは、v1.0が「本物のμITRON完全互換」ではないことです。
あくまで、学習用RTOSとしてここまでの流れを一通り読める状態にした区切りです。

## v1.0でやらないこと

v1.0では、次はまだやりません。

- μITRON完全互換
- production readyなRTOS化
- 完全なinterrupt return frame上のpreemptive context switch
- full timer interrupt subsystem
- SMP
- APIC / IOAPIC / LAPIC
- priority inheritance
- mutex
- event flag
- mailbox
- officialなμITRON仕様準拠テスト
- Doxygen生成HTMLの公開

この線引きをREADMEにも明記しました。
「v1.0」という名前を付けても、未実装のものを実装済みのように見せないことを重視しています。

## README更新

READMEでは、これまでの `Current Milestone Before v1.0` をv1.0の範囲として読めるように整理しました。

主に追加したのは次の内容です。

- `v1.0 Scope`
- `v1.0 Non-Goals`
- `v1.0 Validation`
- Development Progressの15.3行
- Roadmapのv1.0 release summary
- Zenn Articles表の15.3行

READMEは英語で記載しています。
Zenn記事は日本語で、連載としての流れを説明します。

## Doxygen生成物の扱い

15.2でDoxygen生成導線を追加しましたが、生成されたHTML一式は公開しません。

`Doxyfile` とソースコメントはリポジトリに残します。
必要な人は、自分の環境で次を実行します。

```sh
doxygen Doxyfile
```

出力先は次です。

```text
docs/doxygen/html/index.html
```

ただし、`docs/doxygen/` はローカル生成物として扱い、コミット・公開対象にはしません。

## 検証したこと

v1.0として区切る前に、次を検証します。

```text
make
make run
make run VALIDATE_TIMER_IRQ_ENTRY=1
doxygen Doxyfile
```

確認したいことは次です。

- buildが通ること
- QEMU smoke runが通ること
- timer IRQ entry validationが通ること
- Doxygen生成が通ること
- kernel挙動変更やAPI仕様変更の差分がないこと
- Doxygen生成物をコミット・公開しないこと
- `.kiro/specs/v1-0-release-summary/` が `requirements.md` / `design.md` / `tasks.md` の3ファイルだけであること

## 今回のまとめ

今回は、第15章15.3として、学習用μITRON風RTOSを `v1.0` として区切るための整理を行いました。

v1.0は、完成したμITRON互換RTOSという意味ではありません。
起動、task、scheduler、context switch境界、timer/interrupt/preemption、semaphore、delay queue、μITRON風API層、共通エラーコード、Doxygen生成導線までを一通り追える状態にした、学習用RTOSとしての到達点です。

ここから先は、v1.0で固定した土台の上に、未実装項目を1つずつ追加していく段階になります。
ただし、v1.0時点では、何ができて何がまだできないのかを明確にしておくことを優先しました。
