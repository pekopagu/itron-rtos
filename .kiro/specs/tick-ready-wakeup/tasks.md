# Implementation Plan

## 1. delay queue tick 基盤を追加する

- [x] 1.1 `delay_queue_tick()` API と remaining tick 減算を追加する
  - `_Boundary:_ DelayQueue`
  - `timer_tick()` から呼べる public API があり、tick begin、entry remaining before/after、tick end をログで観測できる。
  - Requirements: 1.1, 1.2, 1.3, 1.4, 1.5

## 2. timeout 到達 task を READY へ復帰させる

- [x] 2.1 delay 待ち task の timeout READY 復帰を追加する
  - `_Boundary:_ TaskState, DelayQueue`
  - remaining が 0 になった `TASK_WAIT_REASON_DELAY` task が READY へ戻り、wait fields がクリアされ、delay queue から削除される。
  - Requirements: 2.1, 2.2, 2.3, 2.4, 2.5
  - _Depends: 1.1_

- [x] 2.2 timeout 付き semaphore 待ち task の timeout READY 復帰を追加する
  - `_Boundary:_ SemaphoreWaitQueue, TaskState, DelayQueue`
  - remaining が 0 になった `TASK_WAIT_REASON_SEMAPHORE_TIMEOUT` task が semaphore wait queue から削除され、READY へ戻り、delay queue から削除される。
  - Requirements: 3.1, 3.2, 3.3, 3.4, 3.5
  - _Depends: 1.1_

## 3. timer/preemption 統合と smoke を更新する

- [x] 3.1 `timer_tick()` と preemption pending 観測へ接続する
  - `_Boundary:_ Timer, PreemptionIntegration`
  - `timer_tick()` が delay queue tick 処理を呼び、READY 復帰後の高優先度 task が既存 IRQ preemption 経路で dispatch pending になることをログで観測できる。
  - Requirements: 4.1, 4.2, 4.3, 4.4, 4.5
  - _Depends: 2.1, 2.2_

- [x] 3.2 runtime smoke と成果物を 13.4 到達点へ更新する
  - `_Boundary:_ RuntimeSmoke, DocumentationEvidence`
  - README、Doxygen、`docs/logs/qemu-serial.log` が 13.4 の READY 復帰、timeout semaphore wait queue removal、非対応範囲を説明し、検証コマンドが通る。
  - Requirements: 5.1, 5.2, 5.3, 5.4, 5.5
  - _Depends: 3.1_

## Implementation Notes
