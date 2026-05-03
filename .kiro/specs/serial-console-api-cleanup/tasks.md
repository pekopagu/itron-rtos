# 実装計画

- [x] 1. 現状確認を行う
  - 目的: 第4回で動作していた COM1 シリアル出力の現在地を把握し、API 整理の変更範囲を固定する。
  - 対象ファイル: `arch/x86_64/serial.c`, `arch/x86_64/serial.h`, `kernel/kernel.c`
  - 作業内容: 現在の `serial_write_string` 宣言、実装、`kernel_main` からの呼び出しを確認する。
  - 完了条件: `serial_write_string("kernel_main reached\r\n")` の既存呼び出し位置、COM1 `0x3F8`、ポーリング送信処理、既存初期化値が確認できている。
  - 注意点: この時点ではコードを変更せず、`boot/boot.asm`、`linker.ld`、`Makefile` を変更対象に含めない。
  - _Requirements: 4.1, 4.2, 4.3, 6.2, 6.3, 6.4, 6.5, 6.6_

- [x] 2. 公開APIをヘッダで整理する
  - 目的: シリアルコンソールの公開 API を `serial_init`、`serial_putc`、`serial_write` の3つに整理する。
  - 対象ファイル: `arch/x86_64/serial.h`
  - 作業内容: `serial_init`、`serial_putc`、`serial_write` を宣言し、`serial_write_string` 宣言を削除する。
  - 完了条件: `arch/x86_64/serial.h` に3つの公開 API だけが宣言され、`serial_write_string` 宣言が残っていない。
  - 注意点: ヘッダガード、SPDX コメント、対象外の定義は変更しない。
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 2.5_

- [x] 3. serial.c の関数責務を整理する
  - 目的: 既存 COM1 初期化とポーリング送信を維持しながら、公開 API と内部送信関数の責務を分ける。
  - 対象ファイル: `arch/x86_64/serial.c`
  - 作業内容: `serial_init` を公開関数にし、既存の1文字送信処理を raw 送信用の内部関数として整理し、`serial_putc` と `serial_write` の土台を追加する。
  - 完了条件: `serial_init` が `static` ではなくなり、COM1 `0x3F8`、既存初期化値、送信可能待ちのポーリング方式が維持されている。
  - 注意点: 割り込み、入力、リングバッファ、タイマ、タスク管理、スケジューラに関する処理を追加しない。
  - _Requirements: 1.1, 1.2, 1.3, 2.4, 4.1, 4.2, 4.3, 4.4, 4.5, 4.6_

- [x] 4. 改行変換を serial_putc に集約する
  - 目的: 呼び出し側が `\r\n` を直接意識しなくても、シリアル端末で読みやすい改行を出力できるようにする。
  - 対象ファイル: `arch/x86_64/serial.c`
  - 作業内容: `serial_putc('\n')` では `'\r'` を先に raw 送信してから `'\n'` を送信し、`serial_putc('\r')` は `'\r'` だけを送る。`serial_write` は文字ごとに `serial_putc` を呼ぶ。
  - 完了条件: `serial_write("a\n")` 相当の出力が `a\r\n` になり、`serial_putc('\r')` が二重変換されない構造になっている。
  - 注意点: raw 送信関数には改行変換を入れず、変換責務を `serial_putc` に限定する。
  - _Requirements: 2.2, 2.3, 2.4, 5.6_

- [x] 5. serial_write の NULL 安全性を実装する
  - 目的: デバッグ文字列が `NULL` の場合でもカーネルがクラッシュしないようにする。
  - 対象ファイル: `arch/x86_64/serial.c`
  - 作業内容: `serial_write` が `NULL` を受け取った場合は何もせず戻るようにする。
  - 完了条件: `serial_write(NULL)` のコードパスで参照外しが発生せず、出力も状態変更も行わず戻る。
  - 注意点: エラー表示、ログレベル、戻り値追加などの新しい API 契約は追加しない。
  - _Requirements: 2.1, 2.3_

