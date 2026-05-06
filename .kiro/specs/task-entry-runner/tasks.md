# Implementation Plan

- [x] 1. 起動時検証フローの実装境界を確認する
- [x] 1.1 `kernel.c` のentry実行差し込み位置を確定する
  - 目的: current commit成功後だけentry実行へ進む位置を確定し、commit失敗時やcurrent未設定時にentryを呼ばない前提を明確にする。
  - 変更対象ファイル: `kernel/kernel.c`。
  - 実施内容: selectedログ、dispatcher commit、current取得、task dump、既存停止ループの順序を確認し、entry実行helperの呼び出し位置をcommit成功経路に限定する。
  - 完了条件: `kernel_main()` の通常経路が selected → current/RUNNING → entry実行helper → 停止制御の順に説明でき、commit失敗経路ではentry実行helperへ進まない。
  - 依存関係: なし。
  - コミットメッセージ例: `kernel: identify current entry invocation point`
  - _Requirements: 1.1, 1.2, 1.5, 7.5, 11.6_
  - _Boundary: Kernel Boot Flow_

- [x] 1.2 既存moduleの非責務境界を実装前に確認する
  - 目的: scheduler、dispatcher、task管理へentry実行責務やreturn後制御責務を混ぜない前提を固定する。
  - 変更対象ファイル: なし。確認対象は `kernel/scheduler.c`, `kernel/dispatcher.c`, `kernel/task.c`, `Makefile`。
  - 実施内容: schedulerがREADY選択のみ、dispatcherがcurrent commitのみ、task管理がTCB/状態管理のみ、Makefileが新規runner objectなしであることを確認する。
  - 完了条件: 実装作業が `kernel/kernel.c` を中心に進められ、`scheduler.c`, `dispatcher.c`, `task.c`, `Makefile` へ非要求の変更を入れない方針が確認済みである。
  - 依存関係: なし。
  - コミットメッセージ例: `docs: note entry runner implementation boundary`
  - _Requirements: 1.3, 1.4, 6.1, 6.2, 6.3, 6.4, 6.6, 7.1, 7.2, 7.3, 7.4, 8.1, 8.2, 8.3, 8.4, 9.1, 9.2, 9.3, 9.4, 9.5, 9.6, 9.7, 9.8, 9.9, 9.11, 9.15_
  - _Boundary: Boundary Guard_

- [x] 2. `kernel.c` にentry観測ログを実装する
- [x] 2.1 entry callログとcurrent識別情報を追加する
  - 目的: entry呼び出し前に、どのcurrent taskを実行対象にするかをQEMU serial logで観測可能にする。
  - 変更対象ファイル: `kernel/kernel.c`。
  - 実施内容: entry callログ用のstatic helperを追加し、currentのID、name、priority、stateを出力する。ログはentry bodyログより前に出るようにする。
  - 完了条件: QEMU serial log上で `[entry] calling current: ... state=RUNNING` 相当の行がentry bodyログより前に読める。
  - 依存関係: 1.1。
  - コミットメッセージ例: `kernel: log current task before entry call`
  - _Requirements: 4.1, 4.2, 4.5, 4.6, 5.10_
  - _Boundary: Entry Logging_

- [x] 2.2 entry returnログを追加する
  - 目的: `current->entry()` が通常のC関数呼び出しから戻った事実を、正式終了ではなく観測イベントとして記録する。
  - 変更対象ファイル: `kernel/kernel.c`。
  - 実施内容: entry returnログ用のstatic helperを追加し、return後も同じcurrentのID、name、priority、stateを出力する。
  - 完了条件: QEMU serial log上で `[entry] returned current: ... state=RUNNING` 相当の行がentry bodyログより後に読める。
  - 依存関係: 2.1。
  - コミットメッセージ例: `kernel: log entry return as observation event`
  - _Requirements: 4.4, 4.5, 4.6, 5.1, 5.2, 5.9, 5.10_
  - _Boundary: Entry Logging_

- [x] 2.3 precondition skipログを追加する
  - 目的: current未設定、non-RUNNING、entry未設定のいずれでも、entryを呼ばずに理由を観測可能にする。
  - 変更対象ファイル: `kernel/kernel.c`。
  - 実施内容: skipログ用のstatic helperを追加し、理由文字列と可能な範囲のcurrent識別情報を出力する。`current == NULL` を安全に扱う。
  - 完了条件: skip経路で `[entry] skipped: reason=...` 相当の行が出力され、entry bodyログが出ないことを説明できる。
  - 依存関係: 2.1。
  - コミットメッセージ例: `kernel: log skipped entry preconditions`
  - _Requirements: 2.4, 2.5, 2.6, 2.7, 4.6_
  - _Boundary: Entry Logging_

