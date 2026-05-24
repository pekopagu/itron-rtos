# Implementation Plan

- [x] 1. RUNNING current task の READY 化
  - `yield_tsk()` から RUNNING current task に限って `task_mark_ready_from_running()` を呼び出す。
  - READY 化成功時に `[yield] state transition: current ... RUNNING->READY` と `[yield] deferred: reason=scheduler-not-connected-yet` をログへ出す。
  - current 不在または非 RUNNING の reject は維持し、DORMANT task を READY へ戻さない。
  - 完了時には RUNNING current に対する `yield_tsk()` 呼び出しで READY 化ログが観測できる。
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 2.1, 2.2, 2.3, 2.4, 3.1_
  - _Boundary: ItronApi, TaskMgmt_

- [x] 2. 10.2 の非接続境界維持
  - `yield_tsk()` 内で `scheduler_select_next()`、`dispatcher_switch_to()`、`task_context_switch_to_task_pair()` を呼ばない。
  - timer IRQ handler から dispatcher switch を呼ばず、dispatch pending 消費も追加しない。
  - 必要な場合だけ RUNNING 中の限定的な `yield_tsk()` 観測点を boot smoke に追加し、READY 化後は次 task 選択や switch へ進めない。
  - 完了時には static review と smoke log で 10.2 が READY 化までで止まっていることを確認できる。
  - _Requirements: 3.2, 3.3, 3.4, 3.5_
  - _Boundary: ItronApi, KernelSmoke, Timer/Interrupt_

- [x] 3. 既存 smoke と文書更新
  - 9.1 task_b -> task_c、9.2 dispatcher switch boundary、9.3 RUNNING/READY、9.4 entry return -> DORMANT、10.1 yield reject ログを維持する。
  - README に 10.2 の到達点、未実装範囲、`v10.2-yield-running-to-ready` tag 候補を追記する。
  - Doxygen コメントを 10.2 の責務境界へ更新し、`docs/logs/qemu-serial.log` を `make run` の結果で更新する。
  - `.kiro/specs/yield-running-to-ready/` は `requirements.md`、`design.md`、`tasks.md` の3ファイルだけにする。
  - 完了時には `make`、`make run`、`make run VALIDATE_TIMER_IRQ_ENTRY=1` が通る。
  - _Requirements: 4.1, 4.2, 4.3, 4.4, 4.5, 4.6_
  - _Boundary: Documentation, Runtime log, Spec_