- [x] 6. kernel_main の起動ログ出力を更新する
  - 目的: 起動ログを整理後の公開 API で出力し、第4回の起動到達確認を維持する。
  - 対象ファイル: `kernel/kernel.c`
  - 作業内容: `kernel_main` で `serial_init()` を明示的に呼び、`serial_write("itron-rtos booting...\n")` と `serial_write("kernel_main reached\n")` を出力する。`serial_write_string` 呼び出しを削除する。
  - 完了条件: `kernel/kernel.c` が `serial_write_string` を使わず、起動時に2つの指定ログを `serial_write` で出力する。
  - 注意点: 既存の無限 halt loop は維持し、タスク管理、タイマ、スケジューラの初期化を追加しない。
  - _Requirements: 1.5, 3.1, 3.2, 3.3, 3.4, 3.5, 4.6_

- [x] 7. ビルド確認を行う
  - 目的: API 整理後も最小カーネルのビルド経路が壊れていないことを確認する。
  - 対象ファイル: `Makefile`, `build/kernel.elf`
  - 作業内容: `make clean` ターゲットがある場合は実行し、その後 `make` を実行する。
  - 完了条件: `make` が成功し、`build/kernel.elf` が生成されている。
  - 注意点: Makefile は必要がなければ変更せず、ビルド失敗時もまず API 宣言と呼び出し不一致を確認する。
  - _Requirements: 5.1, 5.2, 6.5_

- [x] 8. QEMU の serial stdio で起動ログを確認する
  - 目的: QEMU 上で整理後のログが実際にシリアル出力として観測できることを確認する。
  - 対象ファイル: `build/kernel.elf`
  - 作業内容: `qemu-system-x86_64 -kernel build/kernel.elf -serial stdio -display none -no-reboot` 相当のコマンドで起動し、標準出力にログが出ることを確認する。
  - 完了条件: QEMU 実行時に `itron-rtos booting...` と `kernel_main reached` が確認でき、改行が端末上で読みやすい。
  - 注意点: QEMU の実行は必要最小限の時間で止め、起動確認以外のデバイス対応や Makefile 変更に広げない。
  - _Requirements: 5.3, 5.4, 5.5, 5.6_

- [x] 9. qemu-serial.log を必要最小限で更新する
  - 目的: 第5回の QEMU 確認結果を短い証跡として残す。
  - 対象ファイル: `docs/logs/qemu-serial.log`
  - 作業内容: QEMU 確認結果に合わせて `itron-rtos booting...` と `kernel_main reached` を含む最小ログへ更新する。
  - 完了条件: `docs/logs/qemu-serial.log` に期待ログが含まれ、大量ログや環境依存の不要情報が残っていない。
  - 注意点: ログファイルは証跡に限定し、生成物や `build/` 配下を変更対象にしない。
  - _Requirements: 5.4, 5.5, 6.2_

- [x] 10. README のシリアル確認手順を更新する
  - 目的: 開発者が同じ手順でビルドと QEMU シリアル出力を確認できるようにする。
  - 対象ファイル: `README.md`
  - 作業内容: `make` の実行手順、QEMU の `-serial stdio` を含む実行コマンド、期待されるログ出力例を記載または更新する。
  - 完了条件: README に `-serial stdio` を含む確認手順と、`itron-rtos booting...`、`kernel_main reached` の期待出力例が記載されている。
  - 注意点: README 更新はシリアル出力確認手順に限定し、ロードマップやライセンス方針など無関係な節を変更しない。
  - _Requirements: 5.3, 5.4, 5.5, 5.6, 6.1, 6.2_

- [x] 11. 最終レビューを行う
  - 目的: requirements と design の受け入れ条件を満たし、既存の最小カーネル起動を壊していないことを確認する。
  - 対象ファイル: `arch/x86_64/serial.c`, `arch/x86_64/serial.h`, `kernel/kernel.c`, `README.md`, `docs/logs/qemu-serial.log`
  - 作業内容: `serial_write_string` が残っていないこと、`boot/boot.asm`、`linker.ld`、`Makefile` が変更されていないこと、割り込み、タイマ、タスク管理に関する実装変更がないことを確認する。`make` と QEMU 確認結果をまとめる。
  - 完了条件: 全 requirements の受け入れ条件、design.md の方針、変更範囲制約、ビルド結果、QEMU 出力結果が確認済みである。
  - 注意点: 差分に非スコープの変更が含まれる場合は完了扱いにせず、対象範囲へ戻す。
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 2.1, 2.2, 2.3, 2.4, 2.5, 3.1, 3.2, 3.3, 3.4, 3.5, 4.1, 4.2, 4.3, 4.4, 4.5, 4.6, 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 6.1, 6.2, 6.3, 6.4, 6.5, 6.6_