- [x] 3. current entry直接呼び出しhelperを実装する
- [x] 3.1 current取得とprecondition checkを実装する
  - 目的: current taskだけをentry実行候補にし、不正なTCBを実行対象にしない。
  - 変更対象ファイル: `kernel/kernel.c`。
  - 実施内容: `dispatcher_get_current()` でcurrentを取得し、`current != NULL`, `current->state == TASK_STATE_RUNNING`, `current->entry != NULL` を順に確認する。各不成立時はskipログへ進める。
  - 完了条件: 3つのpreconditionがすべて成立した場合だけentry呼び出し位置へ到達し、不成立時はentry bodyログが出ない。
  - 依存関係: 2.3。
  - コミットメッセージ例: `kernel: validate current entry preconditions`
  - _Requirements: 2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 2.7, 3.2, 7.5_
  - _Boundary: Kernel Entry Runner_

- [x] 3.2 current entryを通常のC関数として1回だけ呼び出す
  - 目的: コンテキストスイッチなしで、commit済みcurrent taskのentryを第4章4.1の最小モデルとして直接実行する。
  - 変更対象ファイル: `kernel/kernel.c`。
  - 実施内容: precondition成立後にentry callログを出し、`current->entry()` を通常のC関数として1回だけ呼ぶ。loopや再帰経路から呼ばれない構造にする。
  - 完了条件: 通常経路でentry bodyログが1回だけ出て、entry callログより後に読める。
  - 依存関係: 3.1。
  - コミットメッセージ例: `kernel: call current task entry once`
  - _Requirements: 1.1, 1.2, 1.5, 4.1, 4.3, 4.5, 9.1, 9.2, 9.3, 9.4, 9.5, 11.1, 11.2, 11.3, 11.4_
  - _Boundary: Kernel Entry Runner_

- [x] 4. entry return後とskip後の停止制御を実装する
- [x] 4.1 return検知ポイントを接続する
  - 目的: entry returnを正式終了ではなく、`current->entry()` 直後の観測可能イベントとして扱う。
  - 変更対象ファイル: `kernel/kernel.c`。
  - 実施内容: `current->entry()` の直後にreturnログを出す。returnログ後にcurrent解除やstate変更を行わない。
  - 完了条件: entry returnログがentry bodyログより後に出力され、return後もログ上のstateがRUNNINGとして確認できる。
  - 依存関係: 3.2。
  - コミットメッセージ例: `kernel: observe entry return without terminating task`
  - _Requirements: 3.3, 3.4, 4.4, 4.5, 5.1, 5.2, 5.4, 5.5, 5.6, 5.7, 5.8, 5.9, 5.10, 9.10, 9.12, 9.13, 9.14, 11.5_
  - _Boundary: Entry Return Handling_

- [x] 4.2 return後またはskip後の停止合流点を実装する
  - 目的: entry return後やprecondition skip後に、scheduler再実行や別task選択を行わず停止制御へ進める。
  - 変更対象ファイル: `kernel/kernel.c`。
  - 実施内容: return後とskip後が既存停止ループまたは同等の停止制御へ合流するようにする。entryを再呼び出さず、schedulerを呼ばない構造にする。
  - 完了条件: return後またはskip後に新しいselected/currentログや別task entryログが続かず、停止状態へ進むことを確認できる。
  - 依存関係: 4.1。
  - コミットメッセージ例: `kernel: halt after entry return or skip`
  - _Requirements: 5.3, 5.11, 5.12, 6.5, 9.9_
  - _Boundary: Entry Return Handling_

- [x] 5. 起動時検証フローへentry helperを統合する
- [x] 5.1 commit成功経路へentry helperを接続する
  - 目的: selected/current/RUNNING確認後にentry call/return観測へ進む一連のboot-time verification flowを完成させる。
  - 変更対象ファイル: `kernel/kernel.c`。
  - 実施内容: commit成功後にentry実行helperを呼び、commit失敗時はentryへ進まない。selectedログ、committed currentログ、entry callログの順序を保つ。
  - 完了条件: 通常起動ログが selected → committed current/RUNNING → entry call → entry body → entry return → stop の順に読める。
  - 依存関係: 4.2。
  - コミットメッセージ例: `kernel: wire entry runner into boot verification`
  - _Requirements: 1.5, 4.1, 4.3, 4.4, 4.5, 4.6, 11.6_
  - _Boundary: Kernel Boot Flow_

