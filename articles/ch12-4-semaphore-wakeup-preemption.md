---
title: "μITRON風RTOS 12.4: wakeup後のプリエンプション判定を追加する"
emoji: "️"
type: "tech"
topics: ["rtos", "kernel", "c", "qemu", "semaphore"]
published: false
---

## はじめに

この連載では、CodexとSDDを使って、学習目的のμITRON風RTOSを少しずつ作っています。

第12章ではsemaphore APIを題材に、taskの状態遷移を観察できる形で実装しています。
12.1では`wai_sem()`でRUNNING taskをWAITINGへ落としました。
12.2では`sig_sem()`でWAITING taskをREADYへ戻しました。
12.3ではsemaphoreごとのFIFO wait queueを導入し、`wai_sem()`でenqueue、`sig_sem()`でdequeueする形に整理しました。

今回の12.4では、`sig_sem()`でwakeupしたtaskがcurrent taskより高優先度だった場合に、context switchへ進むための判定を追加します。

今回も既存RTOSコードの参照、コピー、流用は行わず、この学習用RTOSの現在の構造に合わせて実装しています。

## 今回のゴール

今回のfeature名は次です。

```text
semaphore-wakeup-preemption
```

tag候補は次です。

```text
v12.4-semaphore-wakeup-preemption
```

今回のゴールは、`sig_sem()`でWAITING taskをREADYへ戻した後にpriority比較を行い、woken taskがcurrent taskより高優先度の場合だけswitchへ進めることです。

このRTOSでは、priority値が小さいtaskを高優先度として扱います。

```text
priority=1  高優先度
priority=5  低優先度
```

今回追加する判断は次のようになります。

```text
sig_sem()
  wait queueからtaskをdequeue
  WAITING -> READY
  current taskとwoken taskのpriorityを比較

  woken priority < current priority
    高優先度wakeupなのでswitchへ進む

  woken priority >= current priority
    同一優先度または低優先度なのでswitchしない
```

同一優先度のtime slice、round-robin、priority順wait queueはまだ扱いません。

## 12.1〜12.3から12.4への接続

12.1では、semaphore countが0のときに`wai_sem()`を呼ぶと、現在RUNNING中のtaskをWAITINGへ落とすところまで作りました。

```text
RUNNING
  -> wai_sem()
  -> WAITING
```

12.2では、`sig_sem()`によってWAITING taskをREADYへ戻しました。

```text
WAITING
  -> sig_sem()
  -> READY
```

12.3では、どのtaskがどのsemaphoreを待っているかをtask table全体から探すのではなく、semaphoreごとのwait queueで管理するようにしました。

```text
wai_sem()
  count == 0
  RUNNING -> WAITING
  semaphore wait queueへenqueue

sig_sem()
  wait queueからdequeue
  WAITING -> READY
```

12.3の時点では、READYへ戻した後に「そのtaskへ切り替えるべきか」はまだ扱っていませんでした。
そのため、12.4ではwakeup後のpreemption判定を追加します。

重要なのは、wait queueの解除順は12.3同様FIFOのまま維持することです。
今回追加するのは、FIFOで取り出した後のpriority比較です。
priority順wait queueへ変更するわけではありません。

## wakeup後preemptionが必要になる理由

たとえば、現在RUNNING中のtaskが低優先度で、`sig_sem()`によって高優先度taskがREADYに戻る場合を考えます。

```text
current task:
  id=7
  priority=5
  state=RUNNING

woken task:
  id=6
  priority=1
  state=READY
```

このRTOSではpriority値が小さいほど高優先度なので、priority=1のwoken taskのほうがpriority=5のcurrent taskより優先されます。

この場合、`sig_sem()`でREADYへ戻しただけで終わると、高優先度taskが実行可能になっているのに低優先度taskが走り続けることになります。
そこで、READY復帰直後にpriorityを比較し、高優先度wakeupであればdispatcherのswitch経路へ接続します。

## `sig_sem()`でREADYへ戻した後のpriority比較

12.4で大事にした順序は次です。

```text
1. sig_sem()が呼ばれる
2. semaphore wait queueからFIFOでtaskをdequeueする
3. dequeueされたtaskをWAITINGからREADYへ戻す
4. READY復帰時にwait_sem_idをクリアする
5. current RUNNING taskとwoken READY taskのpriorityを比較する
6. woken taskが高優先度の場合だけswitchへ進む
```

