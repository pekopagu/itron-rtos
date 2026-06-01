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

#include "dispatcher.h"
#include "hal/console.h"
#include "semaphore.h"
#include "task.h"

#define SEM_ID_MAX 2147483647

static semaphore_t semaphore_table[MAX_SEMAPHORES];
static int next_sem_id = 1;

/**
 * @brief semaphore wait queueを空状態へ初期化する。
 *
 * @details
 * 12.3のwait queueは固定長task id配列によるFIFOである。ここではqueue metadataだけを
 * 初期化し、priority順、timeout、sleep/delay queue、wakeup後preemption判定は扱わない。
 *
 * @param sem 初期化対象のsemaphore。NULLの場合は何もしない。
 */
static void sem_reset_wait_queue(semaphore_t *sem)
{
    int index;

    if (sem == NULL) {
        return;
    }

    for (index = 0; index < MAX_TASKS; index++) {
        sem->wait_queue[index] = 0;
    }
    sem->wait_head = 0;
    sem->wait_tail = 0;
    sem->wait_count = 0;
}

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
        /*
         * 桁分解loopでは0が出力されないため、明示的に1文字だけ出す。
         * semaphore logのcount/max_countで0を正しく観測するための処理である。
         */
        hal_console_putc('0');
        return;
    }

    /*
     * 標準printfを使わず、HAL consoleだけで数値を出す。
     * 下位桁からbufferへ積み、最後に逆順で出すことで10進表記を作る。
     */
    while (value > 0) {
        buffer[index++] = (char)('0' + (value % 10));
        value /= 10;
    }

    /* buffer内は逆順なので、末尾から戻しながら通常の桁順で出力する。 */
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
        /*
         * エラーコードは負値で返すため、符号を先に出してから絶対値部分を共通helperへ渡す。
         * ログの数値出力だけを担当し、semaphore状態は変更しない。
         */
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
/**
 * @brief sig_sem()のtask表示用にid/nameだけを出力する。
 *
 * @details
 * 第12章12.4のwakeup後preemption判定では、current taskとwoken taskを
 * 同じ形式で複数回ログへ出す。ここではTCBを読み取るだけで、task状態、
 * dispatcher current、semaphore count、wait queueは変更しない。
 *
 * @param label 表示するtaskの役割名。例: `current`、`woken`、`from`、`to`。
 * @param task 表示対象task。NULLの場合は `none` として出力する。
 */
static void sig_sem_log_task_id_name(const char *label, const tcb_t *task)
{
    hal_console_write(label);
    if (task == NULL) {
        hal_console_write(" none");
        return;
    }

    hal_console_write(" id=");
    sem_write_int(task->id);
    hal_console_write(" name=");
    hal_console_write((task->name != NULL) ? task->name : "(null)");
}

/**
 * @brief READY復帰済みtaskがcurrentより高優先度なら既存dispatcher境界へ進める。
 *
 * @details
 * 第12章12.4のtask文脈preemption判定である。`sig_sem()` がWAITING taskを
 * READYへ戻し、`wait_sem_id` をclearした後にだけ呼ぶ。priority値が小さいtaskを
 * 高優先度とし、woken taskがcurrent RUNNING taskより高優先度の場合だけ
 * `dispatcher_switch_to()` へ接続する。同一優先度はまだtime slice対象にせず、
 * 低優先度と同じno-switchとして扱う。
 *
 * このhelperはtask文脈専用であり、timer IRQ handlerやdispatch pending
 * request/consume経路とは責務を混ぜない。priority順wait queue、timeout付き待ち、
 * sleep/delay queue、round-robin、完全な割り込み復帰フレーム切替も扱わない。
 *
 * @param woken_task_id READYへ戻したtask ID。
 * @param switched switchへ進んだ場合に1、no-switchの場合に0を書き込む。
 * @return 成功時はSEM_OK。dispatcher接続に失敗した場合はSEM_ERR_TASK。
 */
