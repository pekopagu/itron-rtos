# Implementation Plan

- [x] 1. `sig_sem()` wakeup後preemption判定を実装する
  - READY復帰後にcurrent RUNNING taskとwoken READY taskのpriorityを比較する。
  - woken taskのpriority値がcurrentより小さい場合だけ `dispatcher_switch_to()` へ進む。
  - 同一優先度と低優先度ではno-switchログを出し、time sliceやround-robinには進まない。
  - 完了状態として、`make run` のログにpreempt check、required/not required、switch begin/end、`wakeup-switch` または `wakeup-no-switch` が出る。
  - _Requirements: 1, 2, 3, 4_
  - _Boundary: Semaphore Layer, DispatcherSwitchBoundary_

- [x] 2. smokeと文書成果物を12.4へ更新する
  - `kernel/kernel.c` のsemaphore smokeを高優先度wakeup switchが観測できる設定にする。
  - READMEに12.4の到達点、未実装範囲、必要なZenn tag候補を追記する。
  - `docs/logs/qemu-serial.log` を最新 `make run` 出力で更新する。
  - `.kiro/specs/semaphore-wakeup-preemption/` は最終的に `requirements.md`, `design.md`, `tasks.md` の3ファイルだけにする。
  - _Requirements: 4, 5_
  - _Boundary: Kernel Smoke and Documentation_

- [x] 3. 統合検証を実行する
  - `make`、`make run`、`make run VALIDATE_TIMER_IRQ_ENTRY=1` が通る。
  - `rg` でtimer IRQ handler本体から `sig_sem()`、`wai_sem()`、`yield_tsk()`、`dispatcher_switch_to()` を直接呼んでいないことを確認する。
  - 12.3のFIFO wait queue、count制御、`wait_sem_id` clear、10.4 yield、11.4 deferred dispatch/no-dispatchが維持されていることをログから確認する。
  - _Requirements: 3, 4, 5_
  - _Boundary: Validation_
