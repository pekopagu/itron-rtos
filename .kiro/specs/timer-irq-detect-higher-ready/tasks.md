# Implementation Plan

- [x] 1. scheduler/preemption判断ログを11.1へ更新する
  - schedulerのpreemption判断で、currentよりpriority値が小さいREADY taskだけをswitch対象にし、同一priority READYのみの場合は`same-priority-not-timeslice-target`として分類する。
  - preemption層でcurrent taskとhigher-ready candidateのid/name/prio/stateをログに出し、no-switch理由を`no-higher-priority-ready`または`same-priority-not-timeslice-target`として観測できるようにする。
  - 完了状態として、timer IRQ由来ログに`[preempt-irq] current: ...`、`higher-ready detected`または`no higher-ready`、`decision evaluated`が出る。
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 2.3, 2.4_
  - _Boundary: SchedulerPreemptionDecision, PreemptionIRQAPI_

- [x] 2. dispatch pending request/observeをfrom-to付きにする
  - dispatch pending request APIをcurrent/candidate付きに更新し、`reason=higher-priority-ready from ... to ...`形式で観測できるようにする。
  - not-requested pathはpendingをrequestせず、decision reasonだけをログへ出す。
  - 完了状態として、higher-ready時は`[dispatch-pending] requested: reason=higher-priority-ready from id=... to id=...`が出て、no-switch時は`not-requested`が出る。
  - _Requirements: 2.1, 2.2, 2.3, 2.4, 2.5, 3.3, 3.4_
  - _Boundary: DispatchPendingAPI_
  - _Depends: 1_

- [x] 3. timer IRQ validation状態と非切替境界を維持する
  - `VALIDATE_TIMER_IRQ_ENTRY=1`時に限定して、timer IRQ発生前にtask_a RUNNING priority=5、task_b READY priority=1、task_c READY同等候補の検証状態を作る。
  - timer IRQ handlerとinterrupt exit boundaryは実dispatch、pending消費、`yield_tsk()`、`dispatcher_switch_to()`へ接続しない。
  - 完了状態として、validation runでtimer IRQ pathがhigher-ready検出とdispatch pending requestedを観測し、exit boundaryは`action=not-dispatched-yet`のままになる。
  - _Requirements: 3.1, 3.2, 3.3, 3.4, 4.3_
  - _Boundary: TimerIRQHandler, ValidationSetup_
  - _Depends: 1, 2_

- [x] 4. README、Doxygen、ログ、spec成果物を更新して検証する
  - READMEに11.1の到達点とtag候補`v11.1-timer-irq-detect-higher-ready`を追加し、実dispatch・pending消費・preemptive context switch未接続を明記する。
  - `docs/logs/qemu-serial.log`を新しいvalidation証跡で更新する。
  - `make`、`make run`、`make run VALIDATE_TIMER_IRQ_ENTRY=1`を実行し、通常smokeとtimer IRQ validationを確認する。
  - 最終状態で`.kiro/specs/timer-irq-detect-higher-ready/`が`requirements.md`、`design.md`、`tasks.md`だけになる。
  - _Requirements: 3.5, 4.1, 4.2, 4.4, 4.5, 4.6, 4.7_
  - _Boundary: DocumentationEvidence, SpecArtifacts_
  - _Depends: 1, 2, 3_
