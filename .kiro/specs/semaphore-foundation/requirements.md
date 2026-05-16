# Requirements Document

## Introduction
この仕様は、μITRON風RTOSの第6章6.1として、学習目的のセマフォ管理の土台を追加する。目的は、セマフォの初期化、取得、返却、WAITING遷移、最小wakeup、task/semaphore dumpをQEMU serial logで観測できるようにすることである。まだtimer、preemption、interrupt、timeout付き待ち、本格的なwait queueとは接続しない。

## Boundary Context
- **In scope**: 静的セマフォの観測可能な作成・操作、count更新、WAITING/READY遷移、wait_sem_id表示、semaphore dump、既存smoke pathとの共存
- **Out of scope**: timer、timeout付き待ち、pol_sem、twai_sem、preemption、interrupt、timer interruptからのtask切り替え、優先度付き/FIFO wait queue、本格ready queue、継続scheduler loop、deadlock検出、priority inheritance、mutex、event flag、μITRON完全互換API
- **Adjacent expectations**: schedulerはREADY task選択、dispatcherはcurrent commit、context switch/arch層はregister保存・復元primitiveの責務を維持し、セマフォ待ちの内部責務を持たない

## Requirements

### Requirement 1: セマフォの作成と初期状態の観測
**Objective:** As a RTOS学習者, I want セマフォを静的に作成して初期状態をログで確認できる, so that 同期機構の土台を小さく検証できる

#### Acceptance Criteria
1. When セマフォ初期化が実行される, the RTOS shall セマフォのid、name、count、max_countを保持する
2. When セマフォ初期化が成功する, the RTOS shall 初期化されたセマフォのid、name、count、max_countをserial logへ出力する
3. If セマフォ初期値が最大値を超える, the RTOS shall セマフォを有効な初期状態として扱わず、観測可能な失敗結果を返す
4. The RTOS shall セマフォ情報を静的に管理し、実行時の動的メモリ確保を要求しない

### Requirement 2: wai_sem相当操作によるcount減少
**Objective:** As a RTOS学習者, I want countが残っているセマフォ取得でcountが減ることを確認できる, so that セマフォ取得の基本動作を検証できる

#### Acceptance Criteria
1. When countが1以上のセマフォに対してwai_sem相当操作が実行される, the RTOS shall 操作結果を成功として扱い、countを1減らす
2. When wai_sem相当操作が成功する, the RTOS shall 対象taskのid、name、セマフォid、セマフォname、count_before、count_after、resultをserial logへ出力する
3. If 存在しないセマフォに対してwai_sem相当操作が実行される, the RTOS shall 観測可能な失敗結果を返し、既存のtask状態を変更しない

### Requirement 3: count不足時のWAITING遷移
**Objective:** As a RTOS学習者, I want countが0のセマフォ取得でtaskがWAITINGになることを確認できる, so that 待ち状態の最小モデルを観測できる

#### Acceptance Criteria
1. When countが0のセマフォに対してwai_sem相当操作が実行される, the RTOS shall 対象taskをWAITING状態にする
2. When 対象taskがセマフォ待ちでWAITING状態になる, the RTOS shall 対象taskのwait_sem_idに待ち対象のセマフォidを記録する
3. When 対象taskがWAITING状態になる, the RTOS shall task id、task name、wait_sem_id、stateをserial logへ出力する
4. While WAITING状態が観測用の最小実装である, the RTOS shall timeout、preemption、interrupt連携が未実装であることを挙動上の前提として維持する

### Requirement 4: sig_sem相当操作によるcount増加と最小wakeup
**Objective:** As a RTOS学習者, I want セマフォ返却でcount増加またはWAITING taskの復帰を確認できる, so that 将来のwait queue設計前にwakeup境界を検証できる

#### Acceptance Criteria
1. When WAITING taskが存在しないセマフォに対してsig_sem相当操作が実行される, the RTOS shall max_countを超えない範囲でcountを1増やす
2. When 対象セマフォを待つWAITING taskが存在する状態でsig_sem相当操作が実行される, the RTOS shall 最小実装として1つのWAITING taskをREADY状態へ戻す
3. When sig_sem相当操作がWAITING taskをREADYへ戻す, the RTOS shall 対象taskのwait_sem_idを0に戻す
4. When sig_sem相当操作が実行される, the RTOS shall セマフォid、セマフォname、count_before、結果、wakeup対象task情報またはcount_afterをserial logへ出力する
5. If sig_sem相当操作でcountがmax_countを超える, the RTOS shall 観測可能な失敗結果を返し、countを変更しない

### Requirement 5: task/semaphore dumpによる状態確認
**Objective:** As a RTOS学習者, I want taskとsemaphoreの状態をdumpで確認できる, so that WAITING遷移とcount変化を一連のログとして検証できる

#### Acceptance Criteria
1. When task dumpが実行される, the RTOS shall 各taskのid、name、state、wait_sem_idをserial logへ出力する
2. When semaphore dumpが実行される, the RTOS shall dump start/endと各セマフォのid、name、count、max_countをserial logへ出力する
3. When 第6章6.1のsmoke sequenceが実行される, the RTOS shall セマフォ初期化、wai_sem成功、wai_sem待ち、sig_sem wakeup、task dump、semaphore dumpを観測可能な順序で出力する

### Requirement 6: 既存実行経路と責務分離の維持
**Objective:** As a RTOS学習者, I want セマフォ追加後も既存の最小context switch経路と責務分離が保たれる, so that 第5章までの検証結果を壊さずに同期機構へ進める

#### Acceptance Criteria
1. When セマフォ機能が追加された後にbuildが実行される, the RTOS shall 既存のbuildを成功させる
2. When QEMU smoke runが実行される, the RTOS shall 既存のminimal-context-switch smoke pathを完了できる
3. While cooperative runnerが残っている, the RTOS shall 既存のcooperative runnerで観測しているログ順序を壊さない
4. The RTOS shall scheduler、dispatcher、context switch、arch層にセマフォ管理の責務を追加しない
5. The RTOS shall 既存RTOS実装ソースコードの参照、コピー、流用なしに学習目的の独自実装として提供される
