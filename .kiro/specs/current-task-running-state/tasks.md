# Implementation Plan

- [x] 1. dispatcher公開ヘッダを追加する
  - `kernel/include/dispatcher.h` を追加し、dispatcher境界の公開契約を定義する。
  - `dispatcher_init()`, `dispatcher_commit_current(const tcb_t *selected)`, `dispatcher_get_current()` を宣言する。
  - `DISPATCHER_OK`, `DISPATCHER_ERR_INVAL`, `DISPATCHER_ERR_BAD_STATE`, `DISPATCHER_ERR_NOT_FOUND` を既存の `int` 戻り値方針に合わせて定義する。
  - 公開APIのDoxygenコメントに、dispatcherの責務、今回やること、今回やらないこと、第4章への接続前提が記載されている。
  - 完了時には、dispatcher公開ヘッダだけで current commit API の呼び出し契約とエラー契約を確認できる。
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 2.2, 2.4, 2.5, 3.6, 3.7, 7.1, 7.2, 7.3, 7.4, 7.5, 9.1_
  - _Boundary: Dispatcher Public Contract_

- [x] 2. task状態変更APIを公開契約に追加する
  - `kernel/include/task.h` に `task_mark_running(int task_id)` 相当の状態変更APIを宣言する。
  - 必要な場合は `TASK_ERR_NOT_FOUND` と `TASK_ERR_BAD_STATE` を既存 `TASK_ERR_*` と重複しない負値として追加する。
  - Doxygenコメントに、READY状態の有効TCBだけをRUNNINGへ変更すること、task_tableを直接公開しないこと、entry実行やstack操作を行わないことを記載する。
  - 完了時には、dispatcherがtask_tableを直接触らずにREADYからRUNNINGへの状態変更を要求できる公開契約がある。
  - _Requirements: 3.1, 3.3, 3.5, 5.1, 5.2, 5.5, 7.1, 7.2, 7.3, 7.4, 7.5, 8.1, 8.7_
  - _Boundary: Task State Mutation_

- [x] 3. task_mark_running() の状態変更処理を実装する
  - `kernel/task.c` で task_id から task_table 上の有効TCBを探索する。
  - `task_id <= 0`、該当TCBなし、UNUSEDスロット、READY以外の状態を失敗として扱う。
  - READY状態の対象だけを `TASK_STATE_RUNNING` へ変更する。
  - task_table は `static` のまま維持し、外部から直接書き込めない状態を保つ。
  - entry関数呼び出し、stack操作、context switch、register保存・復元を行わない。
  - 完了時には、task_dumpまたは同等の状態出力でRUNNING状態を観測できる。
  - _Requirements: 3.1, 3.3, 3.5, 5.1, 5.2, 5.3, 5.5, 8.1, 8.4, 8.6, 8.7, 8.9_
  - _Boundary: Task State Mutation_

- [x] 4. dispatcherのcurrent commit処理を実装する
  - `kernel/dispatcher.c` を追加し、current taskを未設定または設定済みとして保持する。
  - `dispatcher_init()` で current task を未設定状態へ初期化する。
  - `dispatcher_commit_current()` で `selected == NULL` と `selected->state != TASK_STATE_READY` を失敗として扱う。
  - `selected->id` を使って `task_mark_running()` を呼び、成功時だけ current task を設定する。
  - `dispatcher_get_current()` で current task を読み取り専用として返し、未設定時は `NULL` を返す。
  - dispatcherはHAL console、scheduler、arch固有ヘッダに依存しない。
  - 完了時には、selected task はcommit前にはcurrentではなく、commit成功後だけcurrent/RUNNINGとして扱われる。
  - _Depends: 2, 3_
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 2.2, 2.3, 2.4, 3.1, 3.2, 3.3, 3.4, 3.5, 8.1, 8.4, 8.5, 8.6, 8.7, 8.8, 8.9, 9.1, 9.2, 9.3, 9.5_
  - _Boundary: Dispatcher Commit_

- [x] 5. 起動時検証フローをkernel側に統合する
  - `kernel.c` の初期化順序に `dispatcher_init()` を追加する。
  - `scheduler_select_next()` の戻り値を selected task としてログ出力する。
  - `dispatcher_commit_current()` を呼び、成功時は `dispatcher_get_current()` で current/RUNNING committed をログ出力する。
  - commit失敗時は、selectedログとは別に commit failed とエラー値をログ出力する。
  - commit後に `task_dump()` を呼び、RUNNING状態をQEMUログで確認できるようにする。
  - ログ出力はHAL console経由で行い、dispatcher.c と scheduler.c にはログ責務を持たせない。
  - 完了時には、QEMUログ上で selected と current/RUNNING committed が別の意味として読める。
  - _Depends: 1, 4_
  - _Requirements: 1.3, 2.1, 2.5, 4.1, 4.2, 4.3, 4.4, 4.5, 4.6, 5.3, 5.4, 6.1, 6.2, 6.3, 6.4, 6.5, 9.4_
  - _Boundary: Kernel Verification Hook_

