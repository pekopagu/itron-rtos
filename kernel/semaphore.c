/*
 * SPDX-License-Identifier: MIT
 */

/**
 * @file semaphore.c
 * @brief 学習用μITRON風セマフォ管理の最小実装（第6章6.1）。
 *
 * @details
 * 静的semaphore table、count操作、WAITING遷移の観測ログを提供する。
 * このファイルはセマフォのid/name/count/max_countだけを所有し、taskの状態更新は
 * task moduleのAPIへ委譲する。scheduler、dispatcher、context switch、arch層へ
 * セマフォ責務を入れないための境界である。
 *
 * この段階のWAITINGはboot-time verification用の最小状態であり、timer、
 * timeout付き待ち、preemption、interrupt、wait queueとは接続していない。
 */

#include <stddef.h>

#include "hal/console.h"
#include "semaphore.h"
#include "task.h"

#define SEM_ID_MAX 2147483647

static semaphore_t semaphore_table[MAX_SEMAPHORES];
static int next_sem_id = 1;

/**
 * @brief 符号なし整数をHAL consoleへ10進出力する。
 *
 * @param value 出力する値。
 */
static void sem_write_uint(unsigned long value)
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
 * @param value 出力する値。
 */
static void sem_write_int(int value)
{
    if (value < 0) {
        hal_console_putc('-');
        sem_write_uint((unsigned long)(-value));
        return;
    }

    sem_write_uint((unsigned long)value);
}

/**
 * @brief 未使用のsemaphore slotを探す。
 *
 * @return 空きslot。なければNULL。
 */
static semaphore_t *find_free_semaphore_slot(void)
{
    int index;

    for (index = 0; index < MAX_SEMAPHORES; index++) {
        /*
         * 第6章6.1では削除APIを持たないため、id==0だけを未使用slotの印にする。
         * nameやcountを空き判定に使わないことで、将来フィールドが増えても境界を保つ。
         */
        if (semaphore_table[index].id == 0) {
            return &semaphore_table[index];
        }
    }

    return NULL;
}

/**
 * @brief セマフォIDを単調に採番する。
 *
 * @return 成功時は1以上のID。失敗時はSEM_ERR_OVERFLOW。
 */
static int allocate_sem_id(void)
{
    int id;

    /*
     * ID 0は「無効」「待ちなし」の観測値として使うため発行しない。
     * wraparoundによるID再利用も避け、ログ上の追跡を単純に保つ。
     */
    if (next_sem_id <= 0 || next_sem_id >= SEM_ID_MAX) {
        return SEM_ERR_OVERFLOW;
    }

    id = next_sem_id;
    next_sem_id++;
    return id;
}

/**
 * @brief IDからsemaphore entryを探す。
 *
 * @param sem_id 探索対象ID。
 * @return 見つかったentry。なければNULL。
 */
static semaphore_t *find_semaphore_by_id(int sem_id)
{
    int index;

    if (sem_id <= 0) {
        return NULL;
    }

    for (index = 0; index < MAX_SEMAPHORES; index++) {
        semaphore_t *sem = &semaphore_table[index];

        /*
         * 静的tableの線形探索に留める。ハッシュや動的管理は6.1の観測目的には不要で、
         * 将来の削除・再利用設計を先取りしない。
         */
        if (sem->id == sem_id) {
            return sem;
        }
    }

    return NULL;
}

/**
 * @brief 静的semaphore tableを起動直後の状態へ戻す。
 *
 * @details
 * すべてのslotを未使用化し、ID採番も初期化する。ここではtask状態、
 * scheduler、dispatcher、timer、interruptには触れない。セマフォ管理だけを
 * 再現可能な観測状態へ戻すための処理である。
 *
 * @return SEM_OK。
 */
int sem_init(void)
{
    int index;

    for (index = 0; index < MAX_SEMAPHORES; index++) {
        /*
         * id==0を未使用slotの唯一の判定条件にするため、関連fieldも既知値へ戻す。
         * staleなname/countがdumpへ混ざらないよう、表示情報も同時に消す。
         */
        semaphore_table[index].id = 0;
        semaphore_table[index].name = NULL;
        semaphore_table[index].count = 0;
        semaphore_table[index].max_count = 0;
    }

    next_sem_id = 1;
    hal_console_write("[sem] table initialized\n");
    return SEM_OK;
}

/**
 * @brief 静的table上に観測用セマフォを作成する。
 *
 * @details
 * countの不変条件 `0 <= count <= max_count` を作成時点で確定する。
 * 不正な初期値は後続のwai_sem/sig_semで曖昧な状態を作るため、tableを変更せず
 * 失敗として返す。
 *
 * @param name セマフォ名。ログ識別に使うためNULL不可。
 * @param initial_count 初期count。
 * @param max_count count上限。
 * @return 成功時はセマフォID。失敗時はSEM_ERR_*。
 */
