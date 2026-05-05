# Implementation Plan

- [x] 1. 既存の起動時検証フローと差し込み位置を確認する
- [x] 1.1 `kernel.c` の selected/current/RUNNING 確認フローを確認する
  - `scheduler_select_next()` の戻り値が selected task として扱われていることを確認する。
  - `dispatcher_commit_current(selected)` が current commit と READY から RUNNING への論理遷移だけを担っていることを確認する。
  - `dispatcher_get_current()` で current task を取得できる既存経路を確認する。
  - 完了時には、entry呼び出しを追加する前の selected → committed current → task_dump の流れを説明できる。
  - _Requirements: 1.5, 6.1, 7.1, 8.1_

- [x] 1.2 current commit後のentry実行位置を確定する
  - commit成功後、既存停止ループへ入る前を entry 実行の差し込み位置として整理する。
  - commit失敗時や current 未設定時には entry を呼ばない経路を確認する。
  - 完了時には、`kernel_main()` 上で entry call が selected/currentログの後に来る位置が明確になっている。
  - _Requirements: 1.1, 1.2, 1.5, 7.5, 11.5_

- [x] 2. `kernel.c` にentry実行用static helperを実装する
- [x] 2.1 entry関連ログhelperを追加する
  - entry呼び出し前ログを出す `kernel_log_entry_call()` 相当のstatic helperを追加する。
  - entry returnログを出す `kernel_log_entry_return()` 相当のstatic helperを追加する。
  - precondition不成立時のskipログを出す `kernel_log_entry_skip()` 相当のstatic helperを追加する。
  - current taskのid、name、priority、stateをログで識別できるようにする。
  - 完了時には、entry call、entry return、skipがそれぞれ別のログ行として読める。
  - _Requirements: 4.1, 4.2, 4.4, 4.5, 4.6, 5.1_
  - _Boundary: Entry Logging_

- [x] 2.2 current entry実行helperを追加する
  - `kernel_run_current_entry_once()` 相当のstatic helperを `kernel.c` 内に追加する。
  - helper内で `dispatcher_get_current()` を使い、current taskを読み取り専用のentry実行候補として扱う。
  - 新規public APIを追加せず、helperを `kernel.c` 内に閉じる。
  - 完了時には、entry実行処理が `kernel.c` のstatic helperとして分離され、外部ヘッダ変更なしで参照できる。
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 7.5, 11.1_
  - _Boundary: Kernel Entry Runner_

- [x] 2.3 entry呼び出し前提条件を実装する
  - current task が `NULL` の場合は entry を呼ばずskipログを出す。
  - current task の state が `TASK_STATE_RUNNING` ではない場合は entry を呼ばずskipログを出す。
  - current task の entry が `NULL` の場合は entry を呼ばずskipログを出す。
  - 完了時には、3つのpreconditionがすべて満たされた場合だけ entry 呼び出しへ進む。
  - _Requirements: 2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 2.7, 3.2_
  - _Boundary: Kernel Entry Runner_
  - _Depends: 2.1, 2.2_

- [x] 2.4 current entryを通常のC関数として1回だけ呼び出す
  - precondition成立後に `current->entry()` を通常のC関数呼び出しとして実行する。
  - entry呼び出し前ログを出してから entry を呼ぶ。
  - 1回の起動時検証フローで current entry を1回だけ呼ぶ。
  - 完了時には、entry内部ログがentry callログの後に1回だけ出る。
  - _Requirements: 1.1, 1.2, 4.1, 4.3, 4.5, 9.1, 9.2, 9.3, 9.4, 9.5_
  - _Boundary: Kernel Entry Runner_
  - _Depends: 2.3_

- [x] 3. entry return後の暫定停止フローを接続する
- [x] 3.1 entry returnログと停止合流点を実装する
  - entryがreturnした後にentry returnログを出す。
  - return後は正式なtask終了状態を作らず、既存HLTまたは同等の停止ループへ進める。
  - `TASK_STATE_RUNNING` から `TASK_STATE_DORMANT`、WAITING、終了状態へ遷移させない。
  - 完了時には、entry return後のログが出た後にkernelが停止状態へ進む。
  - _Requirements: 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 9.10, 11.4_
  - _Boundary: Entry Return Handling_
  - _Depends: 2.4_

- [x] 3.2 precondition skip後の停止フローを実装する
  - skipログ出力後にentryを呼ばず、既存HLTまたは同等の停止ループへ進める。
  - skip経路でschedulerを再実行しない。
  - skip経路でcurrent taskやTCB stateを変更しない。
  - 完了時には、precondition不成立時にentry内部ログが出ず、skipログ後に停止状態へ進む。
  - _Requirements: 2.4, 2.5, 2.6, 2.7, 5.2, 5.6_
  - _Boundary: Entry Return Handling_
  - _Depends: 2.3_

