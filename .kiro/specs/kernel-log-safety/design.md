# kernel-log-safety Design

作成日時: 2026-05-05T11:44:24.4005971+09:00

## Overview

`kernel-log-safety` は、第8回 `simple-priority-scheduler` 実装後の小改善として、kernel側の scheduler 選択結果ログ補助関数をNULL安全にする。

対象は `kernel_log_scheduler_selection(const char *label, const tcb_t *task)` のみとし、schedulerの責務、API、選択ロジックは変更しない。第8回の設計方針である「schedulerはREADYタスクから次候補を選択するだけ」「タスク実行やコンテキストスイッチは行わない」を維持する。

## Goals

- taskなし判定を `task == NULL` にして可読性を改善する。
- `label == NULL` の場合でもログ出力が破綻しないようにする。
- `task->name == NULL` の場合でもログ出力が破綻しないようにする。
- state文字列取得結果が `NULL` の場合でもログ出力が破綻しないようにする。
- 正常系のログ形式を従来と同等に維持する。
- HAL console経由でログを出す現在の方針を維持する。

## Non-Goals

- `scheduler_select_next()` の仕様・実装変更。
- `scheduler_init()` の仕様・実装変更。
- `scheduler.h` / `scheduler.c` の変更。
- `task.c` / `task.h` のAPI変更。
- `Makefile` の変更。
- `task_start()` の追加。
- `context_switch()` の追加。
- task entry の呼び出し。
- RUNNING状態への変更。
- スタック切り替え。
- 割り込み、タイマ、プリエンプションの追加。
- schedulerへのログ出力責務移動。
- 標準 `printf` の使用。
- 動的メモリ確保。

## Scope

### In Scope

- `kernel/kernel.c` 内の `kernel_log_scheduler_selection()` の局所的な修正。
- 同関数のDoxygenコメント維持、必要に応じた説明の補強。
- `NULL` を明示的に使うために必要なincludeの整理。

### Out of Scope

- scheduler層、task層、HAL層、arch層のAPI変更。
- ログ形式の大幅変更。
- タスク状態管理やスケジューリング結果の変更。

## Boundary Commitments

- kernelはHAL consoleを使って表示する。
- schedulerはHAL、arch、serialへ依存しない。
- schedulerは選択ロジックのみを持ち、ログ表示はkernel側に残す。
- task層はタスク情報の保持・登録・参照を担当し、表示fallbackの責務を持たない。
- `kernel_log_scheduler_selection()` は表示専用であり、タスク関数を呼ばず、TCBを書き換えず、RUNNING遷移も行わない。

### Revalidation Triggers

以下が発生した場合、この設計を再確認する。

- `hal_console_write()` のNULL入力契約を変更する場合。
- `scheduler_select_next()` の戻り値や責務を変更する場合。
- `tcb_t` の `name`、`priority`、`state` の意味を変更する場合。
- `kernel_task_state_to_string()` の仕様を変更する場合。
- ログ形式をテストや記事側で厳密比較するようにした場合。

## Current Behavior

現在の `kernel_log_scheduler_selection()` は、scheduler選択結果を次の形で出力する。

- taskなし:
  - `[scheduler] <label> selected: none`
- taskあり:
  - `[scheduler] <label> selected: id=<id> name=<name> prio=<priority> state=<state>`

一方で、次の安全性・可読性課題がある。

- `label` をNULL確認せず `hal_console_write(label)` に渡している。
- taskなし判定が `task == 0` になっている。
- `task->name` をNULL確認せず `hal_console_write(task->name)` に渡している。
- `kernel_task_state_to_string(task->state)` の戻り値をNULL確認せず `hal_console_write()` に渡している。

## Proposed Change

`kernel_log_scheduler_selection()` の中で、HAL consoleへ渡す文字列を事前に安全な表示文字列へ正規化する。

```c
const char *safe_label = (label != NULL) ? label : "(no-label)";
```

taskなし判定は以下のように明示する。

```c
if (task == NULL) {
    hal_console_write(" selected: none\n");
    return;
}
```

taskありの場合は、task名とstate文字列にもfallbackを適用する。

```c
const char *safe_name = (task->name != NULL) ? task->name : "(null)";
const char *state_name = kernel_task_state_to_string(task->state);
const char *safe_state = (state_name != NULL) ? state_name : "UNKNOWN";
```

正常系では従来と同じ値を表示し、NULL入力時のみfallback文字列を表示する。

## Function-Level Design

### kernel_log_scheduler_selection

対象:

```c
static void kernel_log_scheduler_selection(const char *label, const tcb_t *task)
```

責務:

- scheduler選択結果をkernel側でHAL consoleへ表示する。
- `task == NULL` の場合は `selected: none` を表示して終了する。
- `task != NULL` の場合は `id`、`name`、`priority`、`state` を表示する。
- 表示に使う文字列がNULLの場合は、関数内でfallback文字列へ置き換える。

責務外:

- task entry を呼ばない。
- `task->state` を書き換えない。
- RUNNING状態へ変更しない。
- コンテキストスイッチを行わない。
- スタックを切り替えない。
- schedulerの選択結果を変更しない。

設計理由:

- NULL安全性はログ表示の問題であり、schedulerやtask APIの責務ではない。
- schedulerをHAL consoleへ依存させないため、ログ出力は引き続きkernel側に置く。
- fallback処理を関数内に閉じることで、第8回の「選択のみ・実行しない」設計を保ったまま可読性と安全性を改善できる。

