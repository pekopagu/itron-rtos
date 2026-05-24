/*
 * SPDX-License-Identifier: MIT
 */

/**
 * @file task.c
 * @brief 初期タスク管理実装（第3章3.1、第3章3.2）
 *
 * @details
 * 最大256件の静的タスクテーブルを持ち、タスク情報の初期化、登録、dumpを提供する。
 * タスクは実行せず、entry関数呼び出し、コンテキスト作成、スタック初期化、
 * 割り込み、タイマは追加しない。第3章3.2ではschedulerが読み取りアクセサ経由で
 * READYタスクを選べるようにする。
 * 第4章4.3では、cooperative return eventを観測したRUNNING taskを
 * READYへ戻す最小状態遷移を提供する。この遷移は再度scheduler候補に
 * するためだけのcooperative re-candidacyであり、task restartではない。
 *
 * このファイルはkernel層に属するが、ログ出力は第2章2.3のHAL console APIだけを使う。
 * 依存方向は kernel → HAL → arch(x86_64) → serial → COM1 である。
 */

#include <stddef.h>

#include "hal/console.h"
#include "task.h"

#define TASK_ID_MAX 2147483647

static tcb_t task_table[MAX_TASKS];
static int next_task_id = 1;

/**
 * @brief 符号なし整数をHAL consoleへ10進出力する。
 *
 * @details
 * printfを導入せず、タスク管理ログに必要な最小限の数値出力だけを行う。
 *
 * @param value 出力する符号なし整数。
 * @return なし。
 * @note ロジックは表示補助に限定し、タスク管理状態は変更しない。
 */
static void task_write_uint(unsigned long value)
{
    char buffer[20];
    int index = 0;

    if (value == 0) {
        hal_console_putc('0');
        return;
    }

    while (value > 0) {
        buffer[index++] = (char)('0' + (value % 10));
        value /= 10;
    }

    while (index > 0) {
        hal_console_putc(buffer[--index]);
    }
}

/**
 * @brief 符号付き整数をHAL consoleへ10進出力する。
 *
 * @details
 * task id、priority、負のエラーコードを読みやすく表示するための内部補助関数。
 *
 * @param value 出力する符号付き整数。
 * @return なし。
 * @note printfを使わず、HAL console境界内で出力する。
 */
static void task_write_int(int value)
{
    if (value < 0) {
        hal_console_putc('-');
        task_write_uint((unsigned long)(-value));
        return;
    }

    task_write_uint((unsigned long)value);
}

/**
 * @brief 整数値をアドレス確認用の16進表記で出力する。
 *
 * @details
 * entryアドレスとstack_base/stack_topをQEMUログで確認しやすくする。
 *
 * @param value 出力する値。
 * @return なし。
 * @note アドレスを表示するだけで、参照や初期化は行わない。
 */
static void task_write_hex(unsigned long value)
{
    static const char digits[] = "0123456789abcdef";
    int shift;
    int started = 0;

    hal_console_write("0x");

    if (value == 0) {
        hal_console_putc('0');
        return;
    }

    for (shift = (int)(sizeof(unsigned long) * 8) - 4; shift >= 0; shift -= 4) {
        unsigned long nibble = (value >> shift) & 0xFUL;

        if (nibble != 0 || started) {
            hal_console_putc(digits[nibble]);
            started = 1;
        }
    }
}

/**
 * @brief 将来の初期stack pointer候補を算出する。
 *
 * @details
 * x86_64のstackは下方向へ伸びる前提なので、stack領域の上端である
 * `stack_base + stack_size` を候補値として扱う。第5章5.1ではこの値を
 * TCBに保持してログへ出すだけで、CPUのRSPへロードしない。
 *
 * @param stack_base taskに割り当てられたstack領域の基底アドレス。
 * @param stack_size stack領域のサイズ。
 * @return 将来の初期stack pointer候補。
 * @note 16-byte alignmentは将来のcontext switch実装で再確認する。
 */
static void *task_calculate_stack_top(void *stack_base, unsigned long stack_size)
{
    return (void *)((unsigned char *)stack_base + stack_size);
}

