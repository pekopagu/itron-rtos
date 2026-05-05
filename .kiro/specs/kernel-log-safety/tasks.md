# Implementation Plan

- [x] 1. `NULL` 判定の可読性を改善する
  - 対象ファイルは `kernel/kernel.c` の `kernel_log_scheduler_selection()` のみに限定する。
  - taskなし判定を `task == 0` から `task == NULL` へ変更する。
  - `NULL` マクロを明示的に使うため、必要であれば `kernel/kernel.c` に最小限のincludeを追加する。
  - 完了状態: `kernel_log_scheduler_selection()` 内のtaskなし判定が `NULL` 比較で統一され、判定後の `selected: none` 出力とreturnの流れが従来通り維持されている。
  - _Requirements: 1.1, 1.4, 2.1_

- [x] 2. `label` のNULL安全な表示にする
  - 対象ファイルは `kernel/kernel.c` の `kernel_log_scheduler_selection()` のみに限定する。
  - `hal_console_write(label)` へ直接渡す前に、表示用の安全なlabel文字列を選択する。
  - `label == NULL` の場合は `"(no-label)"` を表示し、`label != NULL` の場合は渡されたlabelを従来通り表示する。
  - 完了状態: `label` がNULLでもログ出力が継続し、正常系では既存の `[scheduler] <label>` 相当の表示が変わらない。
  - _Requirements: 1.2, 1.3, 2.1, 2.2, 2.3_

- [x] 3. `task->name` のNULL安全な表示にする
  - 対象ファイルは `kernel/kernel.c` の `kernel_log_scheduler_selection()` のみに限定する。
  - taskが非NULLの場合でも `task->name` を直接 `hal_console_write()` に渡さず、表示用の安全なname文字列を選択する。
  - `task->name == NULL` の場合は `"(null)"` を表示し、非NULLの場合は従来通りtask名を表示する。
  - 完了状態: taskが選択されたログで `name=` が常に表示され、task名がNULLでもクラッシュせず、正常系の `selected: id=... name=... prio=... state=...` 形式が維持されている。
  - _Requirements: 1.5, 1.6, 2.2, 2.4, 3.2_

- [x] 4. state文字列のNULL安全な表示にする
  - 対象ファイルは `kernel/kernel.c` の `kernel_log_scheduler_selection()` のみに限定する。
  - `kernel_task_state_to_string(task->state)` の戻り値を一度 `const char *` に受ける。
  - state文字列がNULLの場合は `"UNKNOWN"` を表示し、非NULLの場合は取得した文字列を従来通り表示する。
  - 完了状態: taskが選択されたログで `state=` が常に表示され、state文字列がNULLでもクラッシュせず、TCBやschedulerの状態は変更されていない。
  - _Requirements: 1.7, 1.8, 2.2, 2.4, 3.1, 4.4_

- [x] 5. コメントと境界制約を維持する
  - 対象ファイルは `kernel/kernel.c` の `kernel_log_scheduler_selection()` のみに限定する。
  - 既存のDoxygen形式コメントを削除せず、表示専用の補助関数であることを維持する。
  - task entry呼び出し、RUNNING遷移、コンテキストスイッチ、スタック切り替えを行わない制約がコードとコメントから崩れていないことを確認する。
  - scheduler.c、scheduler.h、task.c、task.h、Makefileを変更しないことを差分で確認する。
  - 完了状態: コメントが劣化せず、ログ安全化がkernel側に閉じており、scheduler/task APIとHAL/arch境界が変更されていない。
  - _Requirements: 3.3, 3.4, 3.5, 3.6, 3.7, 4.1, 4.2, 4.3, 4.5, 4.6, 4.7, 4.8, 4.9, 4.10, 5.1, 5.2, 5.5_

- [x] 6. buildとQEMUログで回帰確認する
  - `make` が成功することを確認する。
  - QEMU `-serial stdio` のログで `selected: none` が表示されることを確認する。
  - QEMU `-serial stdio` のログで通常の `selected: id=... name=... prio=... state=...` が表示されることを確認する。
  - 選択されたtask entryが実行されていないこと、標準 `printf` や動的メモリ確保が追加されていないことを確認する。
  - 完了状態: 正常系ログの意味が従来と同等で、scheduler選択ロジックに変更がないことを確認結果として説明できる。
  - _Requirements: 2.1, 2.2, 2.3, 2.4, 3.1, 3.2, 3.6, 3.7, 4.3, 4.4, 4.9, 4.10, 5.3, 5.4, 5.5_