- [x] 3.3 `kernel_main()` のcommit後フローへentry実行を接続する
  - commit成功後に `kernel_run_current_entry_once()` 相当を呼ぶ。
  - commit失敗時にはentry実行へ進まない。
  - selectedログ、committed currentログ、entry callログの順序を保つ。
  - 完了時には、起動時検証フローが selected → current/RUNNING → entry call → entry body → entry return → halt の順に読める。
  - _Requirements: 1.5, 4.1, 4.3, 4.4, 4.5, 7.5, 11.5_
  - _Boundary: Kernel Boot Flow_
  - _Depends: 3.1, 3.2_

- [x] 3.4 entry実行が起動時1回のみであることを保証する
  - entry実行が `kernel_main()` の通常フロー内で1回だけ呼ばれることを確認する。
  - entry実行がループ内や再帰的な経路で呼ばれないことを確認する。
  - entry実行後に再スケジュールや再実行トリガが存在しないことを確認する。
  - entry return後は既存HLTまたは停止ループへ進み、再度entryが呼ばれないことを確認する。
  - 完了時には、entry実行がboot-time verificationとして1回のみ発生することを説明できる。
  - _Requirements: 1.1, 1.5, 5.2, 5.6, 11.5_
  - _Boundary: Kernel Boot Flow, Entry Return Handling_
  - _Depends: 3.3_

- [x] 4. Doxygenコメントと設計意図コメントを追加する
- [x] 4.1 entry直接呼び出しのDoxygenコメントを更新する
  - 4.1の直接entry呼び出しが一時的なboot-time verification modelであることを書く。
  - entry呼び出し前提条件として current、RUNNING、non-NULL entry を書く。
  - `current->entry()` が通常のC関数呼び出しであることを書く。
  - 完了時には、`kernel.c` のfile-levelまたはhelper-levelコメントから4.1の実行モデルを確認できる。
  - _Requirements: 10.1, 10.2, 10.3, 10.4, 10.7_
  - _Boundary: Documentation Policy_

- [x] 4.2 RUNNINGと第5章接続のコメントを補足する
  - RUNNINGがCPU継続実行、独立stack実行、CPU context復元を意味しないことを書く。
  - 第5章で直接entry呼び出しをcontext-switch-based executionへ置き換える前提を書く。
  - 必要な場合だけ `task.h` の `TASK_STATE_RUNNING` または `tcb_t.entry` コメントを補足する。
  - 完了時には、RUNNINGの4.1上の意味と第5章への置換点がコメントから追跡できる。
  - _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 8.5, 10.5, 11.1, 11.2, 11.3, 11.4_
  - _Boundary: Documentation Policy_

- [x] 4.3 既存責務分離の設計意図コメントを追加する
  - dispatcherでentryを呼ばない理由を `kernel.c` のentry実行箇所付近に記載する。
  - schedulerのREADY選択責務を変更しない理由を記載する。
  - task_runner専用層を4.1で導入しない理由を記載する。
  - kernel.cで直接呼ぶ理由をboot-time verification modelとして記載する。
  - 完了時には、将来の実装者がentry呼び出しをdispatcher/scheduler/task.cへ移すべきでない理由をコメントから確認できる。
  - _Requirements: 6.1, 6.2, 6.3, 6.4, 6.5, 7.1, 7.2, 8.1, 8.2, 10.6, 11.4_
  - _Boundary: Documentation Policy_

- [x] 5. 既存モジュールとビルド境界を確認する
- [x] 5.1 scheduler/dispatcher/task管理にentry実行責務が入っていないことを確認する
  - `scheduler.c` にentry呼び出し、current commit、HALログ責務が追加されていないことを確認する。
  - `dispatcher.c` にentry呼び出し、context switch、stack switchが追加されていないことを確認する。
  - `task.c` にentry呼び出し、stack switch、CPU context作成が追加されていないことを確認する。
  - 完了時には、entry実行の副作用が `kernel.c` のboot-time verification helper内に限定されている。
  - _Requirements: 6.1, 6.2, 6.3, 6.4, 6.5, 7.1, 7.2, 7.3, 7.4, 8.1, 8.2, 8.3, 8.4_
  - _Boundary: Boundary Guard_

- [x] 5.2 Makefileとpublic API境界を確認する
  - Makefileに新規objectが追加されていないことを確認する。
  - `task_runner.c` / `task_runner.h` が追加されていないことを確認する。
  - entry実行用の新規public APIや新規公開ヘッダが追加されていないことを確認する。
  - 完了時には、既存build構成のまま `kernel.c` 変更だけでentry実行モデルが組み込まれている。
  - _Requirements: 1.3, 1.4, 9.11_
  - _Boundary: Boundary Guard_

