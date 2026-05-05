# Implementation Plan

- [x] 1. task公開契約を第8回scheduler向けに更新する
- [x] 1.1 `kernel/include/task.h` のタスク状態定義とDoxygenコメントを更新する
  - 対象ファイル: `kernel/include/task.h`
  - 変更内容: `task_state_t` に `TASK_STATE_WAITING` を追加し、`UNUSED`, `DORMANT`, `READY`, `RUNNING`, `WAITING` の意味を第8回の責務に合わせて整理する。
  - 変更内容: 各 enum 値にDoxygenコメントを付け、READYが第8回の主な選択対象であることを明記する。
  - 完了条件: `TASK_STATE_WAITING` が公開 enum として参照でき、各 enum 値の意味がコメントから判断できる。
  - 依存関係: なし。
  - _Requirements: 1.1, 1.2, 7.2, 8.2_
  - _Boundary: Task Public Contract_

- [x] 1.2 `kernel/include/task.h` のTCB、task API、読み取りAPI契約を更新する
  - 対象ファイル: `kernel/include/task.h`
  - 変更内容: `tcb_t` の各フィールドコメントを第8回向けに更新し、`priority` は小さい値ほど高優先度、`state` は選択候補判定に使うことを明記する。
  - 変更内容: `task_register()` の `priority` 引数、登録直後READY、entry非実行、stack非切替、context非作成をDoxygenに明記する。
  - 変更内容: `task_get_count()` と `task_get_by_index(int index)` の宣言を追加し、`task_table` を `extern` せず読み取りアクセサだけを使う設計意図をDoxygenに書く。
  - 完了条件: 公開struct、主要フィールド、公開関数のDoxygenコメントから「選択のみで実行しない」制約と読み取り専用契約が読み取れる。
  - 依存関係: 1.1。
  - _Requirements: 1.3, 2.1, 2.2, 2.3, 2.5, 4.4, 5.2, 5.3, 7.1, 7.3, 7.5, 7.7, 7.8, 7.9_
  - _Boundary: Task Public Contract, Task Read Access_

- [x] 2. task実装をscheduler選択に必要な形へ拡張する
- [x] 2.1 `kernel/task.c` の状態文字列表現とdump出力を更新する
  - 対象ファイル: `kernel/task.c`
  - 変更内容: `TASK_STATE_WAITING` を状態文字列変換に追加する。
  - 変更内容: `task_dump()` が登録済みタスクの state と priority を従来通り表示できることを確認し、コメントを第8回向けに更新する。
  - 完了条件: QEMUログ上の `task_dump()` 出力で state と priority が確認でき、WAITING値がUNKNOWNにならない。
  - 依存関係: 1.1。
  - _Requirements: 1.5, 2.4, 5.4, 6.1, 8.6_
  - _Boundary: Task Dump_

- [x] 2.2 `kernel/task.c` にtask table読み取りAPIを実装する
  - 対象ファイル: `kernel/task.c`
  - 変更内容: `task_get_count()` は `MAX_TASKS` を返す。
  - 変更内容: `task_get_by_index()` は範囲内indexでは該当TCBへの `const tcb_t *` を返し、範囲外では `NULL` を返す。
  - 変更内容: APIがtask tableの内容を変更しないこと、`task_table` の所有は `task.c` に残すことをDoxygenコメントに明記する。
  - 完了条件: schedulerが `task_table` を `extern` せずに全スロットを走査でき、範囲外アクセスが `NULL` で扱える。
  - 依存関係: 1.2。
  - _Requirements: 1.4, 2.5, 3.1, 5.1, 5.2, 5.3, 5.5, 6.4, 7.1, 7.9_
  - _Boundary: Task Read Access_

- [x] 3. scheduler公開APIを追加する
- [x] 3.1 `kernel/include/scheduler.h` を作成しDoxygenコメントを追加する
  - 対象ファイル: `kernel/include/scheduler.h`
  - 変更内容: include guard、`task.h` include、`scheduler_init()`、`scheduler_select_next()` の宣言を追加する。
  - 変更内容: `scheduler_select_next()` は `const tcb_t *` を返し、READYなし時は `NULL` を返す契約にする。
  - 変更内容: READYのみ選択、priorityが小さいほど高優先度、同一priorityは登録順、選択のみで実行しないことをDoxygenに明記する。
  - 変更内容: schedulerはHAL/archに依存せず、選択結果ログは呼び出し側の責務であることを明記する。
  - 完了条件: 他ファイルから `scheduler.h` をincludeでき、schedulerの公開API、選択ルール、非実行制約がheaderだけで確認できる。
  - 依存関係: 1.2。
  - _Requirements: 3.1, 3.2, 3.3, 3.4, 4.1, 4.2, 4.3, 4.4, 4.8, 5.5, 6.4, 7.1, 7.4, 7.5, 7.6, 7.7, 7.8, 7.9, 8.1, 8.3, 8.4, 8.5_
  - _Boundary: Scheduler Selector, Documentation Policy_

