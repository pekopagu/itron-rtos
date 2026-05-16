# Implementation Plan

- [x] 1. セマフォ基盤の公開契約とbuild統合を追加する
- [x] 1.1 セマフォの公開型・戻り値・API契約を定義する
  - セマフォがid、name、count、max_countを持つことを公開ヘッダから確認できるようにする
  - 初期化、作成、取得、返却、dumpの呼び出し口を宣言する
  - Doxygenコメントでtimer、preemption、timeout、wait queue未接続の土台であることを明記する
  - 完了時には他moduleがセマフォAPIをincludeできる
  - _Requirements: 1.1, 1.4, 2.1, 4.1, 5.2, 6.4, 6.5_

- [x] 1.2 セマフォ実装をbuild対象へ追加する
  - 新規セマフォ実装ファイルがfreestanding kernelのobject listに含まれるようにする
  - `kernel.c` とセマフォ実装のheader依存をMakefileへ反映する
  - 完了時には空または最小実装のセマフォmoduleを含めても `make` がリンク対象を解決できる
  - _Requirements: 1.4, 6.1_

- [x] 2. task側のセマフォ待ち状態を観測可能にする
- [x] 2.1 TCBにセマフォ待ち理由を保持しtask dumpへ表示する
  - task初期化と登録時に待ちなしを表す値へ初期化する
  - task dumpにstateとwait_sem_idを出力する
  - 完了時には既存task dump行から各taskのwait_sem_idを確認できる
  - _Requirements: 3.2, 5.1, 6.3, 6.4_

- [x] 2.2 セマフォ待ちによるWAITING/READY遷移APIを追加する
  - 指定taskをセマフォ待ちWAITINGへ移し、wait_sem_idを記録できるようにする
  - 指定セマフォを待つtaskを1件READYへ戻し、wait_sem_idを0へ戻せるようにする
  - WAITING化とREADY復帰のserial logを出力する
  - 完了時にはtask moduleだけがTCB状態とwait_sem_idを更新する
  - _Requirements: 3.1, 3.2, 3.3, 4.2, 4.3, 6.4_

- [x] 3. セマフォtableと操作を実装する
- [x] 3.1 セマフォ初期化と作成を実装する
  - 静的tableを初期化し、未使用slotとID採番を管理する
  - 有効な初期値ではid、name、count、max_countを保持し、初期化ログを出す
  - initial_countがmax_countを超える場合や不正入力では失敗を返す
  - 完了時にはQEMU logで `[sem] initialized` 行を確認できる
  - _Requirements: 1.1, 1.2, 1.3, 1.4_

- [x] 3.2 wai_sem相当操作を実装する
  - countが1以上ならcountを1減らして成功ログを出す
  - countが0ならtask側APIを通じて対象taskをWAITINGにする
  - 不正セマフォidではtask状態を変更せず失敗を返す
  - 完了時にはcount減少とWAITING遷移がserial logから確認できる
  - _Requirements: 2.1, 2.2, 2.3, 3.1, 3.2, 3.3, 3.4_

- [x] 3.3 sig_sem相当操作とsemaphore dumpを実装する
  - 対象セマフォを待つtaskがあれば1件だけREADYへ戻す
  - WAITING taskがなければmax_countを超えない範囲でcountを増やす
  - max_count超過時はcountを変更せず失敗を返す
  - semaphore dumpでdump start/endとid、name、count、max_countを出力する
  - 完了時にはwakeupログとsemaphore dumpからcount/max_countを確認できる
  - _Requirements: 4.1, 4.2, 4.3, 4.4, 4.5, 5.2_

- [x] 4. 起動時semaphore smokeを既存経路へ統合する
- [x] 4.1 semaphore smoke sequenceを追加する
  - task登録後にセマフォを作成し、task_bで成功取得、task_cでWAITING、sig_semでwakeupを実行する
  - smoke sequence後にtask dumpとsemaphore dumpを出力する
  - WAITING taskをREADYへ戻してから既存minimal-context-switch smoke pathへ進む
  - 完了時には期待ログ順にセマフォ初期化、wai_sem成功、WAITING、sig_sem wakeup、dumpが出力される
  - _Requirements: 5.3, 6.2, 6.3, 6.4_

- [x] 5. feature全体を検証する
- [x] 5.1 buildとQEMU serial smokeを実行して回帰を確認する
  - `make` が成功することを確認する
  - `make run` が成功し、semaphore初期化、count減少、WAITING遷移、wait_sem_id、sig_sem wakeup、semaphore dumpを確認する
  - 既存のminimal-context-switch smoke pathとcooperative runnerのログが残っていることを確認する
  - scheduler、dispatcher、context switch、arch層にセマフォ責務が混ざっていないことをdiffで確認する
  - 完了時には検証結果を実装ノートまたは最終報告で説明できる
  - _Requirements: 1.2, 2.2, 3.3, 4.4, 5.1, 5.2, 5.3, 6.1, 6.2, 6.3, 6.4, 6.5_

## Implementation Notes
