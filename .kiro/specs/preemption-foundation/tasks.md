# Implementation Plan

- [x] 1. Preemption decisionのscheduler契約を追加する
  - preemption判断結果として、発生、非発生、入力不正を区別できる結果を公開する。
  - 現在taskと候補taskを読み取り専用で返し、dispatcher currentやTCB状態を変更しない契約をDoxygenで説明する。
  - 完了時にはscheduler headerからpreemption decision helperを利用でき、責務境界がコメントで確認できる。
  - _Requirements: 1.2, 2.1, 2.2, 2.3, 2.4, 3.1, 3.2, 6.1, 6.3_
  - _Boundary: Scheduler Preemption Helper_

- [x] 2. Schedulerで高優先度READY候補の判定を実装する
  - 既存のREADY選択規則に沿って最高優先度READY taskを候補として扱う。
  - currentがNULL、currentがRUNNINGでない、READY候補なし、候補priorityが同等以下の各ケースを区別して返す。
  - 候補priorityがcurrent priorityより小さい場合だけpreemption neededとして返す。
  - 完了時にはscheduler helper単体でcurrentより高優先度のREADY taskだけがcandidateになる。
  - _Requirements: 1.2, 1.3, 2.1, 2.2, 2.3, 2.4, 3.1, 3.2, 4.4_
  - _Boundary: Scheduler Preemption Helper_

- [x] 3. Timer契機のboot-time preemption smokeを追加する
  - timer tickを明示的に進めた後、dispatcher currentを取得してschedulerのpreemption decision helperへ渡す。
  - timer tick更新とpreemption判断を別々の処理として呼び出し順がログから追えるようにする。
  - このsmoke helper内ではcurrent確定やcontext switchを直接実行せず、切り替え候補の観測に留める。
  - 完了時にはQEMU serial logでtimer tick後のpreemption判断が確認できる。
  - _Requirements: 1.1, 1.3, 1.4, 3.1, 3.3, 4.3, 5.2, 6.2, 6.4_
  - _Boundary: Kernel Preemption Smoke_
  - _Depends: 1, 2_

- [x] 4. Preemption発生・非発生ログを整備する
  - preemption発生時にcurrent task、candidate task、双方のpriority、判定理由を`[preempt]`ログとして出力する。
  - preemption非発生時にcurrent task、候補有無、非発生理由を`[preempt]`ログとして出力する。
  - 入力不正時は既存task状態を変更せず、不正条件をログで確認できるようにする。
  - 完了時には発生ケースと非発生ケースのどちらもQEMU serial log上で区別できる。
  - _Requirements: 4.1, 4.2, 4.3, 4.4_
  - _Boundary: Kernel Preemption Smoke_
  - _Depends: 3_

- [x] 5. Dispatcher、Timer、Context Switch境界のDoxygenコメントを更新する
  - dispatcherがcurrent確定を担当し、schedulerはpreemption候補を返すだけであることを説明する。
  - timer tickは契機であり、スケジューリング判断やcontext switch詳細を所有しないことを説明する。
  - task_context/context switch層は切り替え実行を担当し、preemption判断を所有しないことを説明する。
  - 完了時にはpublic/internal interfaceコメントから、今回の実装が完全な割り込み駆動preemptionではないことを確認できる。
  - _Requirements: 3.3, 3.4, 5.3, 6.1, 6.2, 6.3, 6.4_

- [x] 6. 既存smoke flowへpreemption確認を統合する
  - 既存のtask、semaphore、minimal context switch、cooperative runnerのログ順序を壊さない位置にpreemption smokeを配置する。
  - currentより高優先度READY taskがある状態をboot-time verification内で作り、preemption neededを観測する。
  - 必要に応じて同一priorityまたは候補なしの非発生ケースも観測できるようにする。
  - 完了時には既存ログと`[preempt]`ログが同一QEMU runで確認できる。
  - _Requirements: 1.1, 2.1, 2.2, 2.3, 3.1, 4.1, 4.2, 4.3, 5.2_
  - _Boundary: Kernel Preemption Smoke_
  - _Depends: 3, 4_

- [x] 7. BuildとQEMU smokeで回帰確認する
  - `make`が成功し、preemption foundationを含むkernel imageが生成されることを確認する。
  - `make run`で`[timer]`、`[preempt]`、`[scheduler]`、`[dispatcher]`、`[context]`、`[sem]`、`[cooperative]`ログを確認する。
  - schedulerとtimerがdispatcher/context switchを直接呼ばないことを差分または検索で確認する。
  - 完了時にはbuild結果、QEMU serial log、境界確認の結果を実装報告に記載できる。
  - _Requirements: 5.1, 5.2, 5.3, 5.4, 6.4_
