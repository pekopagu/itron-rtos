/*
 * SPDX-License-Identifier: MIT
 */

/**
 * @file task.h
 * @brief 初期タスク管理API定義（第3章3.1、第3章3.2）
 *
 * @details
 * kernel層でタスクを「実行対象」ではなく「管理対象」として登録するための
 * TCB、状態、エラーコード、公開APIを定義する。
 * 第3章3.1ではタスクを実行せず、entry関数呼び出し、コンテキスト作成、
 * スタック初期化は行わない。第3章3.2ではREADY状態のタスクを選ぶ
 * 簡易スケジューラから参照できるよう、状態と優先度を公開契約として整理する。
 *
 * ログ出力は第2章2.3のHAL境界を守り、kernel → HAL → arch(x86_64) → serial → COM1
 * の層構造を維持する。
 */

#ifndef ITRON_RTOS_TASK_H
#define ITRON_RTOS_TASK_H

#include <stdint.h>

#define MAX_TASKS 256

#define TASK_ERR_FULL        (-1)
#define TASK_ERR_INVAL       (-2)
#define TASK_ERR_ID_OVERFLOW (-3)
#define TASK_ERR_NOT_FOUND   (-4)
#define TASK_ERR_BAD_STATE   (-5)

typedef void (*task_entry_t)(void);

/**
 * @enum task_wait_reason_t
 * @brief WAITING taskの待ち理由を観測するための分類。
 *
 * @details
 * 第13章13.1では、同じ `TASK_STATE_WAITING` でもsemaphore待ちとdelay待ちを
 * 区別できるようにする。第13章13.3ではtimeout付きsemaphore待ちも別理由として
 * 観測できるようにする。これは観測用の最小モデルであり、tickごとの減算、
 * tick到達時のREADY復帰、timeout時のqueue削除はまだ実装しない。
 */
typedef enum {
    TASK_WAIT_REASON_NONE = 0,      /**< 待ちなし。READY/RUNNING/DORMANT/UNUSEDで使う。 */
    TASK_WAIT_REASON_SEMAPHORE,     /**< `wai_sem()` によるsemaphore待ち。 */
    TASK_WAIT_REASON_DELAY,         /**< `dly_tsk()` によるdelay待ち。 */
    TASK_WAIT_REASON_SEMAPHORE_TIMEOUT, /**< `twai_sem()` によるtimeout付きsemaphore待ち。 */
} task_wait_reason_t;

/**
 * @struct task_context_t
 * @brief 将来のx86_64最小context switchで使うregister保存領域。
 *
 * @details
 * 第5章5.2では、taskごとに保存領域を準備しTCB上で観測できるようにするだけで、
 * 実CPU register値の保存・復元は行わない。`rsp` は第5章5.1で導入した
 * `stack_top` をもとにした将来の復元候補であり、現段階ではCPUのRSPへロードしない。
 *
 * callee-saved register相当の値を明示的に並べることで、第5章5.3以降の
 * 最小context switch実装時に、どの値を保存領域として扱うかを追跡しやすくする。
 * ただし、この構造体の存在はstack switch、assembler dispatch、interrupt、
 * timer、preemption、task stack上でのentry実行を意味しない。
 */
typedef struct {
    /**
     * @brief 将来の最小context switchで復元対象にするstack pointer保存欄。
     *
     * @details
     * 第8章8.1のtimer IRQ tick接続では、この欄に割り込み時の実CPU RSPを保存しない。
     * timer IRQ handlerは `timer_tick()` とEOIだけを行い、task_context_tの保存・復元や
     * interrupt-time context switchには進まない。
     */
    unsigned long rsp; /**< 将来のstack pointer復元候補。第5章5.2ではCPU RSPへロードしない。 */
    unsigned long rbp; /**< 将来保存対象にするbase pointer領域。現段階では実register値ではない。 */
    unsigned long rbx; /**< 将来保存対象にする汎用register領域。現段階では実register値ではない。 */
    unsigned long r12; /**< 将来保存対象にするcallee-saved register領域。現段階では実register値ではない。 */
    unsigned long r13; /**< 将来保存対象にするcallee-saved register領域。現段階では実register値ではない。 */
    unsigned long r14; /**< 将来保存対象にするcallee-saved register領域。現段階では実register値ではない。 */
    unsigned long r15; /**< 将来保存対象にするcallee-saved register領域。現段階では実register値ではない。 */
} task_context_t;

