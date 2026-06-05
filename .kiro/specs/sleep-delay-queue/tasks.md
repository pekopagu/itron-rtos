# Implementation Plan

## 1. delay queue基盤を追加する

- [x] 1.1 delay queueの公開APIと固定長管理を追加する
  - `_Boundary:_ DelayQueue`
  - `kernel/include/delay_queue.h` と `kernel/delay_queue.c` が存在し、初期化、enqueue可否確認、enqueue、dumpを提供する。
  - Requirements: 1.1, 1.2, 1.3, 1.4, 3.1, 3.2, 3.5

- [x] 1.2 delay queueをbuildとkernel初期化へ接続する
  - `_Boundary:_ DelayQueue`
  - `make` の対象に `delay_queue.c` が含まれ、起動時にdelay queue初期化ログが出る。
  - Requirements: 1.1, 5.1

## 2. `dly_tsk()` とdelay queueを統合する

- [x] 2.1 `dly_tsk()` のWAITING化前にenqueue可否を確認する
  - `_Boundary:_ DelayTaskAPI`
  - queue満杯または二重enqueue時はWAITING化せず、失敗ログと `result=-1` を返す。
  - Requirements: 2.1, 3.1, 3.2, 3.3, 3.4

- [x] 2.2 delay WAITING化後にdelay queueへenqueueしてdumpする
  - `_Boundary:_ DelayTaskAPI`
  - queue entryにtask id/name/remaining/state/reasonが出力され、成功時actionが `delay-queued-switch` になる。
  - Requirements: 2.2, 2.3, 2.4, 4.2, 4.4, 5.2

## 3. 既存経路との分離を検証する

- [x] 3.1 semaphore wait queueとdelay queueの混入防止を確認する
  - `_Boundary:_ DelayQueue, SemaphoreWaitIsolation`
  - `sig_sem()` はdelay queue上のtaskをREADYへ戻さず、semaphore wait queueにはdelay待ちtaskが入らない。
  - Requirements: 4.1, 4.2, 4.3, 4.4

- [x] 3.2 timer IRQ handlerの直接呼び出し禁止を確認する
  - `_Boundary:_ TimerIRQCompatibility`
  - timer IRQ handler本体からtask APIまたは `dispatcher_switch_to()` を直接呼んでいないことを確認する。
  - Requirements: 5.3, 5.4

## 4. ドキュメントと検証ログを更新する

- [x] 4.1 READMEとDoxygenコメントに13.2到達点を記録する
  - `_Boundary:_ DocumentationEvidence`
  - READMEに `v13.2-sleep-delay-queue` 候補と未実装範囲が記載される。
  - Requirements: 5.5

- [x] 4.2 build/run検証とserial log更新を行う
  - `_Boundary:_ DocumentationEvidence`
  - `make`、`make run`、`make run VALIDATE_TIMER_IRQ_ENTRY=1` が確認され、`docs/logs/qemu-serial.log` がfresh `make run` 出力になる。
  - Requirements: 5.1, 5.2, 5.3, 5.5

## Implementation Notes

- 13.2ではdelay queue entryのremaining tickは観測用であり、tick decrementやREADY復帰には使わない。
- `dly_tsk()` はqueue可否をWAITING化前に確認し、失敗時に不整合なWAITING taskを残さない。