- [x] 5.2 optional内部フラグを導入しない判断を確認する
  - 目的: entry return観測を外部仕様やtask stateにせず、ログで満たせる範囲に保つ。
  - 変更対象ファイル: `kernel/kernel.c`。必要な場合のみ `kernel/kernel.c` 内のstatic diagnosticに限定する。
  - 実施内容: QEMU serial logでreturn観測が満たせる場合は内部フラグを導入しない。導入が必要な場合でもTCB、dispatcher、scheduler、public APIへ公開しない。
  - 完了条件: `TASK_STATE_EXITED` 等の新状態や外部から参照可能なreturn済みフラグが存在しない。
  - 依存関係: 5.1。
  - コミットメッセージ例: `kernel: keep entry return observation local`
  - _Requirements: 5.8, 9.10, 9.12, 9.13, 10.9_
  - _Boundary: Entry Return Handling_

- [x] 6. Doxygen形式コメントと意図コメントを更新する
- [x] 6.1 entry helper群のDoxygenコメントを更新する
  - 目的: entry直接呼び出し、precondition、return観測、停止制御の意味を実装上で誤読できないようにする。
  - 変更対象ファイル: `kernel/kernel.c`。
  - 実施内容: entry実行helper、entry callログ、entry returnログ、skipログ、停止合流点にDoxygen形式コメントを付ける。entry returnが正式終了ではないこと、RUNNING維持、current保持を明記する。
  - 完了条件: `kernel.c` のコメントから、entry returnをformal terminationとして扱わないことと、scheduler再実行しないことを確認できる。
  - 依存関係: 5.1。
  - コミットメッセージ例: `kernel: document entry return observation model`
  - _Requirements: 10.1, 10.2, 10.3, 10.4, 10.5, 10.8, 10.9_
  - _Boundary: Documentation Policy_

- [x] 6.2 RUNNINGと第5章接続のコメントを補足する
  - 目的: RUNNINGがCPU継続実行や終了状態を意味しないことと、第5章での置換点を明確にする。
  - 変更対象ファイル: `kernel/kernel.c`, 必要な場合のみ `kernel/include/task.h`。
  - 実施内容: RUNNINGはcurrent採用済み論理状態であり、entry return後も意味を変えないことを書く。直接呼び出しが第5章でcontext switchに置き換えられることを書く。
  - 完了条件: コメントから、4.1/4.2のRUNNING意味と将来のcontext switch/明示終了処理との分離を追跡できる。
  - 依存関係: 6.1。
  - コミットメッセージ例: `docs: clarify running state around entry return`
  - _Requirements: 3.1, 3.3, 3.5, 3.6, 3.7, 3.8, 8.5, 10.6, 10.7, 11.4, 11.5, 11.7_
  - _Boundary: Documentation Policy, Chapter 5 Connector_

- [x] 6.3 READMEまたはログ説明の更新要否を確認する
  - 目的: QEMU serial logでの確認方法が、必要な場合に追跡可能な形で残るようにする。
  - 変更対象ファイル: 必要な場合のみ `README.md`。
  - 実施内容: selected/current/RUNNING、entry call、entry body、entry return、停止のログ順序説明がREADMEに必要か判断する。不要な場合はQEMU検証タスクで確認を担保する。
  - 完了条件: README更新ありの場合はログの読み方が記載され、更新なしの場合もQEMU確認タスクで観測方法を説明できる。
  - 依存関係: 5.1。
  - コミットメッセージ例: `docs: describe entry return log sequence`
  - _Requirements: 4.6, 10.1, 10.9_
  - _Boundary: Documentation Policy_