/**
 * @enum task_state_t
 * @brief タスク管理テーブル上のタスク状態。
 *
 * @details
 * μITRON風RTOSの学習用として、将来のタスクライフサイクルを表す状態を定義する。
 * スケジューラが選択対象にするのはREADYのみであり、RUNNINGはdispatcherで
 * currentとして確定された論理状態として扱う。
 *
 * RUNNINGをCPU実行中と直結させないのは、状態確定と実際のタスク実行を分離し、
 * 第4章4.1のboot-time verification modelや第5章のコンテキスト管理へ
 * 段階的に接続しやすくするためである。
 * 第4章4.1ではRUNNINGのcurrent taskだけがentry直接呼び出し対象になるが、
 * 独立stack実行、CPU context復元、継続的なCPU実行中状態は意味しない。
 * 第4章4.2ではentry return後もRUNNINGの意味を変更せず、正式なtask終了状態や
 * RUNNINGからDORMANT/READY/WAITINGへの遷移は導入しない。
 * 第4章4.3ではboot-time verification modelとして、entry returnを
 * cooperative return eventとして観測し、RUNNINGからREADYへ戻すことで
 * 再びscheduler候補にする。ただし、これはtask restartではなく、
 * CPU実行中状態からの退避や正式なyield APIでもない。
 * WAITINGへの待ち入り、コンテキストスイッチ、割り込み連動は将来拡張として残す。
 * 第9章9.4では、context switch smoke上のentry returnを「READY復帰」ではなく
 * その起動分の実行完了として扱い、task_context層の観測点からDORMANTへ最終化する。
 */
typedef enum {
    TASK_STATE_UNUSED = 0, /**< 未使用スロット。空き判定はこの値だけで行う内部管理状態。 */
    TASK_STATE_DORMANT,    /**< 生成済みだが未開始、または第9章9.4でentry return後の起動分完了として最終化された状態。 */
    TASK_STATE_READY,      /**< 実行可能な状態。第3章3.2のscheduler_select_next()が唯一の選択対象にする。 */
    TASK_STATE_RUNNING,    /**< currentとしてcommit済みの論理状態。4.1/4.2/4.3ではentry直接呼び出しとreturn観測の対象だが、正式終了、独立stack実行、CPU context復元は意味しない。 */
    TASK_STATE_WAITING,    /**< 待ち状態。第3章3.2では待ちキューや待ち解除を実装せず、将来拡張用に残す。 */
} task_state_t;

/**
 * @struct tcb_t
 * @brief Task Control Block。
 *
 * @details
 * タスクを実行するためではなく、kernel内部で登録済みタスクを管理・表示し、
 * 簡易スケジューラがREADYタスクを選択し、dispatcherが現在タスクを確定するための
 * 最小情報を保持する。
 *
 * entryとstack情報をこの段階から持たせているのは、第4章以降で入口関数実行や
 * スタック管理へ接続するときにTCBの役割を作り直さずに済むようにするためである。
 * 第4章4.1では、current taskのentryだけを通常のC関数呼び出しで直接呼ぶ。
 * 第4章4.2では、そのentryがreturnしてもTCB状態を終了状態へ変更しない。
 * 第4章4.3では、cooperative return後にRUNNINGからREADYへ戻すことで
 * 再びscheduler候補にできるが、これはtask restartではない。
 * この直接呼び出しは一時的なboot-time verification modelであり、
 * 第5章ではcontext-switch-based executionへ置き換える前提である。
 * ただし第3章3.3では、これらのフィールドは観測用・将来接続用であり、
 * 入口関数呼び出し、スタック切り替え、コンテキスト作成には使わない。
 */
