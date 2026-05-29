# Implementation Plan

- [x] 1. 協調yieldからdispatcher/context switchへ接続する
  - `yield_tsk()` がRUNNING currentをREADYへ戻した後、next READY taskがある場合にswitch begin/endをログし、dispatcher境界へ進む。
  - dispatcher境界は10.4の協調API経由でREADY化済みfromを扱い、to taskのREADY->RUNNING、current更新、task_context委譲を観測できる。
  - `make run` のログに `[yield] switch begin`, `[dispatcher] switch boundary begin`, `[context] task-to-task switch begin`, `[context] task-to-task switch end`, `[yield] switch end` が出る。
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 2.3_
  - _Boundary: YieldAPI, Dispatcher, TaskContext_

- [x] 2. invalid current、DORMANT保護、no-next deferralを維持する
  - currentなし、非RUNNING current、DORMANT currentを従来どおりrejectし、READYへ戻さない。
  - next READY taskが存在しない場合はdispatcher境界へ進まず、`no-ready-task` と `deferred: reason=no-next-task` をログする。
  - 9.4のentry return -> DORMANT確定と、10.1-10.3の観測ログが回帰していないことを `make run` で確認できる。
  - _Requirements: 2.1, 2.2, 2.3, 2.4_
  - _Boundary: YieldAPI, TaskContext_

- [x] 3. interrupt/preemption非接続の境界を検証する
  - timer IRQ handler、interrupt exit boundary、dispatch pending経路から `yield_tsk()` や `dispatcher_switch_to()` を呼ばない状態を維持する。
  - `make run VALIDATE_TIMER_IRQ_ENTRY=1` でtimer IRQ observation pathが壊れず、dispatcher/context switchへ接続されていないことを確認できる。
  - _Requirements: 3.1, 3.2, 3.3, 3.4_
  - _Boundary: InterruptValidation, DispatchPending_

- [x] 4. README、Doxygen、QEMU log、spec artifactsを更新する
  - READMEの進捗表とZenn Articles表へ `v10.4-yield-connect-context-switch` を追加し、10.4の到達点と未接続範囲を記載する。
  - public/internal Doxygenコメントを日本語で更新し、10.4が協調APIからdispatcher/context switchへ接続するがtimer IRQ・dispatch pending・preemptive switchへは接続しないことを明記する。
  - `docs/logs/qemu-serial.log` を `make run` の10.4ログで更新する。
  - 最終的に `.kiro/specs/yield-connect-context-switch/` が `requirements.md`, `design.md`, `tasks.md` の3ファイルだけになる。
  - _Requirements: 4.1, 4.2, 4.3, 4.4_
  - _Boundary: Documentation, SpecArtifacts_