- [x] 5.3 非要求違反がないことをレビューする
  - コンテキストスイッチ、アセンブラ、レジスタ保存・復元、スタック切り替えが追加されていないことを確認する。
  - 割り込み、タイマ、プリエンプション、複数タスク交互実行が追加されていないことを確認する。
  - 正式なtask終了状態やμITRON互換APIが追加されていないことを確認する。
  - 既存RTOS実装の参照・コピー・流用を示すコメントや構造が入っていないことを確認する。
  - 完了時には、4.1の最小entry実行モデルだけが差分として残っている。
  - _Requirements: 9.1, 9.2, 9.3, 9.4, 9.5, 9.6, 9.7, 9.8, 9.9, 9.10, 9.11, 9.12_
  - _Boundary: Boundary Guard, Documentation Policy_

- [x] 6. ビルドとQEMUログで通常経路を確認する
- [x] 6.1 `make` でビルド成功を確認する
  - `make` を実行し、kernel imageが生成されることを確認する。
  - warningやerrorが出る場合は、entry helper追加範囲に限定して修正する。
  - 完了時には、既存Makefileのままビルドが成功する。
  - _Requirements: 1.3, 1.4, 4.6, 9.1, 9.2_
  - _Boundary: Kernel Boot Flow_

- [x] 6.2 QEMUでentry通常経路ログを確認する
  - QEMU `-serial stdio` または既存 `make run` 相当で起動ログを確認する。
  - selectedログと committed current/RUNNINGログがentry callより前に出ることを確認する。
  - entry callログ、task entry内部ログ、entry returnログが順番に出ることを確認する。
  - return後にkernelが停止状態へ進むことを確認する。
  - 完了時には、selected → current/RUNNING → entry call → entry内部ログ → entry return → 停止状態の順序をログから説明できる。
  - _Requirements: 1.1, 1.2, 1.5, 4.1, 4.2, 4.3, 4.4, 4.5, 4.6, 5.1, 5.2, 11.5_
  - _Boundary: Kernel Boot Flow, Entry Logging_
  - _Depends: 6.1_

- [x] 7. skip経路とログ更新を確認する
- [x] 7.1 precondition不成立時のskip経路を確認する
  - current未設定、non-RUNNING、entry NULLのいずれかの検証可能な経路でskipログを確認する。
  - skip経路ではentry内部ログが出ないことを確認する。
  - skip経路では状態遷移や再scheduleが発生しないことを確認する。
  - 完了時には、precondition不成立時にentryを呼ばないことをログで説明できる。
  - _Requirements: 2.4, 2.5, 2.6, 2.7, 5.2, 5.3, 5.4, 5.5, 5.6_
  - _Boundary: Kernel Entry Runner, Entry Logging_

- [x] 7.2 READMEまたはログ記録の更新要否を確認する
  - 第4章4.1のentry call/returnログ説明が必要か確認する。
  - 必要な場合だけREADMEまたは既存ログ記録を更新し、selected、current/RUNNING、entry call、entry内部ログ、entry returnの見方を補足する。
  - 不要な場合は、QEMUログ確認だけで完了条件を満たすことを確認する。
  - 完了時には、4.1の観測方法がREADMEまたは保存ログ、もしくはQEMU検証結果から追跡できる。
  - _Requirements: 4.1, 4.2, 4.3, 4.4, 4.5, 4.6, 10.7_
  - _Boundary: Documentation Policy_

- [x] 8. 最終レビューで第5章への接続性を確認する
- [x] 8.1 第5章で置き換える範囲を確認する
  - `current->entry()` 直接呼び出しが `kernel_run_current_entry_once()` 相当の中に閉じていることを確認する。
  - current task、entry pointer、stack情報が将来のcontext setup入力として残っていることを確認する。
  - 第5章で直接呼び出しをcontext-switch-based executionへ置き換えられることを確認する。
  - 完了時には、第5章で置き換える箇所と維持する境界を説明できる。
  - _Requirements: 11.1, 11.2, 11.3, 11.4, 11.5_
  - _Boundary: Chapter 5 Connector_

- [x] 8.2 4.1全体の完了条件をレビューする
  - current entryが通常経路で1回だけ呼ばれることを確認する。
  - return後に正式なtask終了状態が作られていないことを確認する。
  - scheduler、dispatcher、task管理、Makefileの責務境界が維持されていることを確認する。
  - Doxygenコメント、設計意図コメント、QEMUログ確認がrequirementsとdesignに一致していることを確認する。
  - 完了時には、非要求違反なしで第4章4.1のentry関数扱いが実装・検証済みと判断できる。
  - _Requirements: 1.1, 1.2, 1.5, 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 5.3, 5.4, 5.5, 6.1, 7.1, 8.1, 9.1, 9.2, 9.3, 9.4, 9.5, 9.6, 9.7, 9.8, 9.9, 9.10, 9.11, 9.12, 10.1, 10.2, 10.3, 10.4, 10.5, 10.6, 10.7_
  - _Boundary: Kernel Boot Flow, Boundary Guard, Documentation Policy_