- [x] 4. scheduler選択ロジックを実装する
- [x] 4.1 `kernel/scheduler.c` を作成し `scheduler_init()` を実装する
  - 対象ファイル: `kernel/scheduler.c`
  - 変更内容: `scheduler.c` を追加し、`scheduler_init()` を第8回では内部状態を持たない初期化APIとして実装する。
  - 変更内容: `scheduler.c` は `task.h` と `scheduler.h` のみへ依存し、HAL/arch/動的メモリには依存しない。
  - 完了条件: `scheduler_init()` がリンク可能で、scheduler層にHAL/arch依存が入っていない。
  - 依存関係: 3.1。
  - _Requirements: 5.1, 5.5, 6.4, 7.1, 7.9_
  - _Boundary: Scheduler Selector_

- [x] 4.2 `kernel/scheduler.c` にREADY走査とpriority選択を実装する
  - 対象ファイル: `kernel/scheduler.c`
  - 変更内容: `task_get_count()` と `task_get_by_index()` だけを使って task table 全体を走査する。
  - 変更内容: `NULL` スロット、`TASK_STATE_READY` 以外のタスクを候補から除外する。
  - 変更内容: READY候補の中から最小 `priority` 値のTCBを選択する。
  - 完了条件: READY状態のタスクだけが選択候補になり、異なるpriorityでは最小priorityのタスクが返る。
  - 依存関係: 2.2, 4.1。
  - _Requirements: 1.2, 1.4, 2.3, 3.1, 3.2, 5.2, 6.4, 7.4, 7.5_
  - _Boundary: Scheduler Selector_

- [x] 4.3 `kernel/scheduler.c` に同一priorityとREADYなしの扱いを完成させる
  - 対象ファイル: `kernel/scheduler.c`
  - 変更内容: 同一priorityでは既存のbestを上書きせず、task table昇順で先に見つかったタスクを返す。
  - 変更内容: READYタスクが存在しない場合は `NULL` を返す。
  - 変更内容: `scheduler_select_next()` がentry呼び出し、状態変更、stack切替、context switch、ログ出力を行わないことをコメントとコード構造で明確にする。
  - 完了条件: 同一priorityでは先に登録されたタスクが返り、READYなし時に `NULL` が返り、scheduler実装内にentry呼び出し、state代入、HAL/arch呼び出しが存在しない。
  - 依存関係: 4.2。
  - _Requirements: 3.3, 3.4, 4.1, 4.2, 4.3, 4.8, 6.4, 7.6, 7.7, 7.9, 8.1, 8.2, 8.3, 8.4, 8.5_
  - _Boundary: Scheduler Selector_

- [x] 5. build設定へschedulerを統合する
  - 対象ファイル: `Makefile`
  - 変更内容: `kernel/scheduler.c` 用の object、compile rule、header依存、link対象を追加する。
  - 完了条件: `make` が `scheduler.c` を含めて kernel image を生成でき、scheduler未リンクによるエラーが出ない。
  - 依存関係: 3.1, 4.1。
  - _Requirements: 5.5, 6.3_
  - _Boundary: Build Configuration_

- [x] 6. kernel起動時確認へscheduler選択ログを統合する
- [x] 6.1 `kernel/kernel.c` にscheduler API呼び出しを追加する
  - 対象ファイル: `kernel/kernel.c`
  - 変更内容: `scheduler.h` をincludeし、タスク登録と `task_dump()` の後に `scheduler_init()` と `scheduler_select_next()` を呼ぶ。
  - 変更内容: 選択されたTCBのentry関数は呼ばず、戻り値の参照だけに留める。
  - 完了条件: boot-time flowでscheduler選択が一度実行され、タスク実行ログが増えない。
  - 依存関係: 5。
  - _Requirements: 3.5, 4.1, 4.8, 6.2, 6.3, 8.6_
  - _Boundary: Kernel Verification Hook_

- [x] 6.2 `kernel/kernel.c` に選択結果ログを追加する
  - 対象ファイル: `kernel/kernel.c`
  - 変更内容: `scheduler_select_next()` が非NULLを返した場合に、選択taskの id、name、priority、state をHAL console経由で表示する。
  - 変更内容: `scheduler_select_next()` が `NULL` の場合に、READYタスクなしを示すログをHAL console経由で表示する。
  - 変更内容: scheduler側ではなくkernel側でログ出力する責務分離を保つ。
  - 完了条件: QEMUシリアルログで選択成功とno-selectionのどちらの分岐も識別可能なログ文字列になっている。
  - 依存関係: 6.1。
  - _Requirements: 3.4, 3.5, 3.6, 6.2, 6.3, 6.5, 8.6_
  - _Boundary: Kernel Verification Hook_