typedef struct {
    int id;                    /**< 登録後に割り当てられるタスクID。0は未使用または無効IDとして扱い、将来のAPI境界でも不正ID判定に使える。 */
    const char *name;          /**< dump時に識別しやすくするためのタスク名。ログで状態遷移を追うために保持する。 */
    task_entry_t entry;        /**< 将来の実行開始で使う入口関数。4.1/4.2ではcurrent/RUNNING確認後に直接呼びreturnを観測するが、将来のcontext switch置換点として保持する。 */
    int priority;              /**< scheduler選択用優先度。数値が小さいほど高優先度として扱い、将来の優先度制御の基礎にする。 */
    task_state_t state;        /**< 空き判定、scheduler候補判定、dispatcher確定判定の根拠。状態遷移を一箇所で観測できるようTCBに持たせる。 */
    int wait_sem_id;           /**< セマフォ待ち対象ID。0は待ちなし。第6章6.1では観測用で、timeoutやwait queueとは接続しない。 */
    task_wait_reason_t wait_reason; /**< WAITINGの理由。semaphore待ちとdelay待ちを区別する観測値。 */
    uint32_t delay_ticks_remaining; /**< delay待ちの残tick観測値。13.1では減算やREADY復帰は行わない。 */
    void *stack_base;          /**< 将来のスタック管理に渡す基底アドレス。現段階では保持と表示のみで、切り替えには使わない。 */
    unsigned long stack_size;  /**< stack_baseが指す領域のサイズ。将来のスタック検証に使えるよう、現段階からTCBに含める。 */
    /**
     * @brief 将来の初期stack pointer候補。
     *
     * @details
     * x86_64のstackが下方向へ伸びる前提で、現段階では
     * `stack_base + stack_size` に対応する上端アドレスを保持する。
     * 第5章5.1ではstack foundationの観測用metadataであり、CPUのRSPへ
     * ロードしない。task entryも引き続きboot-timeのC stack上で直接呼び出す。
     */
    void *stack_top;
    /**
     * @brief taskごとのCPU register保存領域。
     *
     * @details
     * 第5章5.2では `context.rsp` に `stack_top` と同じ将来復元候補を保持し、
     * その他のregister領域を0で初期化する。これは保存領域の準備と観測であり、
     * 実際のCPU register保存・復元、CPU RSPへのロード、stack switchは行わない。
     */
    task_context_t context;
} tcb_t;

/**
 * @brief 静的タスクテーブルとID採番状態を初期化する。
 *
 * @details
 * 全スロットをUNUSEDにし、次のタスクIDを1へ戻す。
 * 再起動直後と同じ管理状態にするためのAPIである。
 *
 * @param なし。
 * @return なし。
 * @note タスク実行、コンテキスト作成、スタック初期化、スケジューラ選択は行わない。
 */
void task_init(void);

/**
 * @brief タスク情報を静的テーブルへ登録する。
 *
 * @details
 * 引数検証、空きスロット探索、ID採番、TCB設定、登録ログ出力を行う。
 * 登録直後の状態はREADYであり、第3章3.2の簡易スケジューラの選択候補になる。
 * ただし、この関数はentry関数を呼ばず、実行もコンテキスト作成も行わない。
 *
 * @param name タスク名。NULLは不正。
 * @param entry タスク入口関数。NULLは不正だが、このAPIでは呼び出さない。
 * @param priority 優先度。数値が小さいほど高優先度として第3章3.2 schedulerが扱う。
 * @param stack_base スタック領域の基底アドレス。NULLは不正だが初期化しない。
 * @param stack_size スタック領域のサイズ。0は不正。
 * @return 成功時は1以上のタスクID。失敗時はTASK_ERR_*。
 * @note task_start、コンテキストスイッチ、スタック初期化は未実装である。
 */
int task_register(
    const char *name,
    task_entry_t entry,
    int priority,
    void *stack_base,
    unsigned long stack_size
);

/**
 * @brief 登録済みタスクの一覧をHAL consoleへ出力する。
 *
 * @details
 * UNUSEDスロットを除外し、TCBの主要フィールドをdumpする。
 * HAL境界を守るため、出力はHAL console API経由で行われる。
 *
 * @param なし。
 * @return なし。
 * @note dumpは観測用であり、タスク状態を変更しない。
 */
void task_dump(void);

/**
 * @brief schedulerが走査できるタスクスロット数を返す。
 *
 * @details
 * 第3章3.2ではschedulerがtask_tableを直接extern参照しないよう、この読み取りAPIを使う。
 * 戻り値は固定長テーブルの上限であるMAX_TASKSであり、動的メモリや可変長配列は使わない。
 *
 * @param なし。
 * @return 走査可能なタスクスロット数。
 * @note このAPIはtask_tableの内容を変更しない。
 */
int task_get_count(void);

/**
 * @brief 指定indexのTCBを読み取り専用で参照する。
 *
 * @details
 * schedulerはこのAPIを通じてTCBを確認し、`state == TASK_STATE_READY` のタスクだけを
 * 選択候補にする。task_tableの所有権はtask.cに残し、呼び出し側は返されたTCBを変更しない。
 *
 * @param index 参照するtask_table上のindex。
 * @return 範囲内ならTCBへの読み取り専用ポインタ。範囲外ならNULL。
 * @note task_tableをextern公開しないための境界APIであり、タスク実行は行わない。
 */
const tcb_t *task_get_by_index(int index);