/**
 * @brief task context保存領域を未使用状態へ初期化する。
 *
 * @details
 * task_table全体の初期化で使う。すべてのregister保存領域を0へ戻し、
 * 前回起動や前回登録の観測値が残らないようにする。
 *
 * @param context 初期化対象のcontext保存領域。
 * @return なし。
 * @note 実CPU registerを読み取らず、保存・復元も行わない。
 */
static void task_clear_context(task_context_t *context)
{
    if (context == NULL) {
        return;
    }

    context->rsp = 0;
    context->rbp = 0;
    context->rbx = 0;
    context->r12 = 0;
    context->r13 = 0;
    context->r14 = 0;
    context->r15 = 0;
}

/**
 * @brief 登録済みtask用のcontext保存領域を初期化する。
 *
 * @details
 * 第5章5.2では、`rsp` に将来の復元候補としてTCBの `stack_top` を入れ、
 * その他のregister保存領域は0にする。ここで設定した `rsp` はmetadataであり、
 * CPUのRSPへロードしない。task entryも引き続き通常のC関数呼び出しで実行される。
 *
 * @param task 初期化対象の登録済みTCB。
 * @return なし。
 * @note stack switch、context switch、register save/restoreは行わない。
 */
static void task_initialize_context(tcb_t *task)
{
    if (task == NULL) {
        return;
    }

    task_clear_context(&task->context);
    task->context.rsp = (unsigned long)task->stack_top;
}

/**
 * @brief task context保存領域をログへ出力する。
 *
 * @details
 * 登録ログとdumpログで同じfield順序を保つための表示補助である。
 * 出力される値はTCB内metadataであり、現在のCPU register値ではない。
 *
 * @param context 表示対象のcontext保存領域。
 * @return なし。
 * @note 表示専用であり、contextやCPU状態を変更しない。
 */
static void task_write_context(const task_context_t *context)
{
    hal_console_write(" context.rsp=");
    task_write_hex(context->rsp);
    hal_console_write(" context.rbp=");
    task_write_hex(context->rbp);
    hal_console_write(" context.rbx=");
    task_write_hex(context->rbx);
    hal_console_write(" context.r12=");
    task_write_hex(context->r12);
    hal_console_write(" context.r13=");
    task_write_hex(context->r13);
    hal_console_write(" context.r14=");
    task_write_hex(context->r14);
    hal_console_write(" context.r15=");
    task_write_hex(context->r15);
}

/**
 * @brief 静的タスクテーブルから登録可能なスロットを探す。
 *
 * @details
 * 将来idやnameの意味が増えても空き判定がぶれないよう、
 * `state == TASK_STATE_UNUSED` だけを根拠にする。
 *
 * @param なし。
 * @return 空きTCBへのポインタ。空きがなければNULL。
 * @note IDや名前ではなく状態だけで空きを判定する。
 */
static tcb_t *find_free_slot(void)
{
    int index;

    for (index = 0; index < MAX_TASKS; index++) {
        /* UNUSEDだけを空き判定に使い、idやnameの値には依存しない。 */
        if (task_table[index].state == TASK_STATE_UNUSED) {
            return &task_table[index];
        }
    }

    return NULL;
}

/**
 * @brief 再利用しないタスクIDを採番する。
 *
 * @details
 * `next_task_id` を単純に進める。オーバーフロー時は巻き戻さず、
 * 将来の削除API追加後もID再利用を避ける設計意図を保つ。
 *
 * @param なし。
 * @return 成功時は1以上のID。採番不能時はTASK_ERR_ID_OVERFLOW。
 * @note ID 0は無効値として予約する。
 */
static int allocate_task_id(void)
{
    int id;

    /* ID 0は無効なので、正の範囲で採番できない場合は失敗にする。 */
    if (next_task_id <= 0 || next_task_id >= TASK_ID_MAX) {
        return TASK_ERR_ID_OVERFLOW;
    }

    /* IDは配列インデックスとは独立した単純インクリメントで管理する。 */
    id = next_task_id;
    next_task_id++;

    return id;
}

