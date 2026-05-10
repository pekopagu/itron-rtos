# Requirements Document

## Introduction
この feature は、μITRON 風 RTOS の第5章5.3「最小コンテキストスイッチ」として、TCB 上の `task_context_t` を実際の x86_64 CPU register 保存・復元経路へ接続する。第5章5.1で導入済みの task stack metadata と、第5章5.2で導入済みの `context.rsp`, `rbp`, `rbx`, `r12`, `r13`, `r14`, `r15` を前提に、協調的・明示的に呼び出せる最小 context switch を観測可能にする。

この段階では timer interrupt、preemption、割り込みハンドラからの切り替えには進まない。目的は、scheduler、dispatcher、context switch 実処理の責務を分けたまま、保存対象・復元対象・stack pointer の変化を QEMU serial log で追跡できる小さな学習用実装を作ることである。

## Boundary Context
- **In scope**: x86_64 の callee-saved register と `rsp` の保存・復元、C から呼べる最小 context switch API、明示的・協調的な switch 経路、switch 元・先と context 値の serial log、既存 task 登録・dump log の維持、README またはコメントによる段階的な意味づけ。
- **Out of scope**: timer interrupt、preemption、割り込みハンドラからの context switch、セマフォ、タイマ、複雑な task 待ち状態、task 終了 lifecycle、DORMANT 遷移、μITRON 互換外部 API、既存 RTOS 実装の参照・コピー・流用。
- **Adjacent expectations**: scheduler は READY task の選択だけを維持し、dispatcher は current commit を維持し、context switch 実処理は arch 層または task context switch 層へ分離される。既存の QEMU 起動と serial log 確認は継続して利用できる。

## Requirements

### Requirement 1: 最小 context switch の観測可能な実行
**Objective:** As a RTOS 学習者, I want TCB 上の task context を使った最小 context switch を明示的に実行できること, so that metadata-only の段階から実際の register 保存・復元段階へ進んだことを確認できる

#### Acceptance Criteria
1. When 最小 context switch が明示的に呼び出される, the minimal-context-switch feature shall 現在 task と次 task の context を使って切り替えを試行する。
2. When 最小 context switch が実行される, the minimal-context-switch feature shall switch 元 task と switch 先 task を QEMU serial log で識別可能にする。
3. When 最小 context switch が完了後に serial log が確認される, the minimal-context-switch feature shall 既存の task 登録 log と task dump log を引き続き観測可能にする。
4. While 最小 context switch feature が有効である, the minimal-context-switch feature shall timer interrupt, preemption, or interrupt handler dispatch に依存せずに切り替えを実行する。

### Requirement 2: callee-saved register と stack pointer の保存
**Objective:** As a kernel 開発者, I want switch 元 task の保存対象 register を TCB 上の context に退避できること, so that 次にその task へ戻るための最小 CPU 状態を保持できる

#### Acceptance Criteria
1. When 最小 context switch が switch 元 task を受け取る, the minimal-context-switch feature shall switch 元 task の `rsp`, `rbp`, `rbx`, `r12`, `r13`, `r14`, and `r15` をその task context に保存する。
2. When switch 元 task context が保存される前後, the minimal-context-switch feature shall `context.rsp` の保存前後の値を QEMU serial log で確認可能にする。
3. If switch 元 task が context 保存に利用できない, then the minimal-context-switch feature shall 不正な context switch を成功として報告しない。
4. While 保存処理が行われる, the minimal-context-switch feature shall caller-saved register の完全保存を本featureの完了条件にしない。

### Requirement 3: callee-saved register と stack pointer の復元
**Objective:** As a kernel 開発者, I want switch 先 task の保存済み context から register を復元できること, so that CPU 実行を次 task の stack/context へ移せる

#### Acceptance Criteria
1. When 最小 context switch が switch 先 task を受け取る, the minimal-context-switch feature shall switch 先 task の `rsp`, `rbp`, `rbx`, `r12`, `r13`, `r14`, and `r15` を復元対象として扱う。
2. When switch 先 task context が復元対象になる, the minimal-context-switch feature shall 復元対象の `context.rsp` を QEMU serial log で確認可能にする。
3. If switch 先 task が context 復元に利用できない, then the minimal-context-switch feature shall 不正な context switch を成功として報告しない。
4. While 復元処理が行われる, the minimal-context-switch feature shall timer, interrupt, or preemption による非同期復元を実行しない。

### Requirement 4: 責務分離の維持
**Objective:** As a maintainer, I want scheduler, dispatcher, and context switch 実処理の責務を分けたままにすること, so that 第5章以降の拡張で境界が混ざらない

#### Acceptance Criteria
1. While scheduler が利用される, the minimal-context-switch feature shall scheduler の責務を READY task 選択に限定する。
2. While dispatcher が利用される, the minimal-context-switch feature shall dispatcher の責務を current commit と current 観測に限定する。
3. When context switch 実処理が必要になる, the minimal-context-switch feature shall register 保存・復元の詳細を scheduler に所有させない。
4. When context switch 実処理が必要になる, the minimal-context-switch feature shall register 保存・復元の詳細を dispatcher の current commit 責務と混同しない。

### Requirement 5: 段階的な実装制約と非対象範囲
**Objective:** As a RTOS 学習者, I want この段階で実装したことと未実装のことが明確に残ること, so that timer/preemption へ進む前の最小 switch 境界を誤解しない

#### Acceptance Criteria
1. Where public or internal context switch interfaces are documented, the minimal-context-switch feature shall Doxygen-style comments で目的、前提、制限、非対象範囲を説明する。
2. Where RUNNING の意味が context switch 導入で変わる可能性がある, the minimal-context-switch feature shall README またはコメントで現在の意味を説明する。
3. The minimal-context-switch feature shall 既存 RTOS 実装の source code, structure, or translated implementation を参照・コピー・流用しない。
4. The minimal-context-switch feature shall 学習用の小さな協調的 switch 経路として実装され、task 待ち状態、セマフォ、タイマ、preemption を追加しない。

### Requirement 6: build と QEMU smoke の継続
**Objective:** As a maintainer, I want 既存の build と QEMU serial 確認が壊れないこと, so that 最小 context switch の追加を既存の検証手順で確認できる

#### Acceptance Criteria
1. When `make` is run, the minimal-context-switch feature shall context switch 実装を含む kernel image を build できる。
2. When `make run` is run in the existing QEMU environment, the minimal-context-switch feature shall serial log で task 登録、dump、switch 元、switch 先、context.rsp の保存・復元対象を確認可能にする。
3. If QEMU serial log が確認される, then the minimal-context-switch feature shall 既存の task 登録 log と dump log の情報量を減らさない。
4. While build and smoke validation are performed, the minimal-context-switch feature shall 既存の QEMU 起動手順を不要に変更しない。
