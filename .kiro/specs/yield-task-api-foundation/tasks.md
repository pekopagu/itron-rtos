# Implementation Plan

- [x] 1. API入口の土台
- [x] 1.1 yield_tsk風API層を追加する
  - `yield_tsk()` の公開入口と戻り値契約を追加する。
  - current task がRUNNINGの場合は id/name/state と deferred reason をログへ出す。
  - current task が存在しない、またはRUNNINGではない場合は invalid-current-state として負値を返す。
  - 完了時には `yield_tsk()` 呼び出しログがQEMU serial logで観測できる。
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 2.1_
  - _Boundary: ItronApi_

- [x] 2. boot smoke統合
- [x] 2.1 10.1の非接続境界を保ったままboot smokeへ組み込む
  - `yield_tsk()` を起動時smokeから1回呼び、entry returnとは別のAPI呼び出しとして観測する。
  - `yield_tsk()` ではRUNNING->READY、scheduler選択、dispatcher switch、context switchを行わない。
  - 完了時には9.1から9.4の既存ログが維持され、yieldログが追加される。
  - _Requirements: 2.2, 2.3, 2.4, 3.1, 3.2, 3.3, 3.4_
  - _Boundary: KernelSmoke, ItronApi_

- [x] 3. 文書と検証
- [x] 3.1 文書と検証ログを10.1へ更新する
  - READMEに第10章10.1の到達点と未実装範囲を追記する。
  - Zenn Articles表へ `v10.1-yield-task-api-foundation` の候補行を追加する。
  - `docs/logs/qemu-serial.log` を実行結果で更新する。
  - 完了時には `.kiro/specs/yield-task-api-foundation/` が `requirements.md`、`design.md`、`tasks.md` の3ファイルだけになる。
  - _Requirements: 3.5, 3.6_
  - _Boundary: Documentation, Runtime log_