int sem_create(const char *name, int initial_count, int max_count)
{
    semaphore_t *slot;
    int id;

    /*
     * max_count==0のセマフォは常に取得不能で、6.1のcount変化観測に使えない。
     * initial_count > max_countも不変条件を破るため、作成前に拒否する。
     */
    if (name == NULL || initial_count < 0 || max_count <= 0 || initial_count > max_count) {
        return SEM_ERR_INVAL;
    }

    /*
     * 静的tableだけで管理する。動的メモリ確保を導入しないことで、
     * boot-time verification modelの再現性と単純さを保つ。
     */
    slot = find_free_semaphore_slot();
    if (slot == NULL) {
        return SEM_ERR_FULL;
    }

    /*
     * ID採番が失敗した場合はslotへ何も書き込まない。
     * 途中まで初期化されたセマフォをdumpに出さないための順序である。
     */
    id = allocate_sem_id();
    if (id < 0) {
        return id;
    }

    slot->id = id;
    slot->name = name;
    slot->count = initial_count;
    slot->max_count = max_count;

    hal_console_write("[sem] initialized: id=");
    sem_write_int(slot->id);
    hal_console_write(" name=");
    hal_console_write(slot->name);
    hal_console_write(" count=");
    sem_write_int(slot->count);
    hal_console_write(" max_count=");
    sem_write_int(slot->max_count);
    hal_console_write("\n");

    return id;
}

/**
 * @brief wai_sem相当の取得処理を実行する。
 *
 * @details
 * countが残っている場合は即時取得としてcountだけを減らす。countが0の場合は
 * task moduleへWAITING遷移を委譲する。semaphore moduleはTCBを直接更新せず、
 * wait queueやtimeoutの責務も持たない。
 *
 * @param sem_id 対象セマフォID。
 * @param task_id 取得または待ち入り対象task ID。
 * @return 成功時はSEM_OK。失敗時はSEM_ERR_*。
 */
int wai_sem(int sem_id, int task_id)
{
    semaphore_t *sem = find_semaphore_by_id(sem_id);
    int count_before;
    int result;
    const tcb_t *task;
    const char *task_name;

    /*
     * sem_idの解決とtask_idの基本検証を先に行い、不正入力ではcountもtask状態も
     * 変更しない。失敗時に観測対象の状態が動かないことを保証する。
     */
    if (sem == NULL || task_id <= 0) {
        return SEM_ERR_INVAL;
    }

    /*
     * ログにtask名を含めるため、状態変更前に読み取り専用でTCBを確認する。
     * ここではTCBを書き換えない。
     */
    task = task_get_by_id(task_id);
    if (task == NULL) {
        return SEM_ERR_TASK;
    }

    task_name = (task->name != NULL) ? task->name : "(null)";
    count_before = sem->count;

    if (sem->count > 0) {
        /*
         * 即時取得できる経路ではtask状態を変えない。
         * 6.1では「countが減ること」を観測するだけで、owner管理は導入しない。
         */
        sem->count--;
        hal_console_write("[sem] wai_sem: task id=");
        sem_write_int(task->id);
        hal_console_write(" name=");
        hal_console_write(task_name);
        hal_console_write(" sem id=");
        sem_write_int(sem->id);
        hal_console_write(" name=");
        hal_console_write(sem->name);
        hal_console_write(" count_before=");
        sem_write_int(count_before);
        hal_console_write(" count_after=");
        sem_write_int(sem->count);
        hal_console_write(" result=ok\n");
        return SEM_OK;
    }

    /*
     * count==0の経路では、まずsemaphore側の判定結果をログに出す。
     * 直後にtask moduleがWAITING遷移ログを出すため、判断と状態変更の境界が追いやすい。
     */
    hal_console_write("[sem] wai_sem: task id=");
    sem_write_int(task->id);
    hal_console_write(" name=");
    hal_console_write(task_name);
    hal_console_write(" sem id=");
    sem_write_int(sem->id);
    hal_console_write(" name=");
    hal_console_write(sem->name);
    hal_console_write(" count_before=");
    sem_write_int(count_before);
    hal_console_write(" result=waiting\n");

    /*
     * WAITING遷移の所有者はtask moduleである。semaphore moduleは「どのsemを待つか」
     * だけを渡し、TCB内部のstate/wait_sem_id更新は委譲する。
     */
    result = task_mark_waiting_on_sem(task_id, sem_id);
    if (result != 0) {
        hal_console_write("[sem] wai_sem: result=error err=");
        sem_write_int(result);
        hal_console_write("\n");
        return SEM_ERR_TASK;
    }

    return SEM_OK;
}