static int sig_sem_switch_if_woken_has_higher_priority(int woken_task_id, int *switched)
{
    const tcb_t *current = dispatcher_get_current();
    const tcb_t *woken = task_get_by_id(woken_task_id);
    tcb_t *current_mutable;
    tcb_t *woken_mutable;
    int switch_result;

    if (switched != NULL) {
        *switched = 0;
    }

    hal_console_write("[sig-sem] preempt check:");
    sig_sem_log_task_id_name(" current", current);
    if (current != NULL) {
        hal_console_write(" prio=");
        sem_write_int(current->priority);
    }
    sig_sem_log_task_id_name(" woken", woken);
    if (woken != NULL) {
        hal_console_write(" prio=");
        sem_write_int(woken->priority);
    }
    hal_console_write("\n");

    if (current == NULL || woken == NULL || current->state != TASK_STATE_RUNNING ||
        woken->state != TASK_STATE_READY) {
        /*
         * READY復帰自体は完了済みなので、比較前提が崩れてもcountは増やさない。
         * task文脈のcurrent RUNNINGがない状態ではswitchへ進まない。
         */
        hal_console_write("[sig-sem] preempt not required: reason=invalid-current-or-woken-state\n");
        return SEM_OK;
    }

    if (woken->priority >= current->priority) {
        /*
         * 同一priorityは12.4でもtime slice対象外である。低優先度wakeupと同じく
         * currentを維持し、schedulerやdispatcherへ進まない。
         */
        hal_console_write("[sig-sem] preempt not required: reason=same-or-lower-priority\n");
        return SEM_OK;
    }

    hal_console_write("[sig-sem] preempt required: reason=wakeup-higher-priority\n");

    current_mutable = task_get_mutable_by_id(current->id);
    woken_mutable = task_get_mutable_by_id(woken->id);
    if (current_mutable == NULL || woken_mutable == NULL) {
        hal_console_write("[sig-sem] switch failed: reason=task-not-found\n");
        return SEM_ERR_TASK;
    }

    hal_console_write("[sig-sem] switch begin:");
    sig_sem_log_task_id_name(" from", current);
    sig_sem_log_task_id_name(" to", woken);
    hal_console_write("\n");

    switch_result = dispatcher_switch_to(current_mutable, woken_mutable);

    hal_console_write("[sig-sem] switch end: result=");
    sem_write_int(switch_result);
    hal_console_write("\n");

    if (switch_result != DISPATCHER_OK) {
        return SEM_ERR_TASK;
    }

    if (switched != NULL) {
        *switched = 1;
    }
    return SEM_OK;
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
 * @brief 登録済みsemaphoreを読み取り専用で返す。
 *
 * @details
 * 第12章12.1の `wai_sem()` はtask文脈API層でcurrent taskを扱う。
 * semaphore moduleはID解決とcount保持だけを所有し、schedulerや
 * dispatcherには依存しない。
 *
 * @param sem_id 対象semaphore ID。
 * @return 見つかったsemaphore。存在しない場合はNULL。
 */
const semaphore_t *sem_get_by_id(int sem_id)
{
    return find_semaphore_by_id(sem_id);
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
        sem_reset_wait_queue(&semaphore_table[index]);
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
    sem_reset_wait_queue(slot);

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
 * @brief semaphore countが残っていれば1つ取得する。
 *
 * @details
 * この関数はcount操作だけを担当する。countが0の場合は待ち入りが必要な
 * 事実を `SEM_WAIT_REQUIRED` で返すだけで、RUNNING->WAITING遷移、
 * scheduler選択、dispatcher接続は `itron_api.c` の `wai_sem()` が行う。
 * これにより、semaphore moduleはwait queue、timeout、preemption、
 * interrupt、context switchの責務を持たない。
 *
 * @param sem_id 対象semaphore ID。
 * @param count_before 更新前countの格納先。NULL可。
 * @param count_after 更新後countの格納先。NULL可。
 * @return 取得成功はSEM_OK、待ち入りが必要ならSEM_WAIT_REQUIRED、失敗時はSEM_ERR_*。
 */
int sem_take_if_available(int sem_id, int *count_before, int *count_after)
{
    semaphore_t *sem = find_semaphore_by_id(sem_id);

    if (sem == NULL) {
        /*
         * 存在しないsemaphoreではcountもtask状態も動かさない。
         * 呼び出し側の `wai_sem()` がエラーとしてログ化する。
         */
        return SEM_ERR_INVAL;
    }

    if (count_before != NULL) {
        /*
         * 呼び出し側が「count 1->0」または「count=0」を説明できるよう、
         * 判定前のcountを先に返す。
         */
        *count_before = sem->count;
    }

    if (sem->count > 0) {
        /*
         * 取得できる場合はsemaphore moduleの責務であるcountだけを更新する。
         * task状態とdispatcher currentは変えず、no-switch経路を保つ。
         */
        sem->count--;
        if (count_after != NULL) {
            *count_after = sem->count;
        }
        return SEM_OK;
    }

    if (count_after != NULL) {
        *count_after = sem->count;
    }
    /*
     * count==0では待ち入りの必要性だけを返す。
     * RUNNING->WAITING化、次READY選択、dispatcher接続はAPI層の責務として残す。
     */
    return SEM_WAIT_REQUIRED;
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
/**
 * @brief 12.3のFIFO wait queueへWAITING task idを登録する。
 *
 * @details
 * `wai_sem()` がTCBをWAITINGへ更新した後に呼ぶ。semaphore moduleはqueueだけを所有し、
 * task stateの変更はtask moduleへ残す。固定長queueが満杯の場合はエラーを返し、
 * timeout待ちやpriority順への回避は行わない。
 *
 * @param sem_id 対象semaphore ID。
 * @param task_id WAITING化済みtask ID。
 * @return 成功時はSEM_OK。失敗時はSEM_ERR_*。
 */
int sem_enqueue_waiter(int sem_id, int task_id)
{
    semaphore_t *sem = find_semaphore_by_id(sem_id);
    const tcb_t *task = task_get_by_id(task_id);
    const char *task_name;

    if (sem == NULL || task == NULL || task_id <= 0) {
        return SEM_ERR_INVAL;
    }

    if (sem->wait_count >= MAX_TASKS) {
        return SEM_ERR_OVERFLOW;
    }

    /*
     * FIFO順を保つため、tailへ追加してから循環更新する。task idだけを保持し、
     * priority順制御やtimeout情報は12.3ではqueueに入れない。
     */
    sem->wait_queue[sem->wait_tail] = task_id;
    sem->wait_tail = (sem->wait_tail + 1) % MAX_TASKS;
    sem->wait_count++;

    task_name = (task->name != NULL) ? task->name : "(null)";
    hal_console_write("[sem-wq] enqueue: sem_id=");
    sem_write_int(sem_id);
    hal_console_write(" task id=");
    sem_write_int(task_id);
    hal_console_write(" name=");
    hal_console_write(task_name);
    hal_console_write(" queue_count=");
    sem_write_int(sem->wait_count);
    hal_console_write("\n");

    return SEM_OK;
}

/**
 * @brief 12.3のFIFO wait queueからWAITING task idを1件取り出す。
 *
 * @details
 * `sig_sem()` はこの関数で対象semaphoreの待ちtaskだけを取り出す。queueが空の場合は
 * count-up経路へ進めるよう、SEM_WAIT_QUEUE_EMPTYを返して `[sem-wq] empty` を出力する。
 *
 * @param sem_id 対象semaphore ID。
 * @param task_id 取り出したtask idの格納先。
 * @return 成功時はSEM_OK。空の場合はSEM_WAIT_QUEUE_EMPTY。失敗時はSEM_ERR_*。
 */
int sem_dequeue_waiter(int sem_id, int *task_id)
{
    semaphore_t *sem = find_semaphore_by_id(sem_id);
    const tcb_t *task;
    const char *task_name;
    int dequeued_task_id;

    if (sem == NULL || task_id == NULL) {
        return SEM_ERR_INVAL;
    }

    if (sem->wait_count <= 0) {
        hal_console_write("[sem-wq] empty: sem_id=");
        sem_write_int(sem_id);
        hal_console_write("\n");
        return SEM_WAIT_QUEUE_EMPTY;
    }

    /*
     * headから1件取り出して循環更新する。古いslotは0に戻し、将来の検証で
     * staleなtask idが残って見えないようにする。
     */
    dequeued_task_id = sem->wait_queue[sem->wait_head];
    sem->wait_queue[sem->wait_head] = 0;
    sem->wait_head = (sem->wait_head + 1) % MAX_TASKS;
    sem->wait_count--;
    *task_id = dequeued_task_id;

    task = task_get_by_id(dequeued_task_id);
    task_name = (task != NULL && task->name != NULL) ? task->name : "(null)";
    hal_console_write("[sem-wq] dequeue: sem_id=");
    sem_write_int(sem_id);
    hal_console_write(" task id=");
    sem_write_int(dequeued_task_id);
    hal_console_write(" name=");
    hal_console_write(task_name);
    hal_console_write(" queue_count=");
    sem_write_int(sem->wait_count);
    hal_console_write("\n");

    return SEM_OK;
}

/**
 * @brief 12.4のtask文脈sig_semとしてwakeup後preemption判定まで実行する。
 *
 * @details
 * 対象semaphoreのFIFO wait queueからWAITING taskを1件dequeueし、READYへ戻した後に
 * current RUNNING taskとのpriority比較を行う。woken taskのpriority値が小さい場合だけ
 * 既存のdispatcher switch境界へ接続する。同一priorityまたは低優先度ではswitchしない。
 * 待ちtaskがいる場合はsemaphore countを増やさず、queueが空の場合だけcount-upする。
 *
 * このAPIはtask文脈用であり、timer IRQ handlerから呼ばない。priority順wait queue、
 * timeout付き待ち、同一優先度time slice、round-robinはまだ扱わない。
 *
 * @param sem_id 対象semaphore ID。
 * @return 成功時はSEM_OK。失敗時はSEM_ERR_*。
 */
int sig_sem(int sem_id)
{
    semaphore_t *sem = find_semaphore_by_id(sem_id);
    int count_before = 0;
    int woken_task_id = 0;
    int wake_result;
    int dequeue_result;
    int preempt_result;
    int switched = 0;
    const tcb_t *waiting_task;
    const char *woken_name;

    hal_console_write("[sig-sem] called: sem_id=");
    sem_write_int(sem_id);
    hal_console_write("\n");

    /*
     * 存在しないセマフォへのsignalでは、countもtask状態も変更しない。
     * エラー時の副作用をなくし、QEMUログ上の原因切り分けを簡単にする。
     */
    if (sem == NULL) {
        hal_console_write("[sig-sem] rejected: reason=invalid-semaphore sem_id=");
        sem_write_int(sem_id);
        hal_console_write("\n");
        return SEM_ERR_INVAL;
    }

    count_before = sem->count;
    dequeue_result = sem_dequeue_waiter(sem_id, &woken_task_id);
    if (dequeue_result == SEM_OK) {
        /*
         * wakeup対象の名前をsig_semログへ含めるため、先に読み取り専用探索を行う。
         * 実際のREADY遷移はこの後もtask moduleへ委譲し、TCB直接更新はしない。
         */
        waiting_task = task_get_by_id(woken_task_id);
        if (waiting_task == NULL) {
            hal_console_write("[sig-sem] wakeup failed: reason=task-not-found task id=");
            sem_write_int(woken_task_id);
            hal_console_write("\n");
            return SEM_ERR_TASK;
        }
        woken_name = (waiting_task->name != NULL) ? waiting_task->name : "(null)";

        hal_console_write("[sig-sem] waiting task dequeued: id=");
        sem_write_int(woken_task_id);
        hal_console_write(" name=");
        hal_console_write(woken_name);
        hal_console_write(" state=");
        if (waiting_task->state == TASK_STATE_WAITING) {
            hal_console_write("WAITING");
        } else {
            hal_console_write("NOT-WAITING");
        }
        hal_console_write(" wait_sem_id=");
        sem_write_int(waiting_task->wait_sem_id);
        hal_console_write("\n");

        /*
         * 待ちtaskへ資源を渡した扱いにするため、この経路ではcountを増やさない。
         * 6.1では1 taskだけの最小wakeupで、複数待ちや順序保証は扱わない。
         */
        wake_result = task_wake_waiting_on_sem_by_id(woken_task_id, sem_id);
        if (wake_result != 0) {
            hal_console_write("[sig-sem] wakeup failed: err=");
            sem_write_int(wake_result);
            hal_console_write("\n");
            return SEM_ERR_TASK;
        }
        hal_console_write("[sig-sem] wakeup: sem_id=");
        sem_write_int(sem_id);
        hal_console_write(" task id=");
        sem_write_int(woken_task_id);
        hal_console_write(" name=");
        hal_console_write(woken_name);
        hal_console_write(" WAITING->READY\n");
        preempt_result = sig_sem_switch_if_woken_has_higher_priority(woken_task_id, &switched);
        if (preempt_result != SEM_OK) {
            hal_console_write("[sig-sem] completed: result=");
            sem_write_int(preempt_result);
            hal_console_write(" action=wakeup-switch-failed\n");
            return preempt_result;
        }

        if (switched) {
            hal_console_write("[sig-sem] completed: result=0 action=wakeup-switch\n");
        } else {
            hal_console_write("[sig-sem] completed: result=0 action=wakeup-no-switch\n");
        }
        return SEM_OK;
    }

    if (dequeue_result != SEM_WAIT_QUEUE_EMPTY) {
        hal_console_write("[sig-sem] rejected: reason=wait-queue-error err=");
        sem_write_int(dequeue_result);
        hal_console_write("\n");
        return SEM_ERR_TASK;
    }

    /*
     * 待ちtaskがいない場合だけcountを増やす。max_count超過を拒否することで、
     * semaphore_tの不変条件をsig_sem側でも維持する。
     */
    hal_console_write("[sig-sem] no waiting task: sem_id=");
    sem_write_int(sem_id);
    hal_console_write("\n");

    if (sem->count >= sem->max_count) {
        hal_console_write("[sig-sem] rejected: reason=count-overflow sem_id=");
        sem_write_int(sem->id);
        hal_console_write(" count=");
        sem_write_int(count_before);
        hal_console_write(" max_count=");
        sem_write_int(sem->max_count);
        hal_console_write(" wait_queue_count=");
        sem_write_int(sem->wait_count);
        hal_console_write("\n");
        return SEM_ERR_OVERFLOW;
    }

    sem->count++;
    hal_console_write("[sig-sem] count incremented: sem_id=");
    sem_write_int(sem->id);
    hal_console_write(" count ");
    sem_write_int(count_before);
    hal_console_write("->");
    sem_write_int(sem->count);
    hal_console_write("\n");
    hal_console_write("[sig-sem] completed: result=0 action=count-up\n");
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
        hal_console_write(" wait_queue_count=");
        sem_write_int(sem->wait_count);
        hal_console_write("\n");
    }

    hal_console_write("[sem] dump end\n");
}
