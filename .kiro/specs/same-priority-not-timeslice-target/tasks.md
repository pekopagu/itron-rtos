# Implementation Plan

- [x] 1. scheduler/preemptionの同一優先度READY除外を11.3として固定する
  - `candidate->priority < current->priority` の場合だけpreemption対象にし、同一priorityでは `same-priority-not-timeslice-target` を維持する。
  - Doxygenコメントで、同一優先度READYは11.3時点ではtime slice対象外であり、round-robinやtick count slice管理をまだ導入しないことを明記する。
  - 完了状態として、同一優先度READYのみのpreemptionログに `reason=same-priority-not-timeslice-target` が出る。
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 4.3_
  - _Boundary: SchedulerPreemptionDecision, PreemptionIRQAPI_

- [x] 2. dispatch pending no-request/no-pending境界を11.3証跡として安定化する
  - 同一優先度READYのみの場合にdispatch pendingをrequestしない既存経路を維持する。
  - interrupt exit boundaryで `dispatch-pending=none action=no-dispatch` と `consume skipped: reason=no-pending` が観測できることをコメントとログで確認する。
  - 完了状態として、同一優先度READYだけでは `requested` や `dispatcher_switch_to()` 経路へ進まない。
  - _Requirements: 2.1, 2.2, 2.3, 2.4, 2.5, 3.5_
  - _Boundary: DispatchPendingAPI, TimerIRQExitBoundary_
  - _Depends: 1_

- [x] 3. 11.2高優先度READY deferred dispatchと10.4 yield経路を回帰確認する
  - 高優先度READYが存在する場合は `higher-priority-ready` でpending request/consume/deferred dispatchへ進む経路を維持する。
  - 通常runで10.4 `yield_tsk()`協調context switch経路が維持されることを確認する。
  - 完了状態として、validation runに高優先度READYのdeferred dispatchログが残り、通常runにyield switchログが残る。
  - _Requirements: 3.1, 3.2, 3.3, 3.4_
  - _Boundary: PreemptionIRQAPI, DispatchPendingAPI, YieldAPI_
  - _Depends: 1, 2_

- [x] 4. README、serial log、spec成果物を11.3として更新して検証する
  - READMEの進捗表とZenn Articles表に `v11.3-same-priority-not-timeslice-target` を追記し、11.3の到達点と未実装範囲を記載する。
  - `make`、`make run`、`make run VALIDATE_TIMER_IRQ_ENTRY=1` を実行し、`docs/logs/qemu-serial.log` をfresh evidenceで更新する。
  - `.kiro/specs/same-priority-not-timeslice-target/` を最終的に `requirements.md`、`design.md`、`tasks.md` の3ファイルだけにする。
  - 完了状態として、build/run検証結果とspec成果物の3ファイル制約を確認できる。
  - _Requirements: 4.1, 4.2, 4.4, 4.5_
  - _Boundary: DocumentationEvidence, SpecArtifacts, ValidationEvidence_
  - _Depends: 1, 2, 3_