- [x] 6. dispatcherをビルド対象に追加する
  - Makefileに dispatcher object を追加し、kernel image にリンクされるようにする。
  - `kernel/dispatcher.c` と `kernel/include/dispatcher.h` の依存関係をビルドルールに含める。
  - `kernel.c` のビルドルールに dispatcher header への依存を追加する。
  - 完了時には、dispatcher module を含むkernel imageが通常の `make` 対象として生成される。
  - _Depends: 1, 4, 5_
  - _Requirements: 8.9, 9.4_
  - _Boundary: Build Integration_

- [x] 7. Doxygenコメントと非要求の実装境界を確認する
  - `dispatcher_commit_current()` 相当のコメントに、selected taskをcurrent taskとしてcommitする責務が明記されていることを確認する。
  - READY → RUNNING の論理状態遷移のみを行うことが明記されていることを確認する。
  - entry関数を呼び出さないこと、context switchを行わないこと、stack switchを行わないこと、register save/restoreを行わないことを確認する。
  - 第4章でentry実行モデルへ接続する前提がコメントに含まれていることを確認する。
  - 既存RTOS実装コードや構造への参照がコメントに含まれていないことを確認する。
  - 完了時には、公開ヘッダのコメントが設計上の責務境界と矛盾していない。
  - _Depends: 1, 2, 4_
  - _Requirements: 7.1, 7.2, 7.3, 7.4, 7.5, 7.6, 7.7, 7.8, 7.9, 7.10, 7.11, 7.12, 8.1, 8.4, 8.6, 8.7_
  - _Boundary: Documentation Policy_

- [x] 8. ビルドと境界維持を確認する
  - `make` が成功することを確認する。
  - 未使用関数、include不足、宣言と定義の不一致がないことを確認する。
  - `kernel/scheduler.c` に不要な変更が入っていないことを確認する。
  - schedulerがTCB状態変更、current保持、HAL console出力を行っていないことを確認する。
  - task_table が外部公開されていないことを確認する。
  - 完了時には、ビルド成功とscheduler副作用なし境界を確認できる。
  - _Depends: 6, 7_
  - _Requirements: 4.1, 4.2, 4.3, 4.4, 4.5, 4.6, 5.1, 5.4, 5.5, 8.5, 8.9_
  - _Boundary: Build Integration, Scheduler Selector_

- [x] 9. QEMUログでcurrent/RUNNING状態を確認する
  - QEMU serial log で登録後の selected task が表示されることを確認する。
  - current/RUNNING committed ログが selected ログと区別できることを確認する。
  - commit失敗ケースを通す場合は commit failed ログがcurrent/RUNNING committedと区別できることを確認する。
  - commit後の `task_dump()` で `state=RUNNING` が表示されることを確認する。
  - task entry実行ログが出ていないことを確認する。
  - 完了時には、QEMUログだけで selected → commit → RUNNING観測の流れを追跡できる。
  - _Depends: 8_
  - _Requirements: 1.3, 2.5, 3.6, 3.7, 5.3, 6.1, 6.2, 6.3, 6.4, 6.5, 8.1, 9.2, 9.3, 9.4_
  - _Boundary: Kernel Verification Hook_

- [x] 10. 非要求の最終レビューを行う
  - task entry関数を呼んでいないことを確認する。
  - `task_start()` と `sta_tsk()` などのμITRON風外部APIを追加していないことを確認する。
  - context switch、アセンブラ、register保存・復元、stack切り替えを追加していないことを確認する。
  - 割り込み、タイマ、プリエンプション、動的メモリを追加していないことを確認する。
  - 既存RTOS実装コードを参照、コピー、構造流用していないことを確認する。
  - 完了時には、第3章3.3の完了条件が current採用とRUNNING論理状態の観測に限定されている。
  - _Depends: 9_
  - _Requirements: 3.7, 7.12, 8.1, 8.2, 8.3, 8.4, 8.5, 8.6, 8.7, 8.8, 8.9, 9.4, 9.5_
  - _Boundary: Boundary Policy_