preemption判定は、WAITING taskをREADYへ戻した後に行います。
READYへ戻る前にswitch判定をすると、dispatcherへ渡すtaskの状態がまだWAITINGのままになってしまいます。

また、READY復帰時には`wait_sem_id`を必ずクリアします。
この点は12.3で整理したwait queueと待ち理由の管理を壊さないための確認点です。

## 高優先度wakeup時のswitch経路

高優先度taskをwakeupした場合は、`sig_sem()`内でpreemptionが必要だと判断し、dispatcherのswitch境界へ進みます。

期待するログは次です。

```text
[sig-sem] called: sem_id=1
[sem-wq] dequeue: sem_id=1 task id=6 name=task_wai_sem_from queue_count=0
[sig-sem] waiting task dequeued: id=6 name=task_wai_sem_from state=WAITING wait_sem_id=1
[task] ready: id=6 name=task_wai_sem_from wait_sem_id=none state=READY
[sig-sem] wakeup: sem_id=1 task id=6 name=task_wai_sem_from WAITING->READY
[sig-sem] preempt check: current id=7 name=task_wai_sem_to prio=5 woken id=6 name=task_wai_sem_from prio=1
[sig-sem] preempt required: reason=wakeup-higher-priority
[sig-sem] switch begin: from id=7 name=task_wai_sem_to to id=6 name=task_wai_sem_from
[dispatcher] switch boundary begin: from id=7 name=task_wai_sem_to to id=6 name=task_wai_sem_from
[dispatcher] state transition: from id=7 name=task_wai_sem_to RUNNING->READY
[dispatcher] state transition: to id=6 name=task_wai_sem_from READY->RUNNING
[dispatcher] switch boundary end: result=0
[sig-sem] switch end: result=0
[sig-sem] completed: result=0 action=wakeup-switch
```

見るポイントは、`[task] ready`の後に`[sig-sem] preempt check`が出ていることです。
つまり、まずWAITING taskをREADYへ戻し、その後にpriority比較をしています。

また、`current prio=5`、`woken prio=1`なので、woken taskのほうが高優先度です。
そのため`preempt required`となり、dispatcherのswitch境界へ進んでいます。

## 同一優先度・低優先度wakeup時はswitchしない

同一優先度または低優先度taskをwakeupした場合は、READYへ戻すだけでswitchしません。

```text
[sig-sem] preempt check: current id=7 name=task_wai_sem_to prio=1 woken id=6 name=task_wai_sem_from prio=1
[sig-sem] preempt not required: reason=same-or-lower-priority
[sig-sem] completed: result=0 action=wakeup-no-switch
```

ここではcurrent taskとwoken taskのpriorityが同じです。
同一優先度のtime sliceはまだ実装していないため、12.4ではswitch対象にしません。

低優先度taskをwakeupした場合も同じ扱いです。
READYには戻しますが、current taskを押しのけて実行する理由はないため、`wakeup-no-switch`で完了します。

## wait queueの解除順はFIFOのまま

12.4ではpreemption判定を追加しましたが、wait queueの解除順は12.3から変えていません。

```text
wai_sem()
  待ちに入った順にenqueue

sig_sem()
  先に待ったtaskからdequeue
```

つまり、priority順wait queueはまだ実装していません。
今回のpriority比較は、FIFOでdequeueされたtaskがREADYに戻った後、そのtaskがcurrent taskより高優先度かどうかを見るためのものです。

これにより、12.3で導入した次の性質を維持します。

- wait queueはFIFOのまま
- 待ちtaskがいる場合、`sig_sem()`ではsemaphore countを増やさない
- 待ちtaskがいない場合だけcountを増やす
- READY復帰時に`wait_sem_id`をクリアする

## timer IRQ preemption経路とは責務を分ける

11.4では、timer IRQで高優先度READY taskを検出したときのpreemption / dispatch pendingログを安定化しました。

一方、今回の`sig_sem()`はtask文脈APIです。
timer IRQ handlerから`sig_sem()`を呼ぶ設計にはしていません。

責務は次のように分けます。

```text
timer IRQ path:
  IRQ中にpreemption候補を検出する
  必要ならdispatch pendingを記録する
  IRQ handler本体から直接dispatcher_switch_to()しない

sig_sem() path:
  task文脈でsemaphoreをsignalする
  wait queueからtaskをwakeupする
  READYへ戻した後にpriority比較する
  高優先度wakeup時だけdispatcherのswitch経路へ進む
```

