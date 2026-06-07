# Design Document

## Overview
第14章14.4では、μITRON風API層の戻り値を共通の `ER` 型と `E_*` エラーコードへ整理する。対象は `cre_tsk()` / `sta_tsk()` / `slp_tsk()` / `wup_tsk()` / `wai_sem()` / `sig_sem()` / `pol_sem()` / `twai_sem()` / `dly_tsk()` / `yield_tsk()` である。task module、semaphore module、delay queue module、dispatcher、preemptionの状態遷移は変更せず、API層で内部戻り値を共通エラーコードへ変換する。

## Boundary Commitments
- このspecは `kernel/include/itron_api.h` の公開戻り値型と共通エラーコード定義を所有する。
- このspecは `kernel/itron_api.c` のAPI完了ログと戻り値変換を所有する。
- このspecは `docs/logs/qemu-serial.log`、README、Doxygenコメントの14.4更新を所有する。
- task/semaphore/delay queueの内部エラーコードは必要な範囲で参照するが、内部API全体を公開APIエラー体系へ置換しない。
- timer IRQ handler本体からAPIや `dispatcher_switch_to()` を呼ばない既存方針を維持する。

## Out of Boundary
- 本物のμITRON仕様の完全再現。
- 全サービスコールエラーコードの網羅。
- service call context、CPU lock、dispatch disable、interrupt contextの本格判定。
- `cre_sem`、`del_sem`、`ref_sem`、`rel_wai`、`sus_tsk`、`rsm_tsk`、`ter_tsk`、`ext_tsk`、`exd_tsk`、task削除、semaphore削除、priority順semaphore wait queue、同一優先度time slice、round-robin。

## Architecture

### Public API Error Contract
`kernel/include/itron_api.h` に以下を定義する。

- `typedef int ER;`
- `typedef int ID;`
- `E_OK`
- `E_ID`
- `E_PAR`
- `E_CTX`
- `E_OBJ`
- `E_TMOUT`
- `E_RLWAI`
- `E_QOVR`

既存の `*_OK` / `*_ERR_*` マクロは対象APIの戻り値として使わない。後続の利用者が旧名に依存しないよう、Doxygenコメントは `ER` と `E_*` を説明する。

### Error Name Logging
`kernel/itron_api.c` に `const char *itron_error_name(ER ercd)` を追加する。API完了ログは整数ではなく `result=E_OK` のような名前を出す。未知の内部値は `E_UNKNOWN` としてログ化するが、公開エラーコードとしては増やさない。

### API-Level Mapping
API層は内部module戻り値を以下へ変換する。

- 不正task IDまたは不正semaphore ID: `E_ID`
- NULL pointer、entryなし、stackなし、size 0、timeout 0、delay 0: `E_PAR`
- current task未設定またはRUNNINGでないtask-context API呼び出し: `E_CTX`
- 対象task/semaphore状態が要求と合わない: `E_OBJ`
- semaphore count 0の `pol_sem()`、delay queue timeoutでREADY復帰した `twai_sem()` waiter: `E_TMOUT`
- count上限超過: `E_QOVR`
- 成功: `E_OK`

`wai_sem()` がcount 0でWAITINGへ遷移し、dispatcher境界へ到達する既存挙動は成功として `E_OK` を返す。`pol_sem()` はWAITINGへ遷移せず、即時取得できなかったことを `E_TMOUT` として返す。

### Timeout Completion Logging
`twai_sem()` 自体はtimeout待ち登録時に成功として `E_OK` を返す。timeout到達はdelay queue tick処理側で発生するため、`delay_queue_tick()` がtimeout付きsemaphore waiterをREADYへ戻すログに `result=E_TMOUT action=timeout` を追加する。これにより、API呼び出し時の待ち登録成功と、後続tickでのtimeout完了を区別できる。

## File Structure Plan
- `kernel/include/itron_api.h`: `ER` / `ID` / `E_*` 定義、`itron_error_name()` 宣言、対象APIプロトタイプの戻り値を `ER` へ変更、Doxygenコメント更新。
- `kernel/itron_api.c`: `itron_error_name()` 実装、API完了ログの `result=<error-name>` 化、内部戻り値から `ER` への変換。
- `kernel/delay_queue.c`: timeout付きsemaphore waiterのtimeout到達ログへ `result=E_TMOUT` を追加する。
- `README.md`: Zenn Articles表に14.4を追加し、14.4の到達点と共通エラーコード意味を記載する。
- `docs/logs/qemu-serial.log`: `make run` の最新ログへ更新する。
- `.kiro/specs/itron-error-code-system/requirements.md`: 要求を保持する。
- `.kiro/specs/itron-error-code-system/design.md`: 本設計を保持する。
- `.kiro/specs/itron-error-code-system/tasks.md`: 実装タスクと完了状態を保持する。

## Testing Strategy
- `make` で通常buildが通ることを確認する。
- `make run` でQEMU boot smokeが通り、`docs/logs/qemu-serial.log` が更新されることを確認する。
- `make run VALIDATE_TIMER_IRQ_ENTRY=1` でtimer IRQ entry validationが通ることを確認する。
- QEMU serial logで `cre_tsk()` 成功が `E_OK`、不正引数が `E_PAR` として出ることを確認する。
- QEMU serial logで `sta_tsk()` 不正IDが `E_ID`、不正状態が `E_OBJ` として出ることを確認する。
- QEMU serial logで `slp_tsk()` 成功、`wup_tsk()` 不正ID、不正状態が共通エラー名で出ることを確認する。
- QEMU serial logで `wai_sem()` / `sig_sem()` / `pol_sem()` / `twai_sem()` / `dly_tsk()` / `yield_tsk()` の完了ログが共通エラー名を使うことを確認する。
- `pol_sem()` の取得失敗がWAITING遷移なしで `E_TMOUT` になることを確認する。
- timeout付きsemaphore waiterのtimeout到達ログが `E_TMOUT` を示すことを確認する。

## Requirement Traceability
- 1.1-1.8: `itron_api.h` の `ER` / `ID` / `E_*` と `itron_error_name()`。
- 2.1-2.13: `itron_api.c` の対象API戻り値変換。
- 3.1-3.8: API完了ログとdelay queue timeoutログ。
- 4.1-4.7: spec、README、Doxygen、QEMU log、build/run検証、specディレクトリ整理。

## Revalidation Triggers
- 既存task状態遷移APIの戻り値意味を変更した場合。
- semaphore wait queueやdelay queueの所有責務を変更した場合。
- timer IRQ handler本体からAPIまたはdispatcherを直接呼ぶ変更が入った場合。
- 公開エラーコード値または名前を変更した場合。