/**
 * @brief タスク状態をログ表示用の文字列に変換する。
 *
 * @details
 * dump出力を読みやすくし、将来状態が増えた場合も変換箇所を集約する。
 *
 * @param state 変換対象のタスク状態。
 * @return 状態名を表す静的文字列。
 * @note 未知値はUNKNOWNとして表示し、dump処理を継続する。
 */
static const char *task_state_to_string(task_state_t state)
{
    switch (state) {
    case TASK_STATE_UNUSED:
        return "UNUSED";
    case TASK_STATE_DORMANT:
        return "DORMANT";
    case TASK_STATE_READY:
        return "READY";
    case TASK_STATE_RUNNING:
        return "RUNNING";
    case TASK_STATE_WAITING:
        return "WAITING";
    default:
        return "UNKNOWN";
    }
}

/**
 * @brief タスク管理テーブル全体を初期化する。
 *
 * @details
 * 全スロットを未使用状態へ戻し、ID採番を初期値1に戻す。
 * 起動時に既知の状態から登録確認を始めるための処理である。
 *
 * @param なし。
 * @return なし。
 * @note entry関数呼び出し、コンテキスト作成、スタック初期化は行わない。
 */
void task_init(void)
{
    int index;

    for (index = 0; index < MAX_TASKS; index++) {
        /* 登録済み情報が残らないよう、全フィールドを既知の初期値に戻す。 */
        task_table[index].id = 0;
        task_table[index].name = NULL;
        task_table[index].entry = NULL;
        task_table[index].priority = 0;
        task_table[index].state = TASK_STATE_UNUSED;
        task_table[index].wait_sem_id = 0;
        task_table[index].stack_base = NULL;
        task_table[index].stack_size = 0;
        task_table[index].stack_top = NULL;
        task_clear_context(&task_table[index].context);
    }

    /* ID 0を無効値として残すため、最初の有効IDは1から始める。 */
    next_task_id = 1;
    hal_console_write("[kernel] task init\n");
}

/**
 * @brief タスク情報を静的テーブルへ登録する。
 *
 * @details
 * 入力を検証し、空きスロットへTCBを設定する。
 * 第3章3.1では登録確認だけを目的とするため、entry呼び出し、コンテキスト作成、
 * スタック初期化は行わない。
 *
 * @param name タスク名。NULLの場合はTASK_ERR_INVAL。
 * @param entry タスク入口関数。NULLの場合はTASK_ERR_INVALだが、この関数では呼び出さない。
 * @param priority 優先度。今回は保存とdumpだけに使う。
 * @param stack_base スタック基底アドレス。NULLの場合はTASK_ERR_INVALだが初期化しない。
 * @param stack_size スタックサイズ。0の場合はTASK_ERR_INVAL。
 * @return 成功時は1以上のタスクID。失敗時はTASK_ERR_*。
 * @note この関数は登録のみを行う。task_start、コンテキストスイッチ、割り込み、タイマは未実装である。
 */
int task_register(
    const char *name,
    task_entry_t entry,
    int priority,
    void *stack_base,
    unsigned long stack_size
)
{
    tcb_t *slot;
    int id;

    /* 不完全なTCBを登録しないため、必須情報が欠けていれば即エラーにする。 */
    if (name == NULL || entry == NULL || stack_base == NULL || stack_size == 0) {
        return TASK_ERR_INVAL;
    }

    /* 空きスロットはUNUSED状態だけで探す。idやnameは判定に使わない。 */
    slot = find_free_slot();
    if (slot == NULL) {
        return TASK_ERR_FULL;
    }

    /* TCBを書き込む前にIDを確保し、採番不能ならテーブルを変更しない。 */
    id = allocate_task_id();
    if (id < 0) {
        return id;
    }

    slot->id = id;
    slot->name = name;
    slot->entry = entry;
    slot->priority = priority;
    /* 登録済みだが未実行のタスクとして、将来のスケジューラ候補にしやすいREADYへ置く。 */
    slot->state = TASK_STATE_READY;
    slot->wait_sem_id = 0;
    slot->stack_base = stack_base;
    slot->stack_size = stack_size;
    slot->stack_top = task_calculate_stack_top(stack_base, stack_size);
    task_initialize_context(slot);

    /*
     * ここでは登録内容を可視化するだけで、entry関数は呼び出さない。
     * context保存領域はTCB metadataとして初期化するが、CPU register保存・復元、
     * スタック切り替え、RSPへのcontext.rspロードは将来フェーズの責務として残す。
     */
    hal_console_write("[task] registered: id=");
    task_write_int(slot->id);
    hal_console_write(" name=");
    hal_console_write(slot->name);
    hal_console_write(" state=");
    hal_console_write(task_state_to_string(slot->state));
    hal_console_write(" wait_sem_id=");
    task_write_int(slot->wait_sem_id);
    hal_console_write(" prio=");
    task_write_int(slot->priority);
    hal_console_write(" entry=");
    task_write_hex((unsigned long)slot->entry);
    hal_console_write(" stack_base=");
    task_write_hex((unsigned long)slot->stack_base);
    hal_console_write(" stack_size=");
    task_write_uint(slot->stack_size);
    hal_console_write(" stack_top=");
    task_write_hex((unsigned long)slot->stack_top);
    task_write_context(&slot->context);
    hal_console_write("\n");

    return id;
}

