# Implementation Plan

- [x] 1. task entry returnのDORMANT最終化を実装する
  - `task_mark_dormant_from_entry_return()`をtask管理層に追加し、RUNNINGまたはREADYのtaskだけをDORMANTへ遷移させる。
  - `task_context_enter()`のentry return観測点でreturnedログとfinalizedログを分けて出力する。
  - 9.3の`dispatcher_switch_to()`による`from RUNNING->READY`と`to READY->RUNNING`の状態遷移は変更しない。
  - 完了時、task_bは9.3のREADY経由からDORMANTへ、task_cはRUNNINGからDORMANTへ最終化される。
  - _Boundary:_ task management / task_context lifecycle finalization
  - _Requirements:_ 1.1, 1.2, 1.3, 1.4, 2.1, 2.2, 2.3, 2.4

- [x] 2. 9.4の文書と観測ログを更新する
  - READMEに第9章9.4の到達点、Zenn tag候補、未実装範囲を追記する。
  - Doxygenコメントでtask_context層の責務がentry return後のlifecycle確定に限定されることを明記する。
  - `docs/logs/qemu-serial.log`を`make run`結果で更新する。
  - `.kiro/specs/task-entry-return-finalization/`を`requirements.md`、`design.md`、`tasks.md`だけに整理する。
  - _Depends:_ 1
  - _Boundary:_ documentation / validation evidence
  - _Requirements:_ 3.1, 3.2, 3.3, 3.4, 4.1, 4.2, 4.3, 4.4
