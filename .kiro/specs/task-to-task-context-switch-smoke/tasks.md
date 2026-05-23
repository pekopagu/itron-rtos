# Implementation Plan

- [x] 1. task context層にtask-to-task smoke境界を追加する
  - `task_context_switch_to_task_pair()` を追加し、2つのtask stack frameを準備してbootからfirstへ入れる。
  - `task_context_enter()` のentry return観測点で、一度だけfirstからsecondへ切り替えるsmoke stateを扱う。
  - 完了状態として、ログに `[context] task-to-task switch begin:` が出力され、second task entryがtrampoline経由で実行される。
  - _Requirements: 1.1, 1.2, 1.3, 2.1, 2.2, 2.3, 2.4, 2.5, 3.2, 3.3, 3.4, 3.5_
  - _Boundary: TaskContextPairSmoke_

- [x] 2. kernel smoke coordinatorを9.1向けに拡張する
  - 既存の `kernel_run_minimal_context_switch_smoke()` を2 task入力に拡張し、最初のtaskをdispatcher currentとしてcommitした後、次taskをtask-to-task smoke対象に渡す。
  - 後続のsemaphore/cooperative smokeが壊れないよう、entry return後のREADY復帰を維持する。
  - 完了状態として、通常boot smokeがcontext smoke後も継続し、既存のcooperative flowへ進む。
  - _Requirements: 1.1, 1.2, 1.3, 2.1, 2.4, 3.1, 3.4_
  - _Boundary: BootSmokeCoordinator_
  - _Depends: 1_

- [x] 3. READMEと検証ログを9.1へ更新する
  - READMEに9.1の到達点、非ゴール、task-to-task smokeの位置づけを追加する。
  - Zenn Articles表に `v9.1-task-to-task-context-switch-smoke` を追加する。
  - `docs/logs/qemu-serial.log` を `make run` の9.1証跡で更新する。
  - 完了状態として、READMEとログから9.1が起動時smoke拡張であり実dispatcher/IRQ切替ではないことを確認できる。
  - _Requirements: 3.1, 4.3, 4.4, 4.5_
  - _Boundary: DocumentationEvidence_
  - _Depends: 1, 2_

- [x] 4. build、smoke、timer IRQ validationを検証する
  - `make`、`make run`、`make run VALIDATE_TIMER_IRQ_ENTRY=1` を実行する。
  - source reviewでtimer IRQ handlerがtask context switchを呼ばず、task context層がdispatch pendingを消費しないことを確認する。
  - 完了状態として、全タスクが完了し、検証結果と9.1の非接続境界を報告できる。
  - _Requirements: 3.2, 3.3, 4.1, 4.2_
  - _Boundary: ValidationEvidence_
  - _Depends: 1, 2, 3_

## Implementation Notes
