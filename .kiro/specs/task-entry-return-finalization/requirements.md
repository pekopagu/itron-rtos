# Requirements Document

## Project Description

第9章9.4では、μITRON風RTOSの学習用コンテキストスイッチsmokeにおいて、これまで未確定だった「task entry関数がreturnした後のtask状態」を確定する。現状は9.1のtask_bからtask_cへのtask-to-task context switch smoke、9.2の`dispatcher_switch_to(from, to)`境界、9.3のRUNNING/READY状態遷移が成立している一方、entry return後のtask lifecycleがREADY復帰なのか起動分の完了なのか曖昧である。今回、entry returnしたtaskはその起動分の実行完了として`TASK_STATE_DORMANT`へ遷移させ、プリエンプション、dispatch pending消費、interrupt exit接続、timer IRQからの実切替、μITRON風API完成には進めない。

## Requirements

### Requirement 1: entry return後のDORMANT確定

**User Story:** RTOS学習者として、task entryがreturnした後のtask状態をログとTCB状態で確認したい。そうすることで、entry returnをREADY復帰ではなく起動分の完了として扱う設計を明確に理解できる。

#### Acceptance Criteria

1. WHEN task entry関数がreturnする THEN システム SHALL 対象taskを`TASK_STATE_DORMANT`へ遷移させる。
2. WHEN entry returnしたtaskが直前に`TASK_STATE_RUNNING`だった THEN システム SHALL `RUNNING->DORMANT`として最終化を観測可能にする。
3. WHEN entry returnしたtaskが9.3のdispatcher遷移によって`TASK_STATE_READY`へ戻されていた THEN システム SHALL `READY->DORMANT`として最終化を観測可能にする。
4. WHEN entry return最終化が行われる THEN システム SHALL 対象taskをREADY候補へ戻さない。

### Requirement 2: 既存9.1から9.3の境界維持

**User Story:** RTOS開発者として、9.4の変更後も既存の9.1から9.3の観測ログと責務境界を維持したい。そうすることで、entry return lifecycle確定がdispatcherやschedulerの責務を侵食していないことを確認できる。

#### Acceptance Criteria

1. WHEN 通常起動smokeが実行される THEN システム SHALL task_bからtask_cへのtask-to-task context switch smokeを維持する。
2. WHEN `dispatcher_switch_to(from, to)`が実行される THEN システム SHALL switch boundary begin/endログを維持する。
3. WHEN `dispatcher_switch_to(from, to)`が実行される THEN システム SHALL `from RUNNING->READY`と`to READY->RUNNING`の状態遷移ログを維持する。
4. WHEN entry return最終化が実行される THEN システム SHALL dispatcher層のswitch boundary責務とtask_context層のentry return lifecycle責務を混同しない。

### Requirement 3: 非対象機能を未接続に保つ

**User Story:** RTOS開発者として、9.4がプリエンプション完成回ではないことを検証したい。そうすることで、timer IRQやinterrupt exit boundaryが未完成のまま安全に観測段階へ留まっていることを確認できる。

#### Acceptance Criteria

1. WHEN timer IRQ validation pathが実行される THEN システム SHALL timer IRQ handlerから`dispatcher_switch_to()`を呼ばない。
2. WHEN interrupt exit boundaryが実行される THEN システム SHALL dispatch pendingを消費せず、実dispatchへ接続しない。
3. WHEN 9.4が実装される THEN システム SHALL `yield_tsk`、`sta_tsk`、`ext_tsk`、`exd_tsk`、task再起動API、sleep/delay queue、同一優先度time slice、SMPを実装しない。
4. WHEN schedulerが呼ばれる THEN システム SHALL READY taskの選択責務だけを維持する。

### Requirement 4: 文書と検証ログの更新

**User Story:** 記事化とレビューを行う開発者として、README、Doxygenコメント、spec、QEMU serial logに9.4の到達点を残したい。そうすることで、後続章でdispatch pending消費やinterrupt exit接続へ進む前提を明確にできる。

#### Acceptance Criteria

1. WHEN READMEを読む THEN システム SHALL 第9章9.4の到達点と未実装範囲を確認できる。
2. WHEN Doxygenコメントを読む THEN システム SHALL task_context層がentry return後のlifecycle確定だけを担当することを確認できる。
3. WHEN `docs/logs/qemu-serial.log`を読む THEN システム SHALL entry return後のDORMANT最終化ログを確認できる。
4. WHEN spec成果物を確認する THEN システム SHALL `.kiro/specs/task-entry-return-finalization/`に`requirements.md`、`design.md`、`tasks.md`だけが残る。
