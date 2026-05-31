# Implementation Plan

- [x] 1. `sig_sem()` のwakeup/count-up挙動を12.2ログへ更新する
  - `_Boundary:_ Semaphore Layer`
  - Requirements: 1, 2, 3
  - `sig_sem(int sem_id)` が呼び出し、WAITING task発見、wakeup-no-switch、no waiting task、count-upを `[sig-sem]` ログで観測できる。
  - WAITING taskをREADYへ戻した場合、semaphore countが増えない。
  - 待ちtaskがいない場合だけcountが1増える。

- [x] 2. task READY復帰ログとDoxygenコメントを12.2向けに整える
  - `_Boundary:_ Task Layer`
  - Requirements: 2, 5
  - `task_wake_one_waiting_on_sem()` が `wait_sem_id` を未待ち状態へ戻し、READY復帰ログで `wait_sem_id=none state=READY` を確認できる。
  - コメントがwait queue、FIFO/priority順、preemption、timeout、time slice、round-robinをまだ扱わないことを説明している。

- [x] 3. boot-time smokeとREADME/logを12.2へ更新する
  - `_Boundary:_ Kernel Smoke and Documentation`
  - Requirements: 4, 5
  - `make run` で12.1のWAITING化後に `sig_sem()` のREADY復帰と、待ちtaskなしcount-upを観測できる。
  - READMEに12.2到達点と未実装範囲、必要なZenn tag候補が記載される。
  - `docs/logs/qemu-serial.log` が最新の `make run` 出力で更新される。

- [x] 4. 統合検証を実行する
  - `_Boundary:_ Validation`
  - Requirements: 4, 5
  - `make`, `make run`, `make run VALIDATE_TIMER_IRQ_ENTRY=1` が通る。
  - timer IRQ handler本体から `wai_sem()`、`sig_sem()`、`yield_tsk()`、`dispatcher_switch_to()` を直接呼んでいないことを確認する。
  - `.kiro/specs/sig-sem-waiting-to-ready/` が `requirements.md`, `design.md`, `tasks.md` の3ファイルだけである。

## Implementation Notes
