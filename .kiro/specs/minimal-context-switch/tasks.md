# Implementation Plan

- [x] 1. x86_64 register 実保存に必要な build/runtime 前提を整える
- [x] 1.1 64-bit register を扱える boot と link の前提を更新する
  - x86_64 の `rsp`, `r12`-`r15` を実命令で扱える kernel image として build できる状態にする。
  - 既存の QEMU `make run` 起動手順は維持する。
  - `make` が kernel image を生成できることを完了状態とする。
  - _Requirements: 2.1, 3.1, 6.1, 6.4_
  - _Boundary: BuildRuntime_

- [x] 2. arch context switch primitive を追加する
- [x] 2.1 x86_64 callee-saved register と stack pointer の保存・復元境界を追加する
  - switch 元 context に `rsp`, `rbp`, `rbx`, `r12`, `r13`, `r14`, `r15` が保存される。
  - switch 先 context から同じ register 群が復元される。
  - primitive は log、scheduler、dispatcher の責務を持たない。
  - build artifact に context switch assembly object が含まれることを完了状態とする。
  - _Requirements: 2.1, 2.4, 3.1, 3.4, 4.3, 4.4_
  - _Boundary: ArchContextSwitch_

- [x] 3. TCB-level context switch wrapper を追加する
- [x] 3.1 context switch 用の task stack 初期frameと validation を追加する
  - 登録済み task の `context.rsp` に、初回 switch 後に到達できる戻り先が準備される。
  - NULL task や不正 stack metadata は成功扱いにしない。
  - 初回 task stack に入れる task だけが prepared として扱われることを serial log または smoke flow で確認できる。
  - _Requirements: 1.1, 2.3, 3.3, 5.1_
  - _Boundary: TaskContextSwitch_

- [x] 3.2 switch 元・先と context.rsp を観測できる C wrapper を追加する
  - switch 元 task、switch 先 task、保存前 `context.rsp`、保存後 `context.rsp`、復元対象 `context.rsp` が serial log に出る。
  - wrapper は scheduler selection や dispatcher commit を実行しない。
  - 不正入力では arch primitive を呼ばず、失敗が観測できる。
  - _Requirements: 1.2, 2.2, 2.3, 3.2, 3.3, 4.3, 4.4_
  - _Boundary: TaskContextSwitch_

- [x] 4. 起動時の明示的 context switch smoke を統合する
- [x] 4.1 scheduler と dispatcher の既存責務を保ったまま minimal switch smoke を呼び出す
  - READY task 選択は scheduler、current commit は dispatcher、switch 実処理は task context wrapper に分かれている。
  - boot context から committed task stack へ入り、entry return 後に boot context へ戻る。
  - `make run` の serial log で switch 前後の流れが追跡できる。
  - _Requirements: 1.1, 1.4, 4.1, 4.2, 6.2_
  - _Boundary: KernelSwitchSmoke_

- [x] 5. ドキュメントと既存logの整合性を更新する
- [x] 5.1 README とコメントに第5章5.3の境界を反映する
  - README に 5.3 の進捗、RUNNING の現在の意味、timer/preemption 未導入であることを追記する。
  - public/internal interface の Doxygen comment が目的、前提、制限、非対象範囲を説明している。
  - 既存の task 登録 log と dump log の情報量が減っていないことを確認できる。
  - _Requirements: 1.3, 5.1, 5.2, 5.3, 5.4, 6.3_
  - _Boundary: KernelSwitchSmoke, TaskContextSwitch_

- [x] 6. build と QEMU serial smoke を検証する
- [x] 6.1 feature 全体の validation を実行し、結果を反映する
  - `make` が成功する。
  - `make run` が成功し、serial log に task 登録、dump、switch 元、switch 先、context.rsp の保存・復元対象が出る。
  - scheduler に arch context switch 呼び出しが混入していないことを確認する。
  - `$kiro-validate-impl minimal-context-switch` の結果に対応できる状態にする。
  - _Requirements: 1.3, 6.1, 6.2, 6.3, 6.4_
  - _Boundary: BuildRuntime, KernelSwitchSmoke_

## Implementation Notes
- 1.1: QEMU `-kernel` はELF64を直接読めないため、x86_64 link後に `llvm-objcopy -O elf32-i386` でMultiboot用ELF32コンテナへ変換する。
- 4.1: 5.3 smokeはboot contextからtask stackへ入り、entry return後にboot contextへ戻る。既存4.3 cooperative runnerは比較用の直接C呼び出しmodelとして残した。
