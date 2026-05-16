# Requirements Document

## Introduction
この仕様は、μITRON風RTOSの第6章 6.3「プリエンプション」として、タイマ契機でスケジューラ判断を行うための基盤を追加する。対象ユーザーは、このRTOSを段階的に学習・実装する開発者である。現在は第5章でタスクスタック、レジスタ保存領域、最小コンテキストスイッチを実装済みであり、第6.1でセマフォ基盤、第6.2で明示呼び出し型のタイマ基盤を実装済みである。

この段階では、完全な実時間RTOSではなく、タイマ割り込みまたは周期処理を契機に「現在実行中タスクより高優先度のREADYタスクが存在するか」を判定し、その結果を観測できる状態にする。schedulerは選択と判定のみを担当し、dispatcher/context switch側でcurrent確定と実際の切り替えを扱う。RUNNINGはこれまで通り論理状態として扱い、タイマ契機処理とスケジューリング判断の責務を分離する。

## Boundary Context
- **In scope**: タイマ契機でのプリエンプション判断、現在タスクとREADY候補の優先度比較、切り替え候補の通知、発生・非発生ログ、timer/scheduler/dispatcher/context switchの責務分離、Doxygenコメント更新、boot-time verificationでの観測。
- **Out of scope**: 完全な割り込みネスト制御、SMP、実ITRON互換API、優先度継承、タイムスライス、tickless timer、ユーザモード、高度な割り込みマスク制御、実RTOS実装の参照・コピー・流用。
- **Adjacent expectations**: 既存timer foundationはtickを進められること、schedulerはREADY taskを選べること、dispatcherはcurrent commitを担当すること、task_context/arch層は最小コンテキストスイッチを担当することを前提にする。

## Requirements

### Requirement 1: タイマ契機プリエンプション判断
**Objective:** As a RTOS学習者, I want タイマ契機でプリエンプション要否を判断できる, so that 実時間RTOSへ進む前に切り替え判断の最小境界を観測できる

#### Acceptance Criteria
1. When タイマ契機処理が実行される, the RTOS shall プリエンプション判断を1回実行する。
2. When プリエンプション判断が実行される, the RTOS shall 現在実行中タスクとREADYタスク候補を比較対象として扱う。
3. If 現在実行中タスクが存在しない, then the RTOS shall プリエンプション未発生として観測可能な結果を返す。
4. While タイマ契機処理が実行される, the RTOS shall タイマtick更新とプリエンプション判断を別々の観測可能な処理として保つ。

### Requirement 2: 高優先度READYタスクの検出
**Objective:** As a kernel開発者, I want 現在タスクより高優先度のREADYタスクだけを切り替え候補にできる, so that schedulerの選択責務を小さく保ったまま優先度ベースのプリエンプションを検証できる

#### Acceptance Criteria
1. When READYタスク候補の優先度が現在実行中タスクより高い, the RTOS shall そのREADYタスクをプリエンプション候補として返す。
2. When READYタスク候補の優先度が現在実行中タスクと同じまたは低い, the RTOS shall プリエンプション候補なしとして返す。
3. If READYタスクが存在しない, then the RTOS shall プリエンプション候補なしとして返す。
4. While プリエンプション候補が選択される, the RTOS shall schedulerによる選択だけではcurrent taskを確定しない。

### Requirement 3: DispatcherとContext Switchへの責務分離
**Objective:** As a maintainer, I want scheduler判断とdispatcher/current確定を分離したままプリエンプション基盤を追加できる, so that 第5章までの責務境界を壊さずに段階的な切り替えへ進める

#### Acceptance Criteria
1. When プリエンプション候補が存在する, the RTOS shall dispatcher/context switch側に切り替え対象として渡せる観測可能な結果を生成する。
2. When schedulerがプリエンプション候補を返す, the RTOS shall scheduler内でRUNNING状態またはcurrent pointerを変更しない。
3. When dispatcher/context switch側が切り替えを受け持つ, the RTOS shall current確定をdispatcher側の責務として維持する。
4. While RUNNING状態が使われる, the RTOS shall RUNNINGをCPU実行そのものではなく論理状態として扱う。

### Requirement 4: プリエンプション結果ログ
**Objective:** As a RTOS学習者, I want プリエンプション発生・非発生をQEMU serial logで確認できる, so that タイマ契機判断が正しく動いたかを実装初期段階で検証できる

#### Acceptance Criteria
1. When プリエンプション候補が検出される, the RTOS shall 現在タスク、候補タスク、優先度、判定結果をserial logへ出力する。
2. When プリエンプション候補が検出されない, the RTOS shall 現在タスク、選択候補の有無、判定理由をserial logへ出力する。
3. When QEMU smoke verificationが実行される, the RTOS shall timer tickログとpreemption判断ログを順序付きで確認できるようにする。
4. If プリエンプション判断が入力不正で完了できない, then the RTOS shall 不正条件を示すログを出し、既存のtask状態を変更しない。

### Requirement 5: 段階的実装範囲と既存機能維持
**Objective:** As a maintainer, I want プリエンプション基盤追加後も既存のtimer、semaphore、scheduler、dispatcher、context switchの検証が維持される, so that 学習用RTOSの各章の到達点を壊さずに前進できる

#### Acceptance Criteria
1. When buildが実行される, the RTOS shall 既存機能とpreemption foundationを含むkernel imageをbuildできる。
2. When QEMU smoke verificationが実行される, the RTOS shall 既存のtask、scheduler、dispatcher、context switch、semaphore、timerの観測ログを維持する。
3. The RTOS shall 完全な割り込みネスト制御、SMP、実ITRON互換API、優先度継承、タイムスライス、tickless timer、ユーザモード、高度な割り込みマスク制御をこの機能で提供しない。
4. The RTOS shall 既存RTOS実装ソースコードの参照、コピー、翻訳、流用を行わず、独自の教育用実装として提供される。

### Requirement 6: Doxygenコメントと境界説明
**Objective:** As a maintainer, I want public/internal interfacesにDoxygen形式の境界説明を追加できる, so that タイマ契機判断が何を行い、何をまだ行わないかを後続実装者が誤解しない

#### Acceptance Criteria
1. Where preemption-related public interfaces are introduced, the RTOS shall Doxygen-style commentsで目的、前提、制限、非目標を説明する。
2. Where timer-triggered scheduling helpers are introduced, the RTOS shall タイマ処理とスケジューリング処理の責務分離をコメントで説明する。
3. Where dispatcher/context switch integration is documented, the RTOS shall schedulerがcurrentを確定しないことをコメントで説明する。
4. Where temporary boot-time verification models are used, the RTOS shall それが完全な割り込み駆動プリエンプションではないことをコメントで説明する。
