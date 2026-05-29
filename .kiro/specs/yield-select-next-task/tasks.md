# Implementation Plan

- [x] 1. `yield_tsk()` にREADY化後のscheduler候補選択を追加する
  - RUNNING current taskをREADYへ戻した直後に `scheduler_select_next()` を呼び、選択結果を読み取り専用で扱う。
  - 候補がある場合は `[yield] next selected: id=... name=... prio=... state=READY` を出力する。
  - 候補がない場合は `[yield] no next task: reason=no-ready-task` を出力する。
  - 完了時には `yield_tsk()` が次task候補を観測してもdispatcher currentを更新せず、実switchへ進まない。
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 2.1, 2.2, 2.3, 3.1, 3.2, 3.3, 3.4, 4.1, 4.2, 4.3, 4.4_
  - _Boundary: ItronApi, Scheduler_

- [x] 2. 10.3の非接続境界と既存smoke維持を確認する
  - `yield_tsk()` 内で `dispatcher_switch_to()`、`task_context_switch_to_task_pair()`、dispatcher current commit、dispatch pending消費を行わないことを静的に確認できる状態にする。
  - timer IRQ handlerからdispatcher switchを呼ばず、dispatch pendingを消費しない既存境界を維持する。
  - 9.1 task_b -> task_c、9.2 dispatcher switch boundary、9.3 RUNNING/READY、9.4 entry return -> DORMANT、10.2 RUNNING->READYログを維持する。
  - 完了時には `make run` と `make run VALIDATE_TIMER_IRQ_ENTRY=1` のログで10.3追加と既存観測点が同時に確認できる。
  - _Requirements: 4.1, 4.2, 4.3, 4.4, 4.5, 5.1, 5.2, 5.3, 5.4_
  - _Boundary: KernelSmoke, Timer/Interrupt, StaticReview_

- [x] 3. README、Doxygenコメント、実行ログ、specを更新する
  - READMEに第10章10.3の到達点、未実装範囲、`v10.3-yield-select-next-task` tag候補を追記する。
  - `itron_api`、task管理、kernel smokeのDoxygenコメントを10.3の候補選択境界へ更新する。
  - `docs/logs/qemu-serial.log` を `make run` の10.3ログで更新する。
  - `.kiro/specs/yield-select-next-task/` は最終的に `requirements.md`、`design.md`、`tasks.md` の3ファイルだけにする。
  - 完了時には `make`、`make run`、`make run VALIDATE_TIMER_IRQ_ENTRY=1` が通る。
  - _Requirements: 5.5, 5.6_
  - _Boundary: Documentation, RuntimeLog, Spec_
