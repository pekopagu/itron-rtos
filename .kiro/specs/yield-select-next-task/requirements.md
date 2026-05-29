# Requirements Document

## Introduction

第10章10.3では、10.2で `yield_tsk()` がRUNNING current taskをREADYへ戻せるようになった後段として、READY化直後にschedulerで次のREADY task候補を選び、その候補をログで観測可能にする。対象は学習用μITRON風RTOSの開発者であり、現状は `yield_tsk()` がREADY化後に `scheduler_select_next()` を呼ばず、次に実行可能なtask候補を確認できない。今回の変更では、9.4のentry return -> DORMANT確定と10.1/10.2の「`yield_tsk()` はentry returnではない」という設計を維持したまま、候補選択までを追加する。

## Boundary Context

- **In scope**: RUNNING current taskのREADY化維持、READY化後の `scheduler_select_next()` 呼び出し、選択候補のid/name/prio/stateログ、候補なしログ、deferredログ更新、DORMANT current reject維持、README/Doxygen/docs log/spec更新。
- **Out of scope**: `dispatcher_switch_to()` 呼び出し、`task_context_switch_to_task_pair()` 接続、実context switch、dispatcher currentの次taskへのcommit、dispatch pending消費、interrupt exit boundaryやtimer IRQからのyield接続、preemptive context switch、time slice、semaphore wakeup連携、sleep/delay queue、他μITRON風API完成、nested interrupt、APIC/IOAPIC/LAPIC、SMP。
- **Adjacent expectations**: 9.1 task_b -> task_c smoke、9.2 dispatcher switch boundary、9.3 RUNNING/READY遷移、9.4 entry return -> DORMANT、10.1/10.2のyield reject/READY化ログ、timer IRQ pathは維持する。

## Requirements

### Requirement 1: READY化後の次task候補選択
**Objective:** RTOS学習者として、`yield_tsk()` がRUNNING current taskをREADYへ戻した後、schedulerが次のREADY task候補を選ぶところまでをログで確認したい。

#### Acceptance Criteria

1. When `yield_tsk()` がcurrent task存在中かつ `TASK_STATE_RUNNING` の状態で呼ばれたとき, the kernel shall current taskのid、name、stateを `[yield] called` ログへ出力する。
2. When `yield_tsk()` がRUNNING current taskに対して呼ばれたとき, the kernel shall task管理層のRUNNING->READY遷移APIを通じて対象taskをREADYへ戻す。
3. When RUNNING->READY遷移が成功したとき, the kernel shall `[yield] state transition: current ... RUNNING->READY` をログへ出力する。
4. When RUNNING->READY遷移が成功したとき, the kernel shall `scheduler_select_next()` でREADY task候補を1件選択する。
5. When schedulerが次task候補を選択したとき, the kernel shall `[yield] next selected: id=... name=... prio=... state=READY` をログへ出力する。

### Requirement 2: 候補なしと戻り値の観測
**Objective:** RTOS学習者として、READY候補が存在しない場合でも、`yield_tsk()` の到達点と戻り値を明確に確認したい。

#### Acceptance Criteria

1. If READY化後にschedulerが次task候補を選択できない場合, the kernel shall `[yield] no next task: reason=no-ready-task` をログへ出力する。
2. When READY化後のscheduler選択が完了したとき, the kernel shall `[yield] deferred: reason=dispatcher-switch-not-connected-yet` をログへ出力する。
3. When RUNNING current taskのREADY化が成功したとき, the kernel shall 次候補の有無にかかわらず `YIELD_TSK_OK` を返す。

### Requirement 3: 不正current状態のreject維持
**Objective:** RTOS学習者として、`yield_tsk()` がentry returnやDORMANT taskをREADYへ戻さず、RUNNING currentだけを対象にすることを確認したい。

#### Acceptance Criteria

1. If current taskが存在しない場合, the kernel shall 従来どおりinvalid-current-stateとしてrejectし、負の戻り値を返す。
2. If current taskがRUNNINGではない場合, the kernel shall current taskのid、name、stateをログへ出力したうえでinvalid-current-stateとしてrejectする。
3. If current taskがDORMANTの場合, the kernel shall taskをREADYへ戻さない。
4. The kernel shall `yield_tsk()` をentry returnの代替として扱わない。

### Requirement 4: 10.3の非接続境界維持
**Objective:** RTOS学習者として、10.3が協調スケジューリング完成ではなく、次候補選択までの観測回であることを確認したい。

#### Acceptance Criteria

1. The kernel shall `yield_tsk()` 内で `dispatcher_switch_to()` を呼ばない。
2. The kernel shall `yield_tsk()` 内で `task_context_switch_to_task_pair()` を呼ばない。
3. The kernel shall `yield_tsk()` 内でdispatcher currentを次taskへcommitしない。
4. The kernel shall `yield_tsk()` 内でdispatch pendingを消費しない。
5. The kernel shall timer IRQ handlerから `dispatcher_switch_to()` を呼ばず、dispatch pendingも消費しない。

### Requirement 5: 既存smokeと文書の維持
**Objective:** RTOS学習者として、10.3の追加後も既存9.x/10.xの観測点と文書が一貫していることを確認したい。

#### Acceptance Criteria

1. When 通常smokeを実行したとき, the kernel shall 9.1のtask_b -> task_c context switch smokeログを維持する。
2. When 通常smokeを実行したとき, the kernel shall 9.2のdispatcher switch boundaryログを維持する。
3. When 通常smokeを実行したとき, the kernel shall 9.3のRUNNING/READY状態遷移ログを維持する。
4. When 通常smokeを実行したとき, the kernel shall 9.4のentry return -> DORMANTログを維持する。
5. The repository shall README、Doxygenコメント、`docs/logs/qemu-serial.log` に10.3の到達点と未実装範囲を記載する。
6. The spec directory shall `.kiro/specs/yield-select-next-task/` に最終的に `requirements.md`、`design.md`、`tasks.md` の3ファイルだけを保持する。
