# Implementation Plan

- [x] 1. 公開タスク管理契約を追加する
- [x] 1.1 `kernel/include/task.h` に公開型・定数・APIを定義する
  - `task_entry_t`、`task_state_t`、`tcb_t` を design.md の契約どおりに定義する。
  - `MAX_TASKS` を `256` として定義する。
  - `TASK_ERR_FULL`、`TASK_ERR_INVAL`、`TASK_ERR_ID_OVERFLOW` を負のエラーコードとして定義する。
  - `task_init`、`task_register`、`task_dump` のプロトタイプを公開する。
  - 完了状態: `kernel/include/task.h` だけを見れば、TCB、状態、エラー、API の公開契約が確認できる。
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 2.1, 5.1, 5.2, 5.3_
  - _Boundary: Task Public Contract_

- [x] 1.2 公開ヘッダを freestanding kernel で安全に include できる状態にする
  - include guard を追加し、既存の `kernel/include` include root から利用できる配置にする。
  - `task.h` が arch 固有ヘッダや動的メモリ前提のヘッダに依存しないことを確認する。
  - 完了状態: `kernel/kernel.c` と `kernel/task.c` から `task.h` を include できる。
  - _Requirements: 2.3, 8.7, 8.8, 8.9, 9.1_
  - _Boundary: Task Public Contract_

- [x] 2. タスク管理モジュールの内部状態と内部ヘルパを追加する
- [x] 2.1 `kernel/task.c` に静的タスクテーブルと ID 状態を追加する
  - `task_table[MAX_TASKS]` を静的配列として定義する。
  - `next_task_id` を静的状態として定義する。
  - タスクレコード管理で動的メモリを使わない。
  - 完了状態: タスク管理状態が `kernel/task.c` 内に閉じ、外部から直接変更できない。
  - _Requirements: 2.2, 2.3, 6.1, 6.2, 6.4, 6.5, 8.9_
  - _Boundary: Task Registry_

- [x] 2.2 空きスロット探索を追加する
  - `find_free_slot` 相当の内部処理で `task_table` を走査する。
  - 空き判定は `state == TASK_STATE_UNUSED` のみにする。
  - `id == 0` や `name == NULL` を空き判定に使わない。
  - 完了状態: 空きスロット探索の条件が state のみに限定されていることをコード上で確認できる。
  - _Requirements: 2.4, 2.5, 2.6, 4.11, 5.5_
  - _Boundary: Task Registry_

- [x] 2.3 ID 採番処理を追加する
  - `allocate_task_id` 相当の内部処理で `next_task_id` を単純インクリメントする。
  - ID `0` を割り当てない。
  - オーバーフロー時は `TASK_ERR_ID_OVERFLOW` を返し、`next_task_id` を過去の値へ巻き戻さない。
  - 完了状態: 成功時は `1` 以上の ID が返り、失敗時は ID オーバーフローの負値が返る。
  - _Requirements: 4.1, 5.6, 6.1, 6.2, 6.3, 6.4, 6.5, 6.6, 6.7, 6.8_
  - _Boundary: Task Registry_

- [x] 2.4 タスク状態の文字列表現を追加する
  - `TASK_STATE_UNUSED`、`TASK_STATE_DORMANT`、`TASK_STATE_READY`、`TASK_STATE_RUNNING` を dump 用文字列へ変換する。
  - 未知の状態値は判別可能な fallback 文字列にする。
  - 完了状態: `task_dump` が状態値ではなく状態文字列を出力できる。
  - _Requirements: 1.2, 7.6_
  - _Boundary: Task Dump_

- [x] 3. `task_init` を実装する
- [x] 3.1 全スロット初期化を実装する
  - 全スロットの `state` を `TASK_STATE_UNUSED` にする。
  - 全スロットの `id` を `0` にする。
  - 全スロットの `name`、`entry`、`stack_base` を `NULL` にする。
  - 全スロットの `priority`、`stack_size` を `0` にする。
  - `next_task_id` を `1` に戻す。
  - 完了状態: `task_init` 後のテーブルが requirements.md の初期値と一致する。
  - _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 3.7, 6.2_
  - _Boundary: Task Registry_

