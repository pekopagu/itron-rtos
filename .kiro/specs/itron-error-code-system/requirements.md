# Requirements Document

## Introduction
第14章14.4では、μITRON風RTOSの公開API層でばらついていた `0` / `-1` / `-2` / `-3` などの戻り値を、共通のエラーコード体系へ整理する。利用者は `cre_tsk()` / `sta_tsk()` / `slp_tsk()` / `wup_tsk()` / `wai_sem()` / `sig_sem()` / `pol_sem()` / `twai_sem()` / `dly_tsk()` / `yield_tsk()` の成功・失敗理由を、同じ `ER` 型と `E_OK` / `E_ID` / `E_PAR` / `E_OBJ` / `E_CTX` / `E_TMOUT` / `E_RLWAI` / `E_QOVR` の意味で追跡できる必要がある。既存のtask、semaphore、delay queue、dispatcher、preemptionの状態遷移は維持し、戻り値、ログ、README、Doxygenコメントで14.4の到達点を明確にする。

## Boundary Context
- **In scope**: μITRON風API層の戻り値型、共通エラーコード、対象APIの戻り値変換、完了ログ、README、Doxygenコメント、QEMU serial log更新。
- **Out of scope**: 本物のμITRON仕様の完全再現、全エラーコード網羅、service call contextの本格判定、CPU lock、dispatch disable、interrupt contextの本格判定、未実装APIの追加、既存RTOSコードの参照・コピー・流用。
- **Adjacent expectations**: task module、semaphore module、delay queue module、dispatcher、preemption、timer IRQ deferred dispatchの既存状態遷移は変更しない。timer IRQ handler本体から直接APIや `dispatcher_switch_to()` を呼ばない方針を維持する。

## Requirements

### Requirement 1: 共通エラーコード体系
**Objective:** 学習用RTOSの利用者として、公開API層の戻り値を共通の意味で読めるようにしたい。これにより、章ごとに追加されたAPIの成功・失敗理由を一貫して追跡できる。

#### Acceptance Criteria
1. When μITRON風API層の公開ヘッダを確認するとき、システムは `typedef int ER;` と必要に応じて `typedef int ID;` を提供する。
2. When APIが正常完了するとき、システムは成功を `E_OK` として返す。
3. When APIが不正IDを検出するとき、システムは `E_ID` を返す。
4. When APIがNULL pointerや不正timeout値などの不正引数を検出するとき、システムは `E_PAR` を返す。
5. When API要求と対象taskまたはsemaphoreの状態が合わないとき、システムは `E_OBJ` を返す。
6. When APIが今回判定できる範囲で不正な呼び出し文脈を検出するとき、システムは `E_CTX` を返す。
7. When poll取得失敗またはtimeout到達が発生するとき、システムは `E_TMOUT` を返す。
8. The system shall define only the minimum shared error codes needed for this chapter, including `E_RLWAI` and `E_QOVR` where useful for future-visible meaning.

### Requirement 2: 対象APIの戻り値統一
**Objective:** 学習用RTOSの利用者として、既存APIの戻り値をAPIごとの独自負数ではなく共通エラーコードで扱いたい。これにより、task API、semaphore API、delay API、yield APIの失敗理由を横断的に比較できる。

#### Acceptance Criteria
1. When `cre_tsk()` succeeds, the system shall return `E_OK`.
2. When `cre_tsk()` receives invalid creation parameters, the system shall return `E_PAR`.
3. When `sta_tsk()` receives an invalid task ID, the system shall return `E_ID`.
4. When `sta_tsk()` targets a task that is not DORMANT, the system shall return `E_OBJ`.
5. When `slp_tsk()` succeeds, the system shall return `E_OK`.
6. When `wup_tsk()` receives an invalid task ID, the system shall return `E_ID`.
7. When `wup_tsk()` targets a task that is not sleep waiting, the system shall return `E_OBJ`.
8. When `wai_sem()` succeeds immediately or reaches the existing waiting transition path, the system shall return `E_OK`.
9. When `wai_sem()` receives an invalid semaphore ID, the system shall return `E_ID`.
10. When `sig_sem()` succeeds, the system shall return `E_OK`.
11. When `pol_sem()` cannot acquire immediately, the system shall return `E_TMOUT` without moving the task to WAITING.
12. When `twai_sem()` reaches timeout through the existing delay queue timeout path, the system shall make the timeout result visible as `E_TMOUT` in logs.
13. When `dly_tsk()` and `yield_tsk()` complete or fail, the system shall use the same common error code meanings.

### Requirement 3: ログ上の可観測性
**Objective:** 学習用RTOSの利用者として、QEMU serial logからAPI完了結果と失敗理由を名前で確認したい。これにより、戻り値の整数値を覚えなくても状態遷移とエラー理由を追跡できる。

#### Acceptance Criteria
1. When each target API completes, the system shall log `completed: result=<error-name> action=<reason>` using common error code names.
2. When an invalid task ID is detected, the system shall log the rejected ID and `result=E_ID`.
3. When an invalid semaphore ID is detected, the system shall log the rejected semaphore ID and `result=E_ID`.
4. When invalid parameters are detected, the system shall log the invalid parameter reason and `result=E_PAR`.
5. When invalid object state is detected, the system shall log the observed state, expected state or reason, and `result=E_OBJ`.
6. When `pol_sem()` cannot acquire immediately, the system shall log that the operation would block and complete with `result=E_TMOUT`.
7. When `twai_sem()` timeout arrives through delay queue tick processing, the system shall log the task and semaphore identity and make the timeout completion visible with `result=E_TMOUT`.
8. The system shall keep existing state transition logs for task, semaphore, delay queue, dispatcher, and preemption paths.

### Requirement 4: 文書化と検証
**Objective:** 学習用RTOSの保守者として、14.4の到達点をspec、README、Doxygenコメント、検証ログで確認したい。これにより、後続章が共通エラーコード体系を前提にできる。

#### Acceptance Criteria
1. When reviewing `requirements.md`, `design.md`, and `tasks.md`, the system shall describe the 14.4 scope and exclude unrelated μITRON API work.
2. When reviewing Doxygen comments for target APIs, the system shall explain the common return type and major error meanings.
3. When reviewing README, the system shall document that 14.4 organized the μITRON-like API return values into common error codes and may include tag `v14.4-itron-error-code-system`.
4. When `make` runs, the system shall build successfully.
5. When `make run` runs, the system shall boot under QEMU and update `docs/logs/qemu-serial.log`.
6. When `make run VALIDATE_TIMER_IRQ_ENTRY=1` runs, the system shall preserve timer IRQ entry validation and avoid direct API or `dispatcher_switch_to()` calls from the timer IRQ handler body.
7. When validating the spec directory after implementation, `.kiro/specs/itron-error-code-system/` shall contain only `requirements.md`, `design.md`, and `tasks.md`.