/**
 * @brief sig_sem相当の返却処理を実行する。
 *
 * @details
 * 対象セマフォを待つtaskがあれば、countを増やさずに1 taskだけREADYへ戻す。
 * 待ちtaskがなければcountを1増やす。FIFO順や優先度順はまだ保証せず、
 * 将来のwait queue導入で探索処理を置き換える。
 *
 * @param sem_id 対象セマフォID。
 * @return 成功時はSEM_OK。失敗時はSEM_ERR_*。
 */
int sig_sem(int sem_id)
{
    semaphore_t *sem = find_semaphore_by_id(sem_id);
    int count_before;
    int woken_task_id = 0;
    int wake_result;
    const tcb_t *waiting_task;
    const char *woken_name;

    /*
     * 存在しないセマフォへのsignalでは、countもtask状態も変更しない。
     * エラー時の副作用をなくし、QEMUログ上の原因切り分けを簡単にする。
     */
    if (sem == NULL) {
        return SEM_ERR_INVAL;
    }

    count_before = sem->count;
    waiting_task = task_find_waiting_on_sem(sem_id);
    if (waiting_task != NULL) {
        /*
         * wakeup対象の名前をsig_semログへ含めるため、先に読み取り専用探索を行う。
         * 実際のREADY遷移はこの後もtask moduleへ委譲し、TCB直接更新はしない。
         */
        woken_task_id = waiting_task->id;
        woken_name = (waiting_task->name != NULL) ? waiting_task->name : "(null)";

        hal_console_write("[sem] sig_sem: sem id=");
        sem_write_int(sem->id);
        hal_console_write(" name=");
        hal_console_write(sem->name);
        hal_console_write(" count_before=");
        sem_write_int(count_before);
        hal_console_write(" result=wakeup task id=");
        sem_write_int(woken_task_id);
        hal_console_write(" name=");
        hal_console_write(woken_name);
        hal_console_write("\n");

        /*
         * 待ちtaskへ資源を渡した扱いにするため、この経路ではcountを増やさない。
         * 6.1では1 taskだけの最小wakeupで、複数待ちや順序保証は扱わない。
         */
        wake_result = task_wake_one_waiting_on_sem(sem_id, &woken_task_id);
        if (wake_result != 0) {
            hal_console_write("[sem] sig_sem: wakeup failed err=");
            sem_write_int(wake_result);
            hal_console_write("\n");
            return SEM_ERR_TASK;
        }
        return SEM_OK;
    }

    /*
     * 待ちtaskがいない場合だけcountを増やす。max_count超過を拒否することで、
     * semaphore_tの不変条件をsig_sem側でも維持する。
     */
    if (sem->count >= sem->max_count) {
        hal_console_write("[sem] sig_sem: sem id=");
        sem_write_int(sem->id);
        hal_console_write(" name=");
        hal_console_write(sem->name);
        hal_console_write(" count_before=");
        sem_write_int(count_before);
        hal_console_write(" result=overflow\n");
        return SEM_ERR_OVERFLOW;
    }

    sem->count++;
    hal_console_write("[sem] sig_sem: sem id=");
    sem_write_int(sem->id);
    hal_console_write(" name=");
    hal_console_write(sem->name);
    hal_console_write(" count_before=");
    sem_write_int(count_before);
    hal_console_write(" count_after=");
    sem_write_int(sem->count);
    hal_console_write(" result=ok\n");
    return SEM_OK;
}

/**
 * @brief 登録済みセマフォの状態を一覧出力する。
 *
 * @details
 * dumpは観測専用であり、countやtask状態を変更しない。未使用slotは出力せず、
 * 第6章6.1で確認したいid/name/count/max_countだけを表示する。
 */
void sem_dump(void)
{
    int index;

    hal_console_write("[sem] dump start\n");

    for (index = 0; index < MAX_SEMAPHORES; index++) {
        const semaphore_t *sem = &semaphore_table[index];

        /* 未使用slotはdump対象外にし、観測ログを登録済みセマフォだけに絞る。 */
        if (sem->id == 0) {
            continue;
        }

        hal_console_write("[sem] id=");
        sem_write_int(sem->id);
        hal_console_write(" name=");
        hal_console_write(sem->name);
        hal_console_write(" count=");
        sem_write_int(sem->count);
        hal_console_write(" max_count=");
        sem_write_int(sem->max_count);
        hal_console_write("\n");
    }

    hal_console_write("[sem] dump end\n");
}
