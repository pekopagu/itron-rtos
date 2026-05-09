# Implementation Plan

- [x] 1. 既存の協調実行前提を確認する
  - `kernel/kernel.c` の既存entry runner、entry call/returnログ、`kernel_main()` からの呼び出し位置を確認する。
  - `scheduler.c`、`dispatcher.c`、`task.c` の責務境界を確認し、entry実行責務が既存moduleへ混入していないことを確認する。
  - 既存のQEMUシリアルログ形式と数値出力helperを確認し、4.3ログで再利用できる観測点を整理する。
  - 完了時には、変更対象が `kernel.c`、`task.c`、`task.h`、必要に応じたheaderコメント、README、検証だけに限定できることが説明できる。
  - _Requirements: 9.1, 9.2, 9.3, 9.4, 9.5, 9.6, 10.1, 11.8_

- [x] 2. RUNNINGからREADYへの再候補化をtask管理に追加する
  - `task_mark_ready_from_running(int task_id)` を追加し、登録済みRUNNING taskだけをREADYへ戻す。
  - 不正ID、未登録ID、RUNNING以外の状態では既存の負の `TASK_ERR_*` 系で失敗を返す。
  - `task.h` に宣言を追加し、Doxygenコメントでcooperative re-candidacyでありtask restartではないことを明記する。
  - 完了時には、READY/RUNNING以外の新状態を追加せず、`TASK_STATE_EXITED` も追加されていない。
  - _Requirements: 1.4, 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 9.3, 10.3, 11.4, 11.5, 11.6_
  - _Boundary: Task State Transition_

- [x] 3. cooperative runnerのログ補助をkernel側に追加する
  - iteration開始、cooperative return、READY再候補化成功/失敗、停止理由を出力するkernel-local helperを追加する。
  - selected taskログとcurrent commitログは既存ログhelperを再利用し、必要なら4.3用labelで区別できるようにする。
  - ログはHAL console経由に限定し、schedulerやdispatcherからHAL出力しない。
  - 完了時には、QEMUシリアルログでiteration、entry call、cooperative return、READY再候補化、stop reasonを区別できる文字列が定義されている。
  - _Requirements: 4.6, 5.4, 7.3, 7.4, 7.5, 8.1, 8.2, 8.3, 8.4, 8.5, 8.6_
  - _Boundary: Cooperative Logging_

- [x] 4. cooperative runner本体をkernel側static helperとして追加する
  - `kernel/kernel.c` に `static void kernel_run_cooperative_entries(void)` を追加し、実行回数上限をentry call countとして扱う。
  - 各iterationでREADY task選択、dispatcher commit、dispatcher current取得、current entry直接呼び出し、cooperative return観測、READY再候補化、次iterationへの進行を行う。
  - entry呼び出し対象はselected taskではなく、必ずdispatcher currentから取得したtaskにする。
  - entryは通常のC関数呼び出しとして1回呼び、returnを正式終了ではなくcooperative return eventとして扱う。
  - 完了時には、複数entry呼び出しが有限loop内で進み、上限または停止条件で既存HLT loopへ到達できる。
  - _Requirements: 1.1, 1.2, 1.3, 1.5, 2.1, 2.2, 2.3, 2.4, 2.5, 3.1, 3.2, 3.3, 4.1, 4.2, 6.1, 6.2, 6.3, 7.1, 7.2, 10.2_
  - _Boundary: Cooperative Runner_
  - _Depends: 2, 3_

- [x] 5. precondition失敗と停止条件を有限停止に統合する
  - selected taskなし、dispatcher commit失敗、current NULL、currentがRUNNING以外、entry NULLを検出し、entryを呼ばず停止理由を出す。
  - RUNNINGからREADYへの再候補化失敗時は次iterationへ進まず、失敗理由を観測可能にする。
  - 実行回数上限到達とREADY taskなしは正常停止として扱い、timer、interrupt、preemptionに依存しない。
  - 完了時には、失敗条件ごとに無限retryせず停止する経路があり、停止理由がログで区別できる。
  - _Requirements: 2.6, 3.4, 3.5, 3.6, 6.4, 7.2, 7.3, 7.4, 7.5, 7.6_
  - _Boundary: Cooperative Runner, Cooperative Logging_
  - _Depends: 4_