- [x] 7. 既存module境界と非要求違反をレビューする
- [x] 7.1 scheduler/dispatcher/taskに不要な責務が追加されていないことを確認する
  - 目的: entry実行とreturn後制御が `kernel.c` のboot-time verification boundaryに閉じていることを確認する。
  - 変更対象ファイル: なし。確認対象は `kernel/scheduler.c`, `kernel/dispatcher.c`, `kernel/task.c`。
  - 実施内容: schedulerにentry呼び出し、再スケジュール、HALログ、current commitがないことを確認する。dispatcherにentry呼び出し、return後current解除、context/stack switchがないことを確認する。task.cにentry実行、終了状態、task lifecycleがないことを確認する。
  - 完了条件: `scheduler.c`, `dispatcher.c`, `task.c` の差分がない、またはコメント補足以外の責務追加がない。
  - 依存関係: 5.2。
  - コミットメッセージ例: `review: keep entry execution out of scheduler dispatcher task`
  - _Requirements: 6.1, 6.2, 6.3, 6.4, 6.5, 6.6, 7.1, 7.2, 7.3, 7.4, 8.1, 8.2, 8.3, 8.4_
  - _Boundary: Boundary Guard_

- [x] 7.2 状態遷移とtask lifecycleが追加されていないことを確認する
  - 目的: entry returnが正式終了に見える実装差分を排除する。
  - 変更対象ファイル: なし。確認対象は `kernel/`, `kernel/include/`。
  - 実施内容: `TASK_STATE_EXITED` 等の新状態、RUNNINGからDORMANT/READY/WAITINGへの遷移、task restart、task lifecycle、`ext_tsk` 相当APIが追加されていないことを確認する。
  - 完了条件: 状態定義と状態変更箇所に、4.2要件外の終了・再起動・ライフサイクル処理が存在しない。
  - 依存関係: 7.1。
  - コミットメッセージ例: `review: verify no task lifecycle added for entry return`
  - _Requirements: 3.4, 5.2, 5.4, 5.5, 5.6, 5.7, 5.8, 9.10, 9.12, 9.13, 9.14_
  - _Boundary: Boundary Guard_

- [x] 7.3 context switch関連の非要求違反がないことを確認する
  - 目的: 第4章4.1/4.2の直接C関数呼び出しモデルに、低レベル実行制御が混入していないことを確認する。
  - 変更対象ファイル: なし。確認対象は `kernel/`, `arch/`, `Makefile`。
  - 実施内容: context switch、stack switch、register save/restore、assembly、interrupt、timer、preemption、複数task交互実行が追加されていないことを確認する。
  - 完了条件: 追加差分が通常C関数呼び出し、ログ、停止制御、コメントに限定されている。
  - 依存関係: 7.2。
  - コミットメッセージ例: `review: verify no context switch behavior in entry runner`
  - _Requirements: 9.1, 9.2, 9.3, 9.4, 9.5, 9.6, 9.7, 9.8, 9.9_
  - _Boundary: Boundary Guard_

- [x] 7.4 既存RTOS実装の参照・流用がないことを確認する
  - 目的: 設計制約に反する外部RTOS由来の構造、コメント、API導入を排除する。
  - 変更対象ファイル: なし。確認対象は差分全体。
  - 実施内容: 実ITRON / T-Kernel / FreeRTOSなど既存RTOS実装の参照、コピー、流用を示すコメントや構造がないことを確認する。
  - 完了条件: 差分が本projectの概念設計と既存コードパターンだけに基づいていると説明できる。
  - 依存関係: 7.3。
  - コミットメッセージ例: `review: verify entry runner has no external rtos references`
  - _Requirements: 9.15_
  - _Boundary: Boundary Guard, Documentation Policy_

- [x] 8. ビルドとQEMU serial logで検証する
- [x] 8.1 `make` でビルド成功を確認する
  - 目的: 新規runner moduleやpublic APIを追加せず、既存build構成でentry runnerが組み込まれることを確認する。
  - 変更対象ファイル: なし。確認対象はbuild成果物。
  - 実施内容: `make` を実行し、kernel imageが生成されることを確認する。warning/errorが出た場合はentry helper追加範囲に限定して修正する。
  - 完了条件: 既存Makefileのままビルドが成功し、新規object依存が不要である。
  - 依存関係: 7.4。
  - コミットメッセージ例: `test: build entry runner verification flow`
  - _Requirements: 1.3, 1.4, 9.1, 9.2_
  - _Boundary: Kernel Boot Flow_