12.4では、11.4のtimer IRQ preemption / dispatch pending経路を壊さないことも確認対象にしています。
`sig_sem()`、`wai_sem()`、`yield_tsk()`をtimer IRQ handler本体から呼ばない、という境界も維持します。

## 実装概要

実装の中心は`sig_sem()`です。

待ちtaskがいる場合の流れは次です。

```text
sig_sem(sem_id)
  wait queueからtaskをdequeue
  dequeueしたtaskをログに出す
  taskをWAITINGからREADYへ戻す
  wait_sem_idをクリアする
  wakeupログを出す
  current taskとwoken taskのpriorityを比較する

  woken taskのpriorityが小さい
    preempt required
    dispatcher_switch_to(woken task)
    action=wakeup-switch

  それ以外
    preempt not required
    action=wakeup-no-switch
```

待ちtaskがいない場合は、12.3と同じくcountを増やします。

```text
sig_sem(sem_id)
  wait queueが空
  semaphore countを1増やす
  action=count-up
```

ここで注意したのは、待ちtaskがいる場合にcountを増やさないことです。
`sig_sem()`のsignalは、待っているtaskのwakeupに消費されたものとして扱います。

## QEMU serial logで確認すること

QEMU serial logでは、状態遷移が順番に追えることを確認します。

高優先度wakeupでは、次の流れを見ます。

```text
[sig-sem] called: sem_id=1
[sem-wq] dequeue: sem_id=1 task id=6 name=task_wai_sem_from queue_count=0
[sig-sem] waiting task dequeued: id=6 name=task_wai_sem_from state=WAITING wait_sem_id=1
[task] ready: id=6 name=task_wai_sem_from wait_sem_id=none state=READY
[sig-sem] wakeup: sem_id=1 task id=6 name=task_wai_sem_from WAITING->READY
[sig-sem] preempt check: current id=7 name=task_wai_sem_to prio=5 woken id=6 name=task_wai_sem_from prio=1
[sig-sem] preempt required: reason=wakeup-higher-priority
[sig-sem] switch begin: from id=7 name=task_wai_sem_to to id=6 name=task_wai_sem_from
[dispatcher] switch boundary begin: from id=7 name=task_wai_sem_to to id=6 name=task_wai_sem_from
[dispatcher] state transition: from id=7 name=task_wai_sem_to RUNNING->READY
[dispatcher] state transition: to id=6 name=task_wai_sem_from READY->RUNNING
[dispatcher] switch boundary end: result=0
[sig-sem] switch end: result=0
[sig-sem] completed: result=0 action=wakeup-switch
```

同一優先度または低優先度wakeupでは、switchへ進まないことを見ます。

```text
[sig-sem] preempt check: current id=7 name=task_wai_sem_to prio=1 woken id=6 name=task_wai_sem_from prio=1
[sig-sem] preempt not required: reason=same-or-lower-priority
[sig-sem] completed: result=0 action=wakeup-no-switch
```

待ちtaskがいない場合は、countだけが増えることを見ます。

```text
[sig-sem] called: sem_id=1
[sem-wq] empty: sem_id=1
[sig-sem] no waiting task: sem_id=1
[sig-sem] count incremented: sem_id=1 count 0->1
[sig-sem] completed: result=0 action=count-up
```

この3種類を分けて見ると、`sig_sem()`が何をしたのかをログだけで追いやすくなります。

## 検証したこと

検証コマンドは次です。

```powershell
make
make run
make run VALIDATE_TIMER_IRQ_ENTRY=1
```

確認した観点は次です。

- 通常buildが通ること
- `make run`でsemaphore wakeup後preemptionのログが出ること
- `make run VALIDATE_TIMER_IRQ_ENTRY=1`でtimer IRQ entry経路が壊れていないこと
- 12.3のwait queue enqueue/dequeue経路が壊れていないこと
- `sig_sem()`でdequeueされたtaskがREADYへ戻ること
- READY復帰したtaskの`wait_sem_id`がクリアされること
- WAITING taskをREADYへ戻した場合、semaphore countが増えないこと
- wait queueが空の場合のみsemaphore countが1増えること
- woken taskがcurrentより高優先度ならswitchへ進むこと
- woken taskがcurrentと同一優先度ならswitchしないこと
- woken taskがcurrentより低優先度ならswitchしないこと
- 10.4の`yield_tsk()`協調context switch経路が壊れていないこと
- 11.4のtimer IRQ preemption / dispatch pending経路が壊れていないこと

