# Implementation Plan

- [x] 1. semaphoreごとのFIFO wait queue基盤を追加する
  - `_Boundary:_ Semaphore Layer`
  - Requirements: 1, 6
  - `semaphore_t` に固定長task id配列、head、tail、countを追加し、`sem_init()` と `sem_create()` でemptyに初期化する。
  - enqueue/dequeue/empty helperを追加し、sem_id、task id/name、queue_countをログで観測できる。
  - Doxygenコメントが12.3の到達点と未実装範囲を説明している。

- [x] 2. `wai_sem()` のWAITING化経路をwait queue enqueueへ接続する
  - `_Boundary:_ ITRON API Layer`
  - `_Depends:_ 1`
  - Requirements: 2, 5
  - count 0で `task_mark_waiting_on_sem()` が成功した後、対象semaphoreのwait queueへcurrent task idがenqueueされる。
  - enqueue成功後も既存のscheduler選択、`dispatcher_switch_to()`、10.4 yield経路、12.1 RUNNING->WAITINGログが維持される。
  - enqueue失敗時は観測可能なエラーログを出し、switchへ進まない。

- [x] 3. `sig_sem()` をwait queue dequeue経由のREADY復帰へ置き換える
  - `_Boundary:_ Semaphore and Task Integration`
  - `_Depends:_ 1`
  - Requirements: 3, 4, 5
  - `sig_sem()` はtask table全体を探索せず、対象semaphoreのwait queueから1 taskだけdequeueする。
  - dequeueされたWAITING taskがREADYへ戻り、`wait_sem_id` が未待ち状態へ戻る。
  - 待ちtaskをREADYへ戻した場合はcountが増えず、queue空の場合だけcountが1増える。
  - Doxygenコメントがpriority順、timeout、wakeup後preemption、time slice、round-robin未実装を明示している。

- [x] 4. smoke、README、QEMU log、spec成果物を12.3へ更新する
  - `_Boundary:_ Kernel Smoke and Documentation`
  - `_Depends:_ 2, 3`
  - Requirements: 5, 6
  - `make run` でenqueue/dequeue/empty、READY復帰、count-upのログが確認できる。
  - READMEに12.3到達点と必要なZenn tag候補が記載される。
  - `docs/logs/qemu-serial.log` が最新の `make run` 出力で更新される。
  - `.kiro/specs/semaphore-wait-queue/` は最終的に `requirements.md`, `design.md`, `tasks.md` の3ファイルだけになる。

- [x] 5. 統合検証を実行する
  - `_Boundary:_ Validation`
  - `_Depends:_ 4`
  - Requirements: 5, 6
  - `make`, `make run`, `make run VALIDATE_TIMER_IRQ_ENTRY=1` が通る。
  - timer IRQ handler本体から `sig_sem()`、`wai_sem()`、`yield_tsk()`、`dispatcher_switch_to()` を直接呼んでいないことを確認できる。
  - FIFO順のdequeueが最低限ログから確認できる。

## Implementation Notes