## Logging Compatibility

正常系のログ形式は維持する。

- taskなし:
  - `[scheduler] <label> selected: none`
- taskあり:
  - `[scheduler] <label> selected: id=<id> name=<name> prio=<priority> state=<state>`

NULL入力時のみ表示値を置き換える。

- `label == NULL`: `<label>` の位置に `"(no-label)"` を表示する。
- `task->name == NULL`: `<name>` の位置に `"(null)"` を表示する。
- state文字列が `NULL`: `<state>` の位置に `"UNKNOWN"` を表示する。

`selected: none` と `selected: id=... name=... prio=... state=...` の主要な形は変更しない。

## Error/Safety Handling

- `label == NULL` はエラー扱いにせず、表示用fallbackとして `"(no-label)"` を使う。
- `task == NULL` はREADYタスクなし、または選択対象なしを表す既存の扱いを維持し、`selected: none` を表示する。
- `task != NULL && task->name == NULL` は表示用fallbackとして `"(null)"` を使う。
- `kernel_task_state_to_string()` が `NULL` を返した場合は表示用fallbackとして `"UNKNOWN"` を使う。
- fallback処理は表示専用であり、TCBやscheduler内部状態は変更しない。

## Non-Modified Components

以下は変更しない。

- `kernel/scheduler.c`
- `kernel/include/scheduler.h`
- `kernel/task.c`
- `kernel/include/task.h`
- `Makefile`
- `scheduler_select_next()`
- `scheduler_init()`
- `task_register()`
- `task_dump()`
- `task_get_count()`
- `task_get_by_index()`
- `task_state_t`
- `tcb_t`

## File Structure Plan

| Path | Change |
| --- | --- |
| `kernel/kernel.c` | `kernel_log_scheduler_selection()` のNULL安全化。必要に応じて `NULL` 定義用includeを追加する。 |

新規ファイルは追加しない。

## Requirements Traceability

| Requirement | Design Coverage |
| --- | --- |
| 1.1 | `task == NULL` による明示的なtaskなし判定。 |
| 1.2 | `label == NULL` のfallbackとして `"(no-label)"` を表示。 |
| 1.3 | `label != NULL` の場合は渡されたlabelをそのまま表示。 |
| 1.4 | `task == NULL` の場合は `selected: none` を表示してreturn。 |
| 1.5 | `task->name == NULL` のfallbackとして `"(null)"` を表示。 |
| 1.6 | `task->name != NULL` の場合は渡されたnameをそのまま表示。 |
| 1.7 | state文字列が `NULL` のfallbackとして `"UNKNOWN"` を表示。 |
| 1.8 | state文字列が非NULLの場合は取得した文字列をそのまま表示。 |
| 2.1 | `selected: none` の形式を維持。 |
| 2.2 | `selected: id=... name=... prio=... state=...` の形式を維持。 |
| 2.3 | schedulerの選択結果をkernel側で表示する方針を維持。 |
| 2.4 | scheduler.cにログ責務を追加しない。 |
| 3.1 | scheduler APIを変更しない。 |
| 3.2 | scheduler実装を変更しない。 |
| 3.3 | task APIを変更しない。 |
| 3.4 | task構造体を変更しない。 |
| 3.5 | Makefileを変更しない。 |
| 3.6 | HAL/arch境界を維持。 |
| 3.7 | 標準printfを使わずHAL consoleを維持。 |
| 4.1 | task_startを追加しない。 |
| 4.2 | context_switchを追加しない。 |
| 4.3 | task entryを呼ばない。 |
| 4.4 | RUNNING状態へ変更しない。 |
| 4.5 | スタック切り替えをしない。 |
| 4.6 | 割り込みを追加しない。 |
| 4.7 | タイマを追加しない。 |
| 4.8 | プリエンプションを追加しない。 |
| 4.9 | 動的メモリを使わない。 |
| 4.10 | schedulerへログ責務を移さない。 |
| 5.1 | Doxygenコメントを維持する。 |
| 5.2 | 表示専用であることをコメント方針に含める。 |
| 5.3 | `make` によるbuild確認をVerification Planに含める。 |
| 5.4 | QEMU `-serial stdio` による正常系ログ確認を含める。 |
| 5.5 | `scheduler_select_next()` の挙動非変更を検証項目に含める。 |

## Verification Plan

### Static Review

- 差分が `kernel/kernel.c` に限定されていることを確認する。
- `kernel/scheduler.c`、`kernel/include/scheduler.h`、`kernel/task.c`、`kernel/include/task.h`、`Makefile` に差分がないことを確認する。
- `task_start`、`context_switch`、task entry呼び出し、RUNNING遷移、割り込み、タイマ、プリエンプション、動的メモリ、標準 `printf` が追加されていないことを確認する。

### Build Verification

- `make` が成功することを確認する。

### Runtime Log Verification

QEMU `-serial stdio` で以下を確認する。

- scheduler選択前などREADYなしのケースで `selected: none` が表示される。
- scheduler選択後のケースで `selected: id=... name=... prio=... state=...` が表示される。
- 正常系ログが従来と同等である。
- 選択されたtask entryが実行されていない。

### Regression Check

- `scheduler_select_next()` がREADYタスク選択だけを行う仕様のままである。
- scheduler.cがHAL/arch/serialへ依存していない。
- TCBやtask_tableの内容がログ関数によって変更されていない。