## つまずきやすい点

まず、priorityの大小です。
このRTOSでは、数値が小さいほうが高優先度です。
`prio=1`は`prio=5`より高優先度です。

次に、preemption判定のタイミングです。
12.4では、wakeup後にpriority比較を行います。
つまり、wait queueからdequeueしただけではなく、taskをREADYへ戻してから比較します。

また、同一優先度をswitch対象にしない点も間違えやすいです。
同一優先度time sliceやround-robinはまだ未実装なので、同じpriorityのtaskをwakeupしても`wakeup-no-switch`になります。

さらに、wait queueの順序も変えていません。
priority順wait queueを実装したわけではなく、12.3のFIFO解除順を維持したまま、dequeue後のtaskに対してpriority比較を追加しています。

最後に、timer IRQ経路と混ぜないことです。
`sig_sem()`はtask文脈APIであり、timer IRQ handlerから呼びません。
IRQ中のdispatch pending経路と、task文脈のsemaphore wakeup後switch経路は責務を分けています。

## 失敗時の見方

期待したswitchが起きない場合は、まず`preempt check`ログを見ます。

```text
[sig-sem] preempt check: current ... prio=... woken ... prio=...
```

ここでwoken taskのpriority値がcurrent taskより小さくなっているかを確認します。
数値が同じ、または大きい場合は、12.4ではswitchしません。

READY復帰が怪しい場合は、次のログを見ます。

```text
[task] ready: id=... name=... wait_sem_id=none state=READY
```

`wait_sem_id=none`になっていなければ、WAITINGからREADYへ戻す処理、または待ち理由のクリア処理を疑います。

count制御が怪しい場合は、`count incremented`ログの位置を見ます。
待ちtaskがいるケースでcountが増えているなら、12.3のsemaphore count制御を壊している可能性があります。

timer IRQ系のログが変わった場合は、`sig_sem()`側の変更がIRQ pathへ入り込んでいないかを確認します。
12.4ではtimer IRQ handler本体から`sig_sem()`、`wai_sem()`、`yield_tsk()`、`dispatcher_switch_to()`を直接呼ばない前提です。

## 今回やらないこと

今回は次の項目は実装しません。

- priority順wait queue
- timeout付き`twai_sem`
- sleep/delay queue
- nested interrupt対応
- 同一優先度time slice
- round-robin
- 完全な割り込み復帰フレーム切替
- timer IRQ handlerからの`sig_sem()`呼び出し

今回はあくまで、task文脈の`sig_sem()`でWAITING taskをREADYへ戻した後、高優先度wakeupの場合だけcontext switchへ進むところまでです。

## SDDで進めた流れ

今回もKiro風のSDDで、仕様、設計、タスク、実装、検証の順に進めました。

```text
$kiro-spec-init semaphore-wakeup-preemption
$kiro-spec-requirements semaphore-wakeup-preemption
$kiro-spec-design semaphore-wakeup-preemption -y
$kiro-spec-tasks semaphore-wakeup-preemption -y
$kiro-impl semaphore-wakeup-preemption
$kiro-validate-impl semaphore-wakeup-preemption
```

仕様では、12.3のFIFO wait queue、count制御、`wait_sem_id`クリアを壊さないことを明示しました。
設計では、`sig_sem()`をtask文脈APIとして扱い、timer IRQ preemption / dispatch pending経路とは責務を分けることを確認しました。

## まとめ

12.4では、`sig_sem()`でWAITING taskをREADYへ戻した後にpriority比較を行うようにしました。

woken taskがcurrent taskより高優先度の場合だけ、dispatcherのswitch経路へ進みます。
同一優先度または低優先度のtaskをwakeupした場合は、READYへ戻すだけでswitchしません。

これにより、12.3で導入したFIFO wait queue、semaphore count制御、`wait_sem_id`クリアを維持したまま、semaphore wakeup後のpreemption判定を追加できました。
また、11.4で整理したtimer IRQ preemption / dispatch pending経路とは責務を分けたままにしています。

一方で、priority順wait queue、timeout付き`twai_sem`、同一優先度time slice、round-robinはまだ未実装です。
次回以降も、ログで状態遷移を確認しながら、RTOSらしい実行制御を段階的に足していきます。
