# Implementation Plan

- [x] 1. `wai_sem()` task文脈APIを追加する
  - `_Boundary:_ ITRON API Layer`
  - Requirements: 1, 2, 3, 4
  - `wai_sem(int sem_id)`がdispatcher currentを読み、currentなし/非RUNNINGをエラーとしてログする。
  - countがある場合はcountを減らし、`action=no-switch`で完了する。
  - countが0の場合はRUNNING->WAITING、次READY選択、switch begin/endまで観測できる。

- [x] 2. セマフォ層をcount操作責務へ整理する
  - `_Boundary:_ Semaphore Layer`
  - Requirements: 2, 3
  - セマフォtable探索とcount減算helperを提供し、scheduler/dispatcher責務を持たない。
  - 旧task_id付き`wai_sem()`経路を12.1のtask文脈APIへ置き換える。

- [x] 3. dispatcher境界でWAITING fromを受け付ける
  - `_Boundary:_ Dispatcher Layer`
  - Requirements: 4, 5
  - `yield_tsk()`由来のREADY fromと既存RUNNING fromを壊さず、12.1のWAITING fromを許可する。
  - WAITING fromにはRUNNING->READY遷移を適用しない。

- [x] 4. boot-time smokeと文書を更新する
  - `_Boundary:_ Kernel Smoke and Documentation`
  - Requirements: 5, 6
  - `make run`で取得成功no-switchと取得不能WAITING->次READY switchを確認できる。
  - README、Doxygenコメント、`docs/logs/qemu-serial.log`、specを12.1の到達点へ更新する。

- [x] 5. 統合検証を実行する
  - `_Boundary:_ Validation`
  - Requirements: 5, 6
  - `make`, `make run`, `make run VALIDATE_TIMER_IRQ_ENTRY=1`が通る。
  - timer IRQ handler本体から`wai_sem()`、`yield_tsk()`、`dispatcher_switch_to()`を直接呼んでいないことを確認する。
  - specディレクトリが3ファイルだけであることを確認する。

## Implementation Notes

- 12.1ではWAITING化済みfrom taskを再実行しないため、dispatcherのWAITING-from経路はto taskの単独context smokeへ接続する。