- [x] 7. 起動時サンプルで選択規則を確認できるようにする
- [x] 7.1 `kernel/kernel.c` のサンプルタスク登録を異なるpriorityと同一priority確認向けに調整する
  - 対象ファイル: `kernel/kernel.c`
  - 変更内容: 複数タスクを登録し、最小priorityのREADYタスクが選ばれることがログから分かる登録順とpriorityにする。
  - 変更内容: 同一priorityのREADYタスクを複数登録し、先に登録されたタスクが選ばれる確認ケースを追加する。
  - 変更内容: 既存のtask entry関数は登録だけに使い、呼び出さない。
  - 完了条件: QEMUログで異なるpriority時と同一priority時の登録順、priority、選択結果が対応付けられる。
  - 依存関係: 6.2。
  - _Requirements: 1.3, 2.2, 2.4, 3.2, 3.3, 4.1, 6.1, 6.2, 7.6, 8.1, 8.5, 8.6_
  - _Boundary: Kernel Verification Hook_

- [x] 7.2 READYなしの確認ケースを追加する
  - 対象ファイル: `kernel/kernel.c`, `kernel/task.c` または既存APIの範囲内で確認可能な箇所
  - 変更内容: READYタスクが存在しない場合に `scheduler_select_next()` が `NULL` を返すことを確認するboot-timeまたはreview-level経路を追加する。
  - 変更内容: 既存APIだけで自然に確認できない場合は、実装コードを広げずreview-levelチェックとして明記する。
  - 完了条件: READYなし時の扱いが実行ログまたは明示されたreview-level確認で検証可能になっている。
  - 依存関係: 6.2。
  - _Requirements: 3.4, 3.6, 8.6_
  - _Boundary: Kernel Verification Hook, Scheduler Selector_

- [x] 8. コメント・境界ポリシーの最終確認を行う
- [x] 8.1 公開API、enum、structのDoxygen不足と第8回範囲外事項を確認する
  - 対象ファイル: `kernel/include/task.h`, `kernel/include/scheduler.h`, `kernel/task.c`, `kernel/scheduler.c`, `kernel/kernel.c`
  - 変更内容: 追加・変更した公開関数、公開enum、公開struct、主要フィールドにDoxygenコメントがあることを確認し、不足分を補う。
  - 変更内容: `scheduler_select_next()` のアルゴリズム説明に、READYのみ、最小priority、同一priority先勝ち、NULL返却を含める。
  - 変更内容: `task_start()`、`context_switch()`、割り込み、タイマ、プリエンプション、ラウンドロビン、動的メモリ、実タスク関数呼び出しが追加されていないことを確認する。
  - 変更内容: 既存RTOS実装の模倣ではなく、本プロジェクトの学習用設計としてコメントとコードが整合していることを確認する。
  - 完了条件: コメントなしの公開API・構造体・enumが残らず、範囲外APIや範囲外挙動がコードに存在せず、schedulerが状態変更しないことを確認できる。
  - 依存関係: 7.2。
  - _Requirements: 4.4, 4.5, 4.6, 4.7, 5.1, 6.4, 7.1, 7.2, 7.3, 7.4, 7.5, 7.6, 7.7, 7.8, 7.9, 8.1, 8.2, 8.3, 8.4_
  - _Boundary: Documentation Policy, Scheduler Selector_

- [x] 9. buildとQEMUシリアルログで最終検証する
- [x] 9.1 build検証を実行する
  - 対象ファイル: `Makefile`, `kernel/include/task.h`, `kernel/include/scheduler.h`, `kernel/task.c`, `kernel/scheduler.c`, `kernel/kernel.c`
  - 変更内容: `make` を実行し、scheduler追加後もfreestanding kernel imageが生成されることを確認する。
  - 完了条件: buildが成功し、include不足・未定義参照・HAL/arch境界違反由来のcompile errorがない。
  - 依存関係: 8.1。
  - _Requirements: 5.5, 6.3_
  - _Boundary: Build Configuration_

- [x] 9.2 QEMUシリアルログで選択結果を確認する
  - 対象ファイル: `docs/logs/qemu-serial.log` または `make run` の標準出力
  - 変更内容: QEMU `-serial stdio` 相当の実行で、task dumpとscheduler選択ログを確認する。
  - 変更内容: 異なるpriority、同一priority、READYなしの確認観点をログまたはreview-levelチェックで照合する。
  - 完了条件: 選択結果ログが確認でき、task entry実行ログが出ていない。
  - 依存関係: 9.1。
  - _Requirements: 1.5, 2.4, 3.2, 3.3, 3.4, 3.5, 3.6, 4.1, 4.8, 6.1, 6.2, 6.3, 6.5, 8.6_
  - _Boundary: Kernel Verification Hook_