- [x] 6. kernel_mainを4.3のcooperative runnerへ接続する
  - 4.1/4.2の単発entry確認呼び出しを、4.3の有限cooperative runner呼び出しへ置き換える。
  - return後に正式終了処理、DORMANT遷移、`TASK_STATE_EXITED`、task restart、`yield_tsk` 相当APIを追加しない。
  - schedulerとdispatcherの実装へentry実行loopを移さない。
  - 完了時には、起動フローがtask登録、scheduler初期確認、cooperative verification、既存停止loopの順で読める。
  - _Requirements: 4.3, 4.4, 4.5, 6.5, 6.6, 9.1, 9.2, 9.4, 9.5, 9.6, 9.7, 10.4, 10.5, 10.6, 10.7, 10.8, 10.9, 10.10_
  - _Boundary: Cooperative Runner integration_
  - _Depends: 5_

- [x] 7. コメントとDoxygenを4.3の意味に更新する
  - cooperative runner helperにDoxygen形式コメントを追加し、boot-time verification modelであることを明記する。
  - task状態遷移APIのコメントに、RUNNINGはcurrent採用の論理状態であり、RUNNINGからREADYはcooperative re-candidacyであることを明記する。
  - `kernel.c`、`task.h`、必要に応じて `scheduler.h` / `dispatcher.h` の説明で、本物のcontext switchではないことを補足する。
  - context switch、stack switch、register save/restore、assembler、interrupt、timer、preemptionを行っていないことがsource commentから確認できる。
  - _Requirements: 10.1, 10.2, 10.3, 10.4, 10.5, 10.6, 10.7, 10.8, 10.9, 10.10, 11.1, 11.2, 11.3, 11.4, 11.5, 11.6, 11.7, 11.8_
  - _Boundary: Documentation Policy_
  - _Depends: 6_

- [x] 8. READMEにChapter 4.3の技術説明を追加する
  - READMEにChapter 4.3の概要、観測できるログ順序、Git tag予定を追加する。
  - 4.3はboot-time verification modelであり、本物のcontext switch、stack switch、register save/restoreではないことを明記する。
  - entry returnはcooperative return eventであり、正式終了でも `yield_tsk` 互換APIでもないことを明記する。
  - 完了時には、READMEだけを読んでも4.3の目的、非目標、期待ログの読み方が分かる。
  - _Requirements: 10.1, 11.1, 11.2, 11.3, 11.4, 11.5, 11.7_
  - _Boundary: Documentation Policy_
  - _Depends: 7_

- [x] 9. buildとQEMUログで協調実行を検証する
  - `make` が通り、既存object構成のままbuildできることを確認する。
  - `make run` でQEMUシリアルログを確認し、iteration開始、selected task、current commit、entry call、entry body、cooperative return、READY再候補化、stop reasonが順に観測できることを確認する。
  - 複数task entryが観測できること、または現在のscheduler方針で同一taskが再選択される場合は実行回数上限で有限停止することを確認する。
  - 完了時には、QEMUログからentry call、entry body、cooperative return、next selectionまたはlimit stopの順序を説明できる。
  - _Requirements: 1.5, 3.3, 4.6, 5.3, 5.4, 6.1, 6.2, 6.4, 7.1, 7.2, 7.3, 7.4, 8.1, 8.2, 8.3, 8.4, 8.5, 8.6_
  - _Boundary: Validation_
  - _Depends: 8_

- [x] 10. 非目標と責務分離の回帰確認を行う
  - `TASK_STATE_EXITED` が追加されていないこと、DORMANT遷移が追加されていないことを確認する。
  - `task_runner.c` / `task_runner.h` が追加されていないこと、Makefileにtask_runner objectが追加されていないことを確認する。
  - schedulerとdispatcherにentry実行責務、cooperative return処理、HALログ責務が入っていないことを確認する。
  - assembler、context switch、stack switch、register save/restore、interrupt、timer、preemption、`yield_tsk` が追加されていないことを確認する。
  - 完了時には、4.3がdesign.mdのOut of Boundaryを越えていないことをレビュー結果として説明できる。
  - _Requirements: 4.2, 4.3, 4.4, 4.5, 5.2, 5.6, 6.5, 6.6, 9.1, 9.2, 9.3, 9.4, 9.5, 9.6, 9.7, 10.4, 10.5, 10.6, 10.7, 10.8, 10.9, 10.10, 11.8_
  - _Boundary: Validation_
  - _Depends: 9_