/**
 * @brief 登録済みタスクを一覧表示する。
 *
 * @details
 * 静的テーブル全体を走査し、UNUSED以外のTCBだけをHAL consoleへ出力する。
 * QEMU `-serial stdio` で登録状態を確認するための診断処理である。
 *
 * @param なし。
 * @return なし。
 * @note dumpは表示専用で、タスク状態やID採番状態を変更しない。
 */
void task_dump(void)
{
    int index;

    hal_console_write("[task] dump start\n");

    for (index = 0; index < MAX_TASKS; index++) {
        tcb_t *task = &task_table[index];

        /* UNUSEDは未登録スロットなので、dump対象から除外する。 */
        if (task->state == TASK_STATE_UNUSED) {
            continue;
        }

        /* 登録済みTCBの主要フィールドを出し、将来の状態遷移追加時の確認材料にする。 */
        hal_console_write("[task] id=");
        task_write_int(task->id);
        hal_console_write(" name=");
        hal_console_write(task->name);
        hal_console_write(" prio=");
        task_write_int(task->priority);
        hal_console_write(" state=");
        hal_console_write(task_state_to_string(task->state));
        hal_console_write(" wait_sem_id=");
        task_write_int(task->wait_sem_id);
        hal_console_write(" entry=");
        task_write_hex((unsigned long)task->entry);
        hal_console_write(" stack_base=");
        task_write_hex((unsigned long)task->stack_base);
        hal_console_write(" stack_size=");
        task_write_uint(task->stack_size);
        hal_console_write(" stack_top=");
        task_write_hex((unsigned long)task->stack_top);
        task_write_context(&task->context);
        hal_console_write("\n");
    }

    hal_console_write("[task] dump end\n");
}

/**
 * @brief schedulerが走査できるタスクスロット数を返す。
 *
 * @details
 * 第3章3.2のschedulerはtask_tableを直接extern参照せず、このAPIで固定長テーブルの
 * 走査範囲だけを取得する。task_tableの所有権はこのファイルに閉じたままにする。
 *
 * @param なし。
 * @return 走査可能なスロット数。常にMAX_TASKS。
 * @note task_tableの内容やタスク状態は変更しない。
 */
int task_get_count(void)
{
    return MAX_TASKS;
}

/**
 * @brief 指定indexのTCBを読み取り専用で返す。
 *
 * @details
 * schedulerがREADYタスクを選択するための読み取り専用アクセサである。
 * 範囲外indexはNULLとして扱い、呼び出し側が安全に無視できるようにする。
 *
 * @param index 参照するtask_table上のindex。
 * @return 範囲内ならTCBへの読み取り専用ポインタ。範囲外ならNULL。
 * @note 返したTCBのentryを呼び出さず、状態変更や実行制御も行わない。
 */
const tcb_t *task_get_by_index(int index)
{
    if (index < 0 || index >= MAX_TASKS) {
        return NULL;
    }

    return &task_table[index];
}