/**
 * @brief 登録済みTCBをtask idで読み取り専用参照する。
 *
 * @details
 * semaphore moduleがログ用のtask名と状態を確認するための境界APIである。
 * 返されたTCBを呼び出し側が変更してはならない。
 *
 * @param task_id 登録済みtask id。0以下は不正。
 * @return 見つかったTCBへの読み取り専用ポインタ。見つからない場合はNULL。
 */
const tcb_t *task_get_by_id(int task_id);

/**
 * @brief 登録済みTCBをtask idで更新用に取得する。
 *
 * @details
 * 第5章5.3の最小context switch smokeでは、task登録時に作った
 * `task_context_t` に初回stack frameを準備する必要がある。schedulerは引き続き
 * 読み取り専用 accessor だけを使い、この更新用 accessor はkernel/task-context
 * 境界からのみ利用する。entry呼び出し、scheduler選択、dispatcher commit、
 * 割り込み、timer、preemptionは行わない。
 *
 * @param task_id 登録済みtask id。0以下は不正。
 * @return 更新対象TCBへのポインタ。見つからない場合はNULL。
 */
tcb_t *task_get_mutable_by_id(int task_id);

/**
 * @brief 登録済みREADYタスクを論理的なRUNNING状態へ変更する。
 *
 * @details
 * dispatcherが使用するタスク管理の状態変更APIである。task_tableの所有権はtask.cに残し、
 * 現在状態がTASK_STATE_READYである有効な登録済みタスクだけを変更する。
 * UNUSEDスロットとREADY以外のタスクは失敗として扱う。
 *
 * この関数が行うのはREADYからRUNNINGへの論理状態遷移だけである。
 * タスク入口関数は呼び出さず、コンテキストスイッチ、スタック切り替え、
 * レジスタ保存・復元も行わない。
 *
 * @param task_id 登録済みタスクID。0以下は不正。
 * @return 成功時は0、失敗時は負のTASK_ERR_*値。
 */
int task_mark_running(int task_id);

/**
 * @brief RUNNING taskをREADY候補へ戻す。
 *
 * @details
 * 第4章4.3のboot-time verification modelで使用する状態変更APIである。
 * cooperative return eventを観測した後、currentとして採用されていた
 * RUNNING taskをREADYへ戻し、再びschedulerの選択候補にする。
 * 第10章10.2以降では `yield_tsk()` がRUNNING current taskをREADYへ戻すためにも
 * このAPIを使う。第10章10.4では、READY化成功後にAPI層がschedulerで
 * 次READY候補を選びdispatcher境界へ進むが、この関数はその選択や切替を担当しない。
 *
 * この遷移はtask restartではない。この関数はREADY化までに限定し、
 * 正式なtask終了、DORMANT遷移、次task選択、dispatcher switch、
 * コンテキストスイッチ、スタック切り替え、レジスタ保存・復元は行わない。
 *
 * @param task_id 登録済みタスクID。0以下は不正。
 * @return 成功時は0、失敗時は負のTASK_ERR_*値。
 */
int task_mark_ready_from_running(int task_id);

/**
 * @brief entry returnしたtaskをDORMANTへ最終化する。
 *
 * @details
 * 第9章9.4のtask_context層から呼ばれる状態変更APIである。
 * task entry関数がreturnした時点で、その起動分は完了したものとして扱い、
 * 対象taskをREADY候補へ戻さずDORMANTへ遷移させる。
 *
 * RUNNINGを許すのは、現在実行中としてentry returnしたtaskを直接完了させるためである。
 * READYを許すのは、第9章9.3のdispatcher_switch_to()がtask-to-task smoke開始前に
 * from taskをRUNNINGからREADYへ戻すためであり、READY復帰を継続する意味ではない。
 *
 * この関数はtask entry呼び出し、dispatcher current更新、scheduler選択、
 * dispatch pending消費、interrupt exit接続、context switchを行わない。
 *
 * @param task_id 登録済みタスクID。0以下は不正。
 * @return 成功時は0、失敗時は負のTASK_ERR_*値。
 */
int task_mark_dormant_from_entry_return(int task_id);

/**
 * @brief 指定taskをセマフォ待ちのWAITING状態へ変更する。
 *
 * @details
 * 第6章6.1の観測用最小WAITING遷移である。wait queue、timeout、timer、
 * preemption、interruptとは接続しない。セマフォmoduleから呼ばれるが、
 * TCBの状態とwait_sem_idの所有権はtask moduleに残す。
 *
 * @param task_id 対象task id。
 * @param sem_id 待ち対象セマフォID。
 * @return 成功時は0、失敗時は負のTASK_ERR_*値。
 */