- [x] 3.2 初期化ログを HAL console 経由で出力する
  - `task_init` 完了時に初期化が分かるログを出す。
  - ログ出力は HAL console API のみに限定する。
  - 完了状態: QEMU シリアルログで task init のログを確認できる。
  - _Requirements: 7.10, 9.3_
  - _Boundary: Task Registry, Task Dump_

- [x] 4. `task_register` を実装する
- [x] 4.1 引数検証と `TASK_ERR_INVAL` を実装する
  - `name == NULL` を `TASK_ERR_INVAL` にする。
  - `entry == NULL` を `TASK_ERR_INVAL` にする。
  - `stack_base == NULL` を `TASK_ERR_INVAL` にする。
  - `stack_size == 0` を `TASK_ERR_INVAL` にする。
  - 完了状態: 不正引数では TCB が登録されず、負の引数不正エラーが返る。
  - _Requirements: 4.7, 4.8, 4.9, 4.10, 5.4_
  - _Boundary: Task Registry_

- [x] 4.2 空きスロットなしの処理を実装する
  - 空きスロット探索を呼び出して登録先を決める。
  - 空きスロットがない場合は `TASK_ERR_FULL` を返す。
  - 完了状態: `TASK_STATE_UNUSED` のスロットがない場合、登録処理はテーブルを変更せず満杯エラーを返す。
  - _Requirements: 2.4, 2.5, 2.6, 4.11, 5.5_
  - _Boundary: Task Registry_

- [x] 4.3 ID 採番と TCB 設定を実装する
  - ID 採番処理を呼び出し、オーバーフロー時は `TASK_ERR_ID_OVERFLOW` を返す。
  - 成功時は `id`、`name`、`entry`、`priority`、`stack_base`、`stack_size` を TCB に保存する。
  - 登録直後の `state` を `TASK_STATE_READY` にする。
  - 成功時は割り当てた task id を返す。
  - 完了状態: 複数回登録すると、配列インデックスとは独立した `1` 以上の ID が順に割り当てられる。
  - _Requirements: 1.3, 1.4, 1.5, 4.1, 4.2, 4.3, 5.6, 6.1, 6.3, 6.4, 6.5, 6.6, 6.7, 6.8_
  - _Boundary: Task Registry_

- [x] 4.4 登録時の非対象動作と登録ログを確認可能にする
  - `task_register` 内で entry 関数を呼び出さない。
  - スタック初期化と実行コンテキスト作成を行わない。
  - 登録成功時に `id`、`name`、`state`、`priority`、`entry`、`stack_base`、`stack_size` が分かるログを HAL console 経由で出す。
  - 完了状態: 登録ログは出るが、entry 関数由来のログや実行動作は発生しない。
  - _Requirements: 4.4, 4.5, 4.6, 7.10, 8.3, 8.4, 8.5, 8.6, 9.3, 9.6_
  - _Boundary: Task Registry, Task Dump_

- [x] 5. `task_dump` を実装する
- [x] 5.1 登録済みタスクだけを走査・選別する
  - `task_table` の全スロットを走査する。
  - `TASK_STATE_UNUSED` のスロットを出力対象から除外する。
  - 登録済みタスクだけが dump 対象になるようにする。
  - 完了状態: 未使用スロットは dump ログに現れない。
  - _Requirements: 7.1, 7.2_
  - _Boundary: Task Dump_

- [x] 5.2 dump の開始・終了と全項目出力を実装する
  - dump 開始ログと終了ログを出す。
  - 各登録済みタスクについて `id`、`name`、`priority`、状態文字列、`entry` アドレス、`stack_base`、`stack_size` を出力する。
  - ログ出力は HAL console API のみに限定する。
  - 完了状態: QEMU シリアルログで登録済みタスクの全指定項目を一覧確認できる。
  - _Requirements: 7.3, 7.4, 7.5, 7.6, 7.7, 7.8, 7.9, 7.10, 9.3, 9.5_
  - _Boundary: Task Dump_