const tcb_t *task_get_by_id(int task_id)
{
    int index;

    /*
     * task_idは1以上だけを有効にする。0は未使用・無効値として扱うため、
     * semaphore moduleが誤って待ちなしIDを渡してもTCBへ到達しない。
     */
    if (task_id <= 0) {
        return NULL;
    }

    for (index = 0; index < MAX_TASKS; index++) {
        const tcb_t *task = &task_table[index];

        if (task->state == TASK_STATE_UNUSED) {
            continue;
        }

        /*
         * 読み取り専用APIなので、見つかったTCBをそのまま返すだけにする。
         * 状態変更はtask_mark_*系APIへ集約し、呼び出し側の責務を限定する。
         */
        if (task->id == task_id) {
            return task;
        }
    }

    return NULL;
}

tcb_t *task_get_mutable_by_id(int task_id)
{
    int index;

    if (task_id <= 0) {
        return NULL;
    }

    for (index = 0; index < MAX_TASKS; index++) {
        tcb_t *task = &task_table[index];

        if (task->state == TASK_STATE_UNUSED) {
            continue;
        }

        if (task->id == task_id) {
            return task;
        }
    }

    return NULL;
}

/**
 * @brief 登録済みREADYタスクを論理的なRUNNING状態へ変更する。
 *
 * @details
 * dispatcherの現在タスク確定経路から利用される状態変更処理である。
 * task_tableの所有権はtask.cに残したまま、タスクIDから有効なTCBを探し、
 * TASK_STATE_READYだけをTASK_STATE_RUNNINGへ変更する。
 * この章ではタスク入口関数の呼び出し、コンテキストスイッチ、スタック切り替え、
 * レジスタ保存・復元は行わない。
 *
 * @param task_id 登録済みタスクID。0以下は不正。
 * @return 成功時は0、失敗時は負のTASK_ERR_*値。
 */
int task_mark_running(int task_id)
{
    int index;

    /*
     * 0以下のIDはTCB登録で発行しない。
     * 不正IDを早期に拒否しておくことで、将来API経由でtask_idが渡される場合も
     * task_table走査前に入力エラーとして扱える。
     */
    if (task_id <= 0) {
        return TASK_ERR_INVAL;
    }

    for (index = 0; index < MAX_TASKS; index++) {
        tcb_t *task = &task_table[index];

        /*
         * UNUSEDスロットは未登録領域なので状態変更対象にしない。
         * task_tableをstaticに保ちながら内部でだけ走査することで、
         * 将来TCB配置を変えても外部APIの契約を保てる。
         */
        if (task->state == TASK_STATE_UNUSED) {
            continue;
        }

        if (task->id != task_id) {
            continue;
        }

        /*
         * RUNNINGへ進める入口をREADYに限定する。
         * これにより、schedulerの選択結果だけがdispatcher経由でcurrent化される
         * という第3章3.3の状態モデルを保てる。
         */
        if (task->state != TASK_STATE_READY) {
            return TASK_ERR_BAD_STATE;
        }

        /*
         * 第3章3.3の論理状態確定だけを行う。
         * 入口関数呼び出し、スタック切り替え、レジスタ保存、コンテキストスイッチは行わない。
         */
        task->state = TASK_STATE_RUNNING;
        return 0;
    }

    return TASK_ERR_NOT_FOUND;
}

/**
 * @brief RUNNING taskをREADY候補へ戻す。
 *
 * @details
 * 第4章4.3のboot-time verification modelで、entry returnを
 * cooperative return eventとして観測した後に使用する。
 * 第10章10.2では `yield_tsk()` からも呼び出され、RUNNING current taskだけを
 * READYへ戻す状態遷移APIとして使う。
 * 対象taskがRUNNINGの場合だけREADYへ戻し、再びscheduler候補にする。
 *
 * これはtask restartではない。10.2時点のyield用途でもREADY化までに限定し、
 * 次task選択、dispatcher switch、コンテキストスイッチ、スタック切り替え、
 * レジスタ保存・復元、割り込み、タイマ、プリエンプションは行わない。
 *
 * @param task_id 登録済みタスクID。0以下は不正。
 * @return 成功時は0、失敗時は負のTASK_ERR_*値。
 */
