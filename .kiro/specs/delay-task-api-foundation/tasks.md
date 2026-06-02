# Implementation Plan

## 1. Task wait reason基盤を追加する

- [x] 1.1 TCBにwait reasonとdelay残tickの観測フィールドを追加する
  - `_Boundary:_ TaskWaitReasonState`
  - `task_wait_reason_t`、`wait_reason`、`delay_ticks_remaining` が定義され、初期化・登録ログ・dumpで観測できる。
  - Requirements: 2.2, 2.3, 3.5

- [x] 1.2 semaphore待ち遷移をwait reason modelへ接続する
  - `_Boundary:_ TaskWaitReasonState`
  - `wai_sem()` 経由のWAITING taskが `wait_reason=semaphore` を持ち、READY復帰時に待ち情報がclearされる。
  - Requirements: 3.1, 3.2, 3.3, 3.4, 5.5

## 2. `dly_tsk()` APIを追加する

- [x] 2.1 delay WAITING遷移APIをtask moduleに追加する
  - `_Boundary:_ TaskWaitReasonState`
  - RUNNING taskだけがdelay WAITINGへ遷移し、`wait_sem_id=0` と `delay_ticks_remaining=delay_ticks` がログで確認できる。
  - Requirements: 2.1, 2.2, 2.3, 2.4, 2.5

- [x] 2.2 `dly_tsk(uint32_t delay_ticks)` の入力検証とinvalid-delay経路を追加する
  - `_Boundary:_ DelayTaskAPI`
  - `delay_ticks == 0` で状態を変えず、result `-1` / action `invalid-delay` をログ出力する。
  - Requirements: 1.1, 1.2, 1.3, 1.4, 1.5

- [x] 2.3 `dly_tsk()` をscheduler/dispatcher switch境界へ接続する
  - `_Boundary:_ DelayTaskAPI`
  - delay WAITING化後に次READYを選び、存在すれば `dispatcher_switch_to()`、存在しなければno-readyログで安全に戻る。
  - Requirements: 4.1, 4.2, 4.3, 4.4, 4.5

## 3. 起動時検証と既存経路の維持を確認する

- [x] 3.1 kernel smokeにdelay task検証を追加する
  - `_Boundary:_ DelayTaskAPI`
  - `make run` ログでinvalid-delayとdelay-switchが確認でき、既存yield/semaphore smokeも継続して実行される。
  - Requirements: 1.1, 1.4, 2.5, 4.2, 4.4, 5.2, 5.5

- [x] 3.2 timer IRQ handlerとsemaphore wait queueの分離を検証する
  - `_Boundary:_ SemaphoreWaitIsolation`
  - delay待ちtaskがsemaphore wait queueに混ざらず、timer IRQ handler本体からtask文脈APIやdispatcherを直接呼ばないことを確認できる。
  - Requirements: 3.3, 3.4, 5.3, 5.4

## 4. ドキュメントと検証ログを更新する

- [x] 4.1 READMEとDoxygenコメントに13.1の到達点を記録する
  - `_Boundary:_ DocumentationEvidence`
  - READMEに13.1と `v13.1-delay-task-api-foundation` 候補、未実装範囲が記載される。
  - Requirements: 6.1, 6.2

- [x] 4.2 build/run検証を実行してserial logとspec成果物を整える
  - `_Boundary:_ DocumentationEvidence`
  - `make`、`make run`、`make run VALIDATE_TIMER_IRQ_ENTRY=1` が確認され、`docs/logs/qemu-serial.log` がfresh `make run` 出力になり、specディレクトリは3ファイルだけになる。
  - Requirements: 5.1, 5.2, 5.3, 6.3, 6.4

## Implementation Notes

- 13.1では `delay_ticks == 0` をno-opではなく `DLY_TSK_ERR_INVALID_DELAY` とし、状態変更前に返す。
- delay待ちは `wait_reason=delay` と `delay_ticks_remaining` で観測し、`wait_sem_id` はsemaphore待ち専用の意味を維持する。