int task_mark_waiting_on_sem(int task_id, int sem_id);

/**
 * @brief 指定taskをdelay待ちのWAITING状態へ遷移させる。
 *
 * @details
 * 第13章13.1の `dly_tsk()` 用の最小状態遷移APIである。対象taskはRUNNINGで
 * ある必要があり、遷移後は `wait_reason=TASK_WAIT_REASON_DELAY`、
 * `wait_sem_id=0`、`delay_ticks_remaining=delay_ticks` として観測できる。
 *
 * このAPIはsleep/delay queueを作らず、tickごとの減算、tick到達時READY復帰、
 * timer IRQ handlerからの呼び出しも扱わない。scheduler選択やdispatcher switchは
 * 呼び出し元の `dly_tsk()` が担当する。
 *
 * @param task_id delay待ちへ入れるtask ID。
 * @param delay_ticks 観測用に保持するdelay tick数。0は不正。
 * @return 成功時は0、失敗時はTASK_ERR_*。
 */
int task_mark_waiting_on_delay(int task_id, uint32_t delay_ticks);

/**
 * @brief `twai_sem()` 用にRUNNING taskをtimeout付きsemaphore WAITINGへ遷移させる。
 *
 * @details
 * 第13章13.3のtimeout付きsemaphore待ち入口で使う状態遷移である。
 * 通常の `wai_sem()` 待ちとは異なる `TASK_WAIT_REASON_SEMAPHORE_TIMEOUT` を設定し、
 * `wait_sem_id` には待ち対象semaphore IDを、`delay_ticks_remaining` には
 * timeout観測用tick数を保持する。
 *
 * この関数はsemaphore wait queue、delay queue、tickごとのdecrement、timeout到達時READY復帰、
 * scheduler選択、dispatcher switchを行わない。queue登録とswitchは呼び出し元の
 * `twai_sem()` が既存境界へ委譲する。
 *
 * @param task_id timeout付きsemaphore待ちへ入るtask ID。
 * @param sem_id 待ち対象semaphore ID。0以下は不正。
 * @param timeout_ticks timeout観測用に保持するtick数。0は不正。
 * @return 成功時は0、失敗時はTASK_ERR_*。
 */
int task_mark_waiting_on_sem_timeout(int task_id, int sem_id, uint32_t timeout_ticks);

/**
 * @brief 指定セマフォを待つtaskを読み取り専用で1件探す。
 *
 * @details
 * 第12章12.2のsig_semログでwakeup対象を先に観測するための読み取りAPIである。
 * task状態は変更しない。wait queue未導入のため、探索順はtask table順であり、
 * FIFO順や優先度順を保証しない。
 *
 * @param sem_id 対象セマフォID。
 * @return 見つかったWAITING task。対象なしや不正入力はNULL。
 */
const tcb_t *task_find_waiting_on_sem(int sem_id);

/**
 * @brief 指定セマフォを待つtaskを1件READYへ戻す。
 *
 * @details
 * 第12章12.2ではwait queueを持たないため、task tableを走査して最初に見つかった
 * WAITING taskを1件だけREADYへ戻し、`wait_sem_id` を未待ち状態へ戻す。
 * FIFO順や優先度順、wakeup後preemption、timeout、同一優先度time slice、
 * round-robinは保証しない。
 *
 * @param sem_id 対象セマフォID。
 * @param woken_task_id wakeupしたtask idの格納先。NULLも許容する。
 * @return wakeup成功時は0。対象なしや不正入力は負のTASK_ERR_*値。
 */
int task_wake_one_waiting_on_sem(int sem_id, int *woken_task_id);

/**
 * @brief 指定taskを指定semaphore待ちからREADYへ戻す。
 *
 * @details
 * 第12章12.3のsemaphore wait queue経由wakeupで使う。wakeup対象は
 * semaphore queueからdequeue済みのtask idに限定し、task table全体から
 * 別のWAITING taskを探さない。READY復帰時に `wait_sem_id` を0へ戻す。
 * priority順wait queue、wakeup後preemption、timeout、time slice、round-robinは扱わない。
 *
 * @param task_id READYへ戻すtask ID。
 * @param sem_id taskが待っているべきsemaphore ID。
 * @return 成功時は0。失敗時はTASK_ERR_*。
 */
int task_wake_waiting_on_sem_by_id(int task_id, int sem_id);

#endif