int task_mark_ready_from_running(int task_id)
{
    int index;

    if (task_id <= 0) {
        return TASK_ERR_INVAL;
    }

    for (index = 0; index < MAX_TASKS; index++) {
        tcb_t *task = &task_table[index];

        if (task->state == TASK_STATE_UNUSED) {
            continue;
        }

        if (task->id != task_id) {
            continue;
        }

        /*
         * 4.3のcooperative re-candidacyと10.2のyield READY化はいずれも
         * RUNNINGからREADYだけを許す。DORMANTや終了状態への遷移、
         * task restartはここでは扱わない。
         */
        if (task->state != TASK_STATE_RUNNING) {
            return TASK_ERR_BAD_STATE;
        }

        task->state = TASK_STATE_READY;
        return 0;
    }

    return TASK_ERR_NOT_FOUND;
}

/**
 * @brief entry returnしたtaskをDORMANTへ最終化する。
 *
 * @details
 * 第9章9.4で、task entry関数のreturnを「READYへの再投入」ではなく
 * その起動分の実行完了として扱うための状態変更APIである。
 * RUNNINGは通常のentry return対象として、READYは9.3のdispatcher_switch_to()が
 * from taskへ適用したRUNNING->READY遷移後の最終化対象として受け付ける。
 *
 * この関数はTCB状態だけを更新し、entry呼び出し、scheduler選択、
 * dispatcher current更新、dispatch pending消費、interrupt exit接続は行わない。
 *
 * @param task_id 登録済みタスクID。0以下は不正。
 * @return 成功時は0、失敗時は負のTASK_ERR_*値。
 */
int task_mark_dormant_from_entry_return(int task_id)
{
    int index;

    if (task_id <= 0) {
        return TASK_ERR_INVAL;
    }

    for (index = 0; index < MAX_TASKS; index++) {
        tcb_t *task = &task_table[index];

        /*
         * UNUSEDは未登録スロットであり、entry returnという実行履歴を持たない。
         * 未登録領域をDORMANTへ変えると「生成済みtask」と誤認されるため対象外にする。
         */
        if (task->state == TASK_STATE_UNUSED) {
            continue;
        }

        /* task idが一致するTCBだけを状態変更対象にし、task tableの所有権はtask.cに閉じる。 */
        if (task->id != task_id) {
            continue;
        }

        /*
         * entry return後の最終化元として許す状態をRUNNING/READYに限定する。
         * READYを許すのは9.3のdispatcherがswitch元taskを先にREADYへ戻すためであり、
         * WAITINGやDORMANTからの再完了、未設計の再起動APIを暗黙に認めないためである。
         */
        if (task->state != TASK_STATE_RUNNING && task->state != TASK_STATE_READY) {
            return TASK_ERR_BAD_STATE;
        }

        /*
         * ここで行うのはTCB上のlifecycle確定だけである。
         * ready queue操作、scheduler再選択、dispatcher current更新、context保存復元は行わない。
         */
        task->state = TASK_STATE_DORMANT;
        return 0;
    }

    return TASK_ERR_NOT_FOUND;
}

/**
 * @brief セマフォ待ちによるWAITING遷移をtask module内で実行する。
 *
 * @details
 * 第6章6.1ではWAITINGを観測可能な最小状態として扱う。ここではTCBのstateと
 * wait_sem_idだけを更新し、wait queue、timeout、timer、preemption、interrupt、
 * context switch連携は行わない。
 *
 * @param task_id 対象task id。
 * @param sem_id 待ち対象セマフォID。
 * @return 成功時は0、失敗時は負のTASK_ERR_*値。
 */
