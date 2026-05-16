# Requirements Document

## Introduction

この仕様は、μITRON風RTOSの第7章7.1として、x86_64向けの最小interrupt/exception foundationを定義する。目的は、timer interruptやpreemptionへ進む前段として、CPU例外を受け取り、QEMU serial logでIDT初期化と例外handler到達を観測できる状態を作ることである。

現在の実装は、x86_64 + QEMUで起動し、HAL console、task管理、priority scheduler、dispatcher、最小context switch smoke、semaphore foundation、timer foundation、preemption decision foundationまで完了している。今回の仕様では、この既存boot-time verification flowを壊さずに、CPU例外受信の最小基盤を追加する。

また、HAL構造とマルチアーキテクチャ思想を維持するため、kernel共通層はx86_64固有のinterrupt headerを直接利用しない。kernel共通層はHAL interrupt APIだけを呼び、x86_64固有のIDT、GDT前提、`lidt`、exception entry stubはarch層に閉じる。

## Boundary Context

- **In scope**: IDT初期化完了の観測、IDT load完了の観測、CPU例外handler到達の観測、例外番号または例外名のログ出力、既存GDT/segment前提の明示、kernel共通層からHAL interrupt APIだけを使う境界の維持、明示的な検証buildでの安全な例外発生。
- **Out of scope**: hardware timer interrupt、IRQ routing、PIC/APIC/HPET初期化、scheduler呼び出し、dispatcher呼び出し、context switch接続、preemption実行、task state変更、semaphore timeout、nested interrupt制御、user mode、SMP、μITRON互換API、既存RTOS実装コードの参照・コピー・翻訳・流用。
- **Adjacent expectations**: 既存Chapter 6.3までのsmoke flowは通常bootで継続する。HAL consoleは観測ログ出力の既存境界として利用する。x86_64固有処理はarch層が担い、kernel共通層はIDT/GDT/lidt/entry stubの詳細を所有しない。

## Requirements

### Requirement 1: IDT初期化の観測

**Objective:** As a RTOS学習者, I want 起動中にIDT初期化が完了したことを観測できる, so that timer interruptやpreemptionへ進む前にCPU例外受信の前提を確認できる

#### Acceptance Criteria

1. When kernel起動時のinterrupt/exception foundation初期化が実行される, the μITRON風RTOS shall IDT初期化完了をQEMU serial logで確認できるメッセージとして出力する
2. When IDT loadが完了する, the μITRON風RTOS shall IDT load完了をQEMU serial logで確認できるメッセージとして出力する
3. If interrupt/exception foundation初期化が失敗する, then the μITRON風RTOS shall 初期化失敗をQEMU serial logで確認できるメッセージとして出力し、既存smoke flowへ進まない
4. While 通常bootが実行される, the μITRON風RTOS shall IDT初期化後も既存のtask、timer、preemption decision、semaphore、context smoke flowを継続する

### Requirement 2: CPU例外handler到達の観測

**Objective:** As a RTOS学習者, I want CPU例外handlerに到達したことをserial logで確認できる, so that IDT entryとexception entry pathが機能していることを検証できる

#### Acceptance Criteria

1. When 登録済みCPU例外が発生する, the μITRON風RTOS shall 例外handler到達をQEMU serial logで確認できるメッセージとして出力する
2. When CPU例外handlerが実行される, the μITRON風RTOS shall 例外番号または例外名の少なくとも一方をQEMU serial logへ出力する
3. Where 明示的な検証buildが使われる, the μITRON風RTOS shall 検証用CPU例外の発生とhandler到達をQEMU serial logで観測できるようにする
4. If 発生したCPU例外が復帰不能な検証ケースである, then the μITRON風RTOS shall handler到達ログを出力した後に停止状態へ入ってよい

### Requirement 3: HAL interrupt境界の維持

**Objective:** As a RTOS開発者, I want kernel共通層がHAL interrupt APIだけを利用する, so that 将来のマルチアーキテクチャ展開に向けてarch固有処理を分離できる

#### Acceptance Criteria

1. The μITRON風RTOS shall kernel共通層がx86_64固有のinterrupt headerを直接includeしない状態を維持する
2. When kernel共通層がinterrupt/exception foundationを初期化する, the μITRON風RTOS shall HAL interrupt APIを通じて初期化を要求する
3. When HAL interrupt APIが呼び出される, the μITRON風RTOS shall 対象archのinterrupt foundationへ処理を委譲できる境界を持つ
4. The μITRON風RTOS shall kernel共通層がIDT、GDT selector、`lidt`、exception entry stubの詳細を直接所有しない状態を維持する

### Requirement 4: GDT/segment前提の明示

**Objective:** As a RTOS開発者, I want 既存GDT/segment前提が7.1の範囲で明示される, so that IDTとCPU例外受信基盤が既存boot前提と矛盾しないことを確認できる

#### Acceptance Criteria

1. When interrupt/exception foundationの仕様が確認される, the μITRON風RTOS shall 現在のlong mode起動に必要なGDT/segment前提を7.1の範囲として説明できる状態にする
2. If 7.1の実装に追加のGDT/segment整理が不要である, then the μITRON風RTOS shall 既存前提を維持することを仕様またはコメントで確認できるようにする
3. If 7.1の実装に最小限のGDT/segment整理が必要である, then the μITRON風RTOS shall その整理がCPU例外受信基盤のための範囲に限られることを確認できるようにする

### Requirement 5: 非対象機能との分離

**Objective:** As a RTOS学習者, I want 7.1の例外受信基盤がtimer/preemptionやtask実行制御へ接続されないことを確認できる, so that 段階的なRTOS構築の責務分離を維持できる

#### Acceptance Criteria

1. The μITRON風RTOS shall 7.1の範囲でhardware timer interruptを導入しない
2. The μITRON風RTOS shall 7.1の範囲でIRQ routing、PIC初期化、APIC初期化を導入しない
3. The μITRON風RTOS shall 7.1の範囲でscheduler呼び出し、dispatcher呼び出し、context switch接続、preemption実行を導入しない
4. The μITRON風RTOS shall 7.1の範囲でtask state変更、semaphore timeout、nested interrupt制御、user mode、SMP、μITRON互換APIを導入しない
5. When 7.1のQEMU serial logを確認する, the μITRON風RTOS shall 例外受信基盤のログと既存smoke flowのログを区別できるようにする

### Requirement 6: 教育用実装制約の維持

**Objective:** As a RTOS開発者, I want 今回のCPU例外handlerが教育用の観測handlerであることを明確にする, so that 復帰可能例外処理や実運用向けinterrupt subsystemと誤解しない

#### Acceptance Criteria

1. The μITRON風RTOS shall 7.1の仕様、コメント、またはDoxygenで、この実装がtimer/preemptionではなくCPU例外受信基盤であることを確認できるようにする
2. The μITRON風RTOS shall CPU例外handlerが復帰可能例外処理ではなく教育用観測handlerであることを確認できるようにする
3. The μITRON風RTOS shall 既存RTOS実装コードの参照、コピー、翻訳、流用を行わず、このプロジェクト向けの教育用独自実装として進める
