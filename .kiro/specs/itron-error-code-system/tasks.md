# Implementation Plan

## 1. 公開エラーコード契約を定義する

- [x] 1.1 `itron_api.h` に共通戻り値型とエラーコードを追加する
  - `ER` / `ID` / `E_OK` / `E_ID` / `E_PAR` / `E_CTX` / `E_OBJ` / `E_TMOUT` / `E_RLWAI` / `E_QOVR` が公開ヘッダで確認できる。
  - 対象APIのプロトタイプが `ER` を返し、Doxygenコメントが各主要戻り値の意味を説明している。
  - _Boundary:_ PublicAPIHeader
  - _Requirements:_ 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 4.2

## 2. API層の戻り値とログを共通エラーコードへ変換する

- [x] 2.1 `itron_error_name()` と完了ログ helper を追加する
  - API完了ログで `result=E_OK` のような名前が出力され、未知値はログ上で区別できる。
  - _Boundary:_ ItronAPILogging
  - _Depends:_ 1.1
  - _Requirements:_ 3.1

- [x] 2.2 task API群の戻り値を共通エラーコードへ寄せる
  - `cre_tsk()` / `sta_tsk()` / `slp_tsk()` / `wup_tsk()` / `yield_tsk()` が成功、不正ID、不正引数、不正状態、不正文脈を `E_*` で返す。
  - 既存のREADY/WAITING/DORMANT遷移ログとpreemption pendingログが維持されている。
  - _Boundary:_ TaskAPIErrorMapping
  - _Depends:_ 2.1
  - _Requirements:_ 2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 2.7, 2.13, 3.2, 3.4, 3.5, 3.8

- [x] 2.3 semaphore API群の戻り値を共通エラーコードへ寄せる
  - `wai_sem()` / `sig_sem()` / `pol_sem()` / `twai_sem()` が成功、不正semid、不正文脈、不正状態、poll失敗、overflowを `E_*` で返す。
  - `pol_sem()` の取得失敗はWAITINGへ遷移せず `E_TMOUT` としてログ化される。
  - _Boundary:_ SemaphoreAPIErrorMapping
  - _Depends:_ 2.1
  - _Requirements:_ 2.8, 2.9, 2.10, 2.11, 3.3, 3.5, 3.6, 3.8

- [x] 2.4 delay APIとtimeout到達ログを共通エラーコードへ寄せる
  - `dly_tsk()` の成功、不正引数、不正文脈、queue資源不足が共通エラー名でログ化される。
  - timeout付きsemaphore waiterのtimeout到達が `result=E_TMOUT action=timeout` としてQEMU logで確認できる。
  - _Boundary:_ DelayAndTimeoutErrorMapping
  - _Depends:_ 2.1
  - _Requirements:_ 2.12, 2.13, 3.7, 3.8

## 3. 文書と検証ログを更新する

- [x] 3.1 READMEとspecの14.4到達点を整理する
  - READMEに `v14.4-itron-error-code-system` と共通エラーコードの意味が記載される。
  - `.kiro/specs/itron-error-code-system/` の3文書が14.4の範囲を説明している。
  - _Boundary:_ Documentation
  - _Requirements:_ 4.1, 4.3

- [x] 3.2 build/run検証を実行し、QEMU serial logを更新する
  - `make`、`make run`、`make run VALIDATE_TIMER_IRQ_ENTRY=1` が成功する。
  - `docs/logs/qemu-serial.log` に共通エラーコード名のAPI完了ログが残る。
  - timer IRQ handler本体からAPIや `dispatcher_switch_to()` を直接呼んでいないことを確認する。
  - _Boundary:_ Validation
  - _Depends:_ 2.2, 2.3, 2.4, 3.1
  - _Requirements:_ 4.4, 4.5, 4.6

- [x] 3.3 specディレクトリを最終形へ整理する
  - `.kiro/specs/itron-error-code-system/` が `requirements.md`、`design.md`、`tasks.md` の3ファイルだけになる。
  - _Boundary:_ SpecPackaging
  - _Depends:_ 3.2
  - _Requirements:_ 4.7

## Implementation Notes
- 14.4では旧API別戻り値名を公開ヘッダ内で共通 `E_*` の別名として残し、既存呼び出し側を壊さずに戻り値の意味を統一した。