int task_mark_waiting_on_sem(int task_id, int sem_id)
{
    tcb_t *task = task_get_mutable_by_id(task_id);

    /*
     * sem_id==0は「待ちなし」を表す値としてTCBに使うため、
     * WAITING遷移の待ち対象としては受け付けない。
     */
    if (task_id <= 0 || sem_id <= 0) {
        return TASK_ERR_INVAL;
    }

    if (task == NULL) {
        return TASK_ERR_NOT_FOUND;
    }

    /*
     * 第6章6.1ではREADYまたはRUNNINGからの観測用待ち入りだけを許す。
     * DORMANTや既にWAITINGのtaskを重ねて待たせる設計は、wait queue導入時に扱う。
     */
    if (task->state != TASK_STATE_READY && task->state != TASK_STATE_RUNNING) {
        return TASK_ERR_BAD_STATE;
    }

    /*
     * ここで初めてTCBを更新する。semaphore moduleではなくtask moduleに閉じることで、
     * schedulerやdispatcherが参照するstateの所有権を分散させない。
     */
    task->state = TASK_STATE_WAITING;
    task->wait_sem_id = sem_id;

    hal_console_write("[task] waiting: id=");
    task_write_int(task->id);
    hal_console_write(" name=");
    hal_console_write(task->name);
    hal_console_write(" wait_sem_id=");
    task_write_int(task->wait_sem_id);
    hal_console_write(" state=");
    hal_console_write(task_state_to_string(task->state));
    hal_console_write("\n");

    return 0;
}

/**
 * @brief 指定セマフォを待つtaskを読み取り専用で1件探す。
 *
 * @details
 * sig_sem側がwakeupログを先に組み立てるための探索APIである。
 * task状態は変更しない。wait queue未導入のため、探索順はtask table順であり、
 * FIFO順や優先度順を意味しない。
 *
 * @param sem_id 対象セマフォID。
 * @return 見つかったWAITING task。対象なしや不正入力はNULL。
 */
const tcb_t *task_find_waiting_on_sem(int sem_id)
{
    int index;

    if (sem_id <= 0) {
        return NULL;
    }

    for (index = 0; index < MAX_TASKS; index++) {
        const tcb_t *task = &task_table[index];

        /*
         * wait_sem_idだけではなくstateも確認する。
         * READY taskに古いwait_sem_idが残るような不整合が将来起きても、
         * wakeup候補として誤認しないためである。
         */
        if (task->state == TASK_STATE_WAITING && task->wait_sem_id == sem_id) {
            return task;
        }
    }

    return NULL;
}

/**
 * @brief 指定セマフォを待つtaskを1件だけREADYへ戻す。
 *
 * @details
 * wait queueをまだ持たない第6章6.1の最小wakeupである。task tableの走査順で
 * 最初に見つかったtaskだけをREADYへ戻すため、FIFO順や優先度順は保証しない。
 * 将来のwait queue導入時にはこの探索境界を置き換える。
 *
 * @param sem_id 対象セマフォID。
 * @param woken_task_id wakeupしたtask idの格納先。NULLも許容する。
 * @return wakeup成功時は0。対象なしや不正入力は負のTASK_ERR_*値。
 */
int task_wake_one_waiting_on_sem(int sem_id, int *woken_task_id)
{
    int index;

    /*
     * sem_id==0は待ちなしを表す予約値なので、wakeup対象として扱わない。
     */
    if (sem_id <= 0) {
        return TASK_ERR_INVAL;
    }

    for (index = 0; index < MAX_TASKS; index++) {
        tcb_t *task = &task_table[index];

        if (task->state != TASK_STATE_WAITING) {
            continue;
        }

        if (task->wait_sem_id != sem_id) {
            continue;
        }

        /*
         * 第6章6.1のwakeupは「最初に見つかった1件だけ」をREADYへ戻す。
         * ここにはFIFO/priorityの意味を持たせず、将来wait queueで置き換える。
         */
        task->state = TASK_STATE_READY;
        task->wait_sem_id = 0;

        /*
         * 呼び出し側がwakeup対象を追加ログや検証に使えるようIDを返す。
         * NULLは許容し、READY遷移そのものは常に実行する。
         */
        if (woken_task_id != NULL) {
            *woken_task_id = task->id;
        }

        hal_console_write("[task] ready: id=");
        task_write_int(task->id);
        hal_console_write(" name=");
        hal_console_write(task->name);
        hal_console_write(" state=");
        hal_console_write(task_state_to_string(task->state));
        hal_console_write(" wait_sem_id=");
        task_write_int(task->wait_sem_id);
        hal_console_write("\n");

        return 0;
    }

    return TASK_ERR_NOT_FOUND;
}