- [x] 6. `kernel_main` に起動時確認フローを接続する
- [x] 6.1 サンプル登録用の task entry と静的スタック領域を用意する
  - `task_a` と `task_b` 相当のダミー entry 関数を定義する。
  - 各タスク用の静的スタック領域を用意する。
  - entry 関数内に識別用ログを置く場合でも、今回の起動フローでは呼び出されない状態にする。
  - 完了状態: 登録に使う entry と stack 情報は存在するが、entry 実行ログは出ない。
  - _Requirements: 1.5, 4.4, 4.5, 8.3, 8.4, 8.5, 9.4, 9.6_
  - _Boundary: Kernel Boot Hook_

- [x] 6.2 起動時に `task_init`、複数登録、`task_dump` を呼び出す
  - `kernel/kernel.c` で `task.h` を include する。
  - `hal_console_init` と既存起動ログの後に `task_init` を呼ぶ。
  - `task_register` で複数タスクを登録する。
  - 登録後に `task_dump` を呼ぶ。
  - 完了状態: QEMU 起動時に初期化、複数登録、dump のログがこの順で確認できる。
  - _Requirements: 3.7, 9.2, 9.3, 9.4, 9.5, 9.6, 9.7, 9.8_
  - _Boundary: Kernel Boot Hook_

- [x] 6.3 `task_register` の戻り値を起動ログで確認できるようにする
  - 各 `task_register` の戻り値を HAL console 経由で出力する。
  - 成功時 ID と負のエラーコードを区別して読めるログにする。
  - 完了状態: 起動ログから登録結果の task id を確認できる。
  - _Requirements: 4.1, 5.1, 5.2, 5.3, 9.3, 9.4_
  - _Boundary: Kernel Boot Hook, Task Dump_

- [x] 7. ビルド統合を行う
- [x] 7.1 `kernel/task.c` を Makefile のビルド対象に追加する
  - `kernel/task.c` の object を compile/link 対象に追加する。
  - `kernel/include/task.h` の依存関係を必要なルールに追加する。
  - 既存 include path で足りる場合は新しい include path を増やさない。
  - 完了状態: `make` が `kernel/task.c` を含めて kernel image を生成する。
  - _Requirements: 9.1_
  - _Boundary: Kernel Boot Hook, Task Registry_

- [x] 8. 動作確認と非対象範囲の確認を行う
- [x] 8.1 ビルドと QEMU シリアルログを確認する
  - `make` が成功することを確認する。
  - QEMU で kernel が起動することを確認する。
  - `-serial stdio` または既存 run ターゲットのログで task init、task register、task dump の出力を確認する。
  - 完了状態: シリアルログに複数タスクの登録結果と dump 一覧が出る。
  - _Requirements: 9.1, 9.2, 9.3, 9.4, 9.5_
  - _Boundary: Kernel Boot Hook, Task Dump_

- [x] 8.2 entry 非実行と実行機構非導入を確認する
  - entry 関数由来のログが出ないことを確認する。
  - scheduler 動作と context-switch 動作が起動ログやコードに追加されていないことを確認する。
  - 割り込み、タイマ、動的メモリが今回の task 管理に追加されていないことを確認する。
  - 完了状態: 登録と dump は動作するが、タスク実行・切替・スケジューリングは発生しない。
  - _Requirements: 8.1, 8.2, 8.3, 8.4, 8.5, 8.6, 8.7, 8.8, 8.9, 9.6, 9.7, 9.8_
  - _Boundary: Task Registry, Kernel Boot Hook_

- [x] 8.3 非対象 API と非対象処理が追加されていないことを確認する
  - `task_start` を実装していないことを確認する。
  - `scheduler_start` を実装していないことを確認する。
  - `context_switch` を実装していないことを確認する。
  - timer interrupt、stack frame setup、dynamic allocation を追加していないことを確認する。
  - 完了状態: 今回の差分が task 登録台帳とログ確認の範囲に留まっている。
  - _Requirements: 4.4, 4.5, 4.6, 8.1, 8.2, 8.3, 8.4, 8.5, 8.6, 8.7, 8.8, 8.9_
  - _Boundary: Task Registry, Kernel Boot Hook_