- [x] 8.2 QEMU serial logで通常経路を確認する
  - 目的: entry call、entry body、entry returnが順序通り観測でき、return後に停止制御へ進むことを確認する。
  - 変更対象ファイル: なし。確認対象はQEMU serial log。
  - 実施内容: QEMU `-serial stdio` または既存 `make run` 相当で起動し、selected/current/RUNNING、entry call、entry body、entry returnの順序を確認する。
  - 完了条件: returnログにcurrent識別情報とRUNNING状態が出て、return後に別task選択やentry再呼び出しが発生しない。
  - 依存関係: 8.1。
  - コミットメッセージ例: `test: verify entry return serial log sequence`
  - _Requirements: 1.1, 1.2, 1.5, 3.2, 3.3, 4.1, 4.2, 4.3, 4.4, 4.5, 4.6, 5.1, 5.2, 5.3, 5.9, 5.10, 5.11, 5.12, 11.6_
  - _Boundary: Kernel Boot Flow, Entry Logging, Entry Return Handling_

- [x] 8.3 precondition skip経路を確認する
  - 目的: precondition不成立時にentryを呼ばず、skipログ後に停止制御へ進むことを確認する。
  - 変更対象ファイル: 必要な場合のみ一時的な検証差分。恒久差分には残さない。
  - 実施内容: current未設定、non-RUNNING、entry NULLのいずれか検証可能な経路でskipログを確認し、entry bodyログが出ないことを確認する。
  - 完了条件: skip経路でscheduler再実行、state変更、entry再呼び出しが発生しないことをログまたはレビューで説明できる。
  - 依存関係: 8.2。
  - コミットメッセージ例: `test: verify skipped entry precondition path`
  - _Requirements: 2.4, 2.5, 2.6, 2.7, 5.3, 5.11, 5.12_
  - _Boundary: Kernel Entry Runner, Entry Logging, Entry Return Handling_

- [x] 9. 第5章への接続性と最終完了条件をレビューする
- [x] 9.1 第5章で置き換える直接呼び出し範囲を確認する
  - 目的: 第5章のcontext switch導入時に、直接C関数呼び出し部分を明確に置き換えられるようにする。
  - 変更対象ファイル: なし。確認対象は `kernel/kernel.c`, `kernel/include/task.h`。
  - 実施内容: `current->entry()` 直接呼び出しがentry helper内に閉じており、current task、entry pointer、stack_base、stack_sizeが将来のcontext setup入力として残っていることを確認する。
  - 完了条件: 第5章ではどの呼び出し箇所をcontext switchへ置き換え、どのTCB情報を維持するかを説明できる。
  - 依存関係: 8.3。
  - コミットメッセージ例: `review: confirm chapter 5 entry switch boundary`
  - _Requirements: 8.5, 10.6, 11.1, 11.2, 11.3, 11.4, 11.5, 11.7_
  - _Boundary: Chapter 5 Connector_

- [x] 9.2 4.1/4.2全体の完了レビューを行う
  - 目的: requirements/designに反する挙動がなく、実装・ログ・コメント・検証が一貫していることを最終確認する。
  - 変更対象ファイル: なし。確認対象は差分全体、build結果、QEMU serial log。
  - 実施内容: entryは1回だけ呼ばれる、returnは正式終了ではない、current/RUNNINGは保持される、scheduler再実行はない、非要求違反はない、Doxygen/通常コメントが設計と一致することを確認する。
  - 完了条件: 第4章4.1/4.2のentry直接呼び出しとentry return観測が、requirements.md/design.md通りに実装・検証済みと判断できる。
  - 依存関係: 9.1。
  - コミットメッセージ例: `review: complete task entry runner chapter 4 verification`
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 2.7, 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 3.7, 3.8, 4.1, 4.2, 4.3, 4.4, 4.5, 4.6, 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 5.7, 5.8, 5.9, 5.10, 5.11, 5.12, 6.1, 6.2, 6.3, 6.4, 6.5, 6.6, 7.1, 7.2, 7.3, 7.4, 7.5, 8.1, 8.2, 8.3, 8.4, 8.5, 9.1, 9.2, 9.3, 9.4, 9.5, 9.6, 9.7, 9.8, 9.9, 9.10, 9.11, 9.12, 9.13, 9.14, 9.15, 10.1, 10.2, 10.3, 10.4, 10.5, 10.6, 10.7, 10.8, 10.9, 11.1, 11.2, 11.3, 11.4, 11.5, 11.6, 11.7_
  - _Boundary: Kernel Boot Flow, Kernel Entry Runner, Entry Logging, Entry Return Handling, Boundary Guard, Documentation Policy, Chapter 5 Connector_
