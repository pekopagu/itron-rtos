# Implementation Plan

## 1. task生成APIの基盤を追加する

- [x] 1.1 `cre_tsk()` の公開契約とDORMANT登録helperを追加する
  - `itron_task_create_param_t` と `cre_tsk()` 宣言をAPI headerから利用できる。
  - `task_create_dormant()` が未使用TCBに指定IDのtaskをDORMANTとして登録し、`task_register()` のREADY登録契約は維持される。
  - `cre_tsk()` 呼び出し、task登録、初期状態DORMANT、完了resultをlogで観測できる。
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 2.1, 2.2, 2.3, 2.4_
  - _Boundary: ItronTaskAPI, TaskState_

## 2. task起動APIとpreemption接続を追加する

- [x] 2.1 `sta_tsk()` とDORMANT->READY起動helperを追加する
  - `sta_tsk()` がDORMANT taskだけをREADYへ遷移させ、scheduler READY候補として観測できる。
  - READY / RUNNING / WAITING / unknown ID に対する `sta_tsk()` は状態を変更せず失敗する。
  - `sta_tsk()` 呼び出し、DORMANT->READY、不正状態、完了resultをlogで観測できる。
  - _Requirements: 3.1, 3.2, 3.3, 3.4, 4.1, 4.2, 4.3, 4.4, 4.5_
  - _Boundary: ItronTaskAPI, TaskState, Scheduler_
  - _Depends: 1.1_

- [x] 2.2 `sta_tsk()` READY化後のpreemption pending接続を追加する
  - READY化したtaskがcurrentより高優先度の場合、既存pending request境界へ接続される。
  - 高優先度でない場合は新しいpendingをrequestしない。
  - current / ready candidate / priority / `reason=task-start` をlogで観測できる。
  - _Requirements: 5.1, 5.2, 5.3, 5.4, 5.5_
  - _Boundary: PreemptionIntegration, DispatchPending_
  - _Depends: 2.1_

## 3. runtime smokeと成果物を更新する

- [x] 3.1 14.1 boot-time smokeと検証成果物を更新する
  - `make`, `make run`, `make run VALIDATE_TIMER_IRQ_ENTRY=1` が通る。
  - `make run` のserial logに `cre_tsk()` DORMANT、`sta_tsk()` READY、invalid-state、preemption pendingのevidenceが残る。
  - README、Doxygen、`docs/logs/qemu-serial.log`、specに14.1の到達点と非対応範囲を記載する。
  - `.kiro/specs/create-start-task-api/` は最終的に `requirements.md`, `design.md`, `tasks.md` の3ファイルだけになる。
  - _Requirements: 6.1, 6.2, 6.3, 6.4, 6.5, 6.6_
  - _Boundary: RuntimeSmoke, DocumentationEvidence_
  - _Depends: 2.2_

## Implementation Notes
