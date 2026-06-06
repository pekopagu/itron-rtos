/*
 * SPDX-License-Identifier: MIT
 */

/**
 * @file delay_queue.c
 * @brief 13.2 sleep/delay queueの固定長観測モデル。
 *
 * @details
 * `dly_tsk()` でdelay WAITINGへ入ったtaskを、semaphore wait queueとは別の
 * 固定長queueに登録する。13.4ではtimer tickごとにremaining tickを減算し、
 * timeout到達taskをREADYへ戻す。ただしtimer IRQ handler本体から直接dispatcherへは進まない。
 */

#include <stddef.h>

#include "delay_queue.h"
#include "hal/console.h"
#include "semaphore.h"
#include "task.h"

/**
 * @struct delay_queue_entry_t
 * @brief delay queue内の1 entry。
 *
 * @details
 * 13.2ではtask idとremaining tickだけをqueue側に保持する。
 * task名、state、wait reasonはdump時にtask tableから読み直すことで、
 * queue管理とtask状態管理の責務を分離する。
 */
typedef struct {
    int task_id;                       /**< delay WAITING taskのID。0は未使用entry。 */
    uint32_t delay_ticks_remaining;    /**< 13.4ではtickごとに減算するremaining tick。 */
} delay_queue_entry_t;

static delay_queue_entry_t delay_queue_entries[MAX_TASKS];
static int delay_queue_count;

/**
 * @brief 符号なし整数をHAL consoleへ10進出力する。
 *
 * @param value 出力する値。
 *
 * @note 表示専用helperであり、queueやtask状態は変更しない。
 */
static void delay_queue_write_uint(unsigned long value)
{
    char buffer[20];
    int index = 0;

    /* 0は変換loopに入らないため、明示的に1文字だけ出力する。 */
    if (value == 0UL) {
        hal_console_putc('0');
        return;
    }

    /* 下位桁からbufferへ積み、後で逆順に出力できる形にする。 */
    while (value > 0UL) {
        buffer[index++] = (char)('0' + (value % 10UL));
        value /= 10UL;
    }

    /* buffer内の逆順桁を末尾から戻しながら通常の桁順で出力する。 */
    while (index > 0) {
        hal_console_putc(buffer[--index]);
    }
}

/**
 * @brief 符号付き整数をHAL consoleへ10進出力する。
 *
 * @param value 出力する値。
 *
 * @note 表示専用helperであり、TCBやqueue entryは変更しない。
 */
static void delay_queue_write_int(int value)
{
    /* 負値は符号を先に出し、絶対値部分を共通のunsigned出力へ委譲する。 */
    if (value < 0) {
        hal_console_putc('-');
        delay_queue_write_uint((unsigned long)(-value));
        return;
    }

    /* 非負値はそのままunsigned出力へ委譲し、数値表示の実装を一箇所に保つ。 */
    delay_queue_write_uint((unsigned long)value);
}

/**
 * @brief task状態をログ用文字列へ変換する。
 *
 * @param state 変換対象のtask状態。
 * @return 固定文字列。
 *
 * @note 未知の値はdump継続のため `UNKNOWN` として扱う。
 */
static const char *delay_queue_task_state_name(task_state_t state)
{
    /* dumpログでは数値ではなく固定文字列にして、観測結果を読みやすくする。 */
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
 * @brief WAITING理由をログ用文字列へ変換する。
 *
 * @param reason 変換対象のWAITING理由。
 * @return 固定文字列。
 *
 * @note 未知の値はdump継続のため `unknown` として扱う。
 */
static const char *delay_queue_wait_reason_name(task_wait_reason_t reason)
{
    /* semaphore待ちとdelay待ちの混同を避けるため、wait reasonを文字列で出す。 */
    switch (reason) {
    case TASK_WAIT_REASON_NONE:
        return "none";
    case TASK_WAIT_REASON_SEMAPHORE:
        return "semaphore";
    case TASK_WAIT_REASON_DELAY:
        return "delay";
    case TASK_WAIT_REASON_SEMAPHORE_TIMEOUT:
        return "semaphore-timeout";
    default:
        return "unknown";
    }
}

/**
 * @brief 指定taskがqueue内に存在するかを線形探索する。
 *
 * @param task_id 探索対象task ID。
 * @return 見つかったentry index。存在しなければ -1。
 *
 * @note priority順queueやdelta queue最適化は13.2の責務ではない。
 */
static int delay_queue_find_index_by_task_id(int task_id)
{
    int index;

    /*
     * delay queueは13.2時点では固定長の小さな観測queueなので、単純な線形探索にする。
     * 重複検出だけを担い、entry順序の最適化は将来章に残す。
     */
    for (index = 0; index < delay_queue_count; index++) {
        /* task idが一致したentryを見つけたら、そのindexを返して呼び出し側で判定できるようにする。 */
        if (delay_queue_entries[index].task_id == task_id) {
            return index;
        }
    }

    /* 見つからない場合は未登録taskとして扱えるように -1 を返す。 */
    return -1;
}

int delay_queue_init(void)
{
    int index;

    /* 起動ごとに観測entryを消去し、semaphore wait queueとは独立した空queueへ戻す。 */
    for (index = 0; index < MAX_TASKS; index++) {
        delay_queue_entries[index].task_id = 0;
        delay_queue_entries[index].delay_ticks_remaining = 0U;
    }

    delay_queue_count = 0;
    hal_console_write("[delay-q] initialized\n");
    return DELAY_QUEUE_OK;
}

int delay_queue_can_enqueue(int task_id)
{
    /* task id 0以下は登録済みtaskを指さないため、queue状態を見ずに不正入力として扱う。 */
    if (task_id <= 0) {
        return DELAY_QUEUE_ERR_INVAL;
    }

    /* 同じtaskを二重にqueueへ入れると将来のdecrement設計で状態が分裂するため拒否する。 */
    if (delay_queue_find_index_by_task_id(task_id) >= 0) {
        return DELAY_QUEUE_ERR_DUPLICATE;
    }

    /* 13.2では固定長queueだけを導入し、動的拡張は行わない。 */
    if (delay_queue_count >= MAX_TASKS) {
        return DELAY_QUEUE_ERR_FULL;
    }

    return DELAY_QUEUE_OK;
}

int delay_queue_enqueue(int task_id, uint32_t delay_ticks)
{
    const tcb_t *task = task_get_by_id(task_id);
    int can_enqueue_result;

    /*
     * enqueue本体でも入力を再確認する。
     * dly_tsk()側の事前確認だけに依存せず、不正なtick値や存在しないtaskを拒否する。
     */
    if (delay_ticks == 0U || task == NULL) {
        return DELAY_QUEUE_ERR_INVAL;
    }

    /*
     * WAITING化後の防御として、delay待ちまたはtimeout付きsemaphore待ちだけをdelay queueへ入れる。
     * 通常semaphore待ちを受け入れるとtimeout観測queueとsemaphore wait queueの境界が崩れる。
     */
    if (task->state != TASK_STATE_WAITING ||
        (task->wait_reason != TASK_WAIT_REASON_DELAY &&
         task->wait_reason != TASK_WAIT_REASON_SEMAPHORE_TIMEOUT)) {
        hal_console_write("[delay-q] enqueue failed: reason=not-delay-compatible-waiter task id=");
        delay_queue_write_int(task_id);
        hal_console_write("\n");
        return DELAY_QUEUE_ERR_TASK_STATE;
    }

    can_enqueue_result = delay_queue_can_enqueue(task_id);
    /*
     * 事前確認後に呼び出し順が変わった場合でも、満杯と重複はqueue側の責務としてログに残す。
     * 現在は単一CPUのboot-time modelだが、将来の拡張前提として防御を二重に置く。
     */
    if (can_enqueue_result == DELAY_QUEUE_ERR_FULL) {
        hal_console_write("[delay-q] enqueue failed: reason=full task id=");
        delay_queue_write_int(task_id);
        hal_console_write(" name=");
        hal_console_write((task->name != NULL) ? task->name : "(null)");
        hal_console_write(" delay_ticks=");
        delay_queue_write_uint(delay_ticks);
        hal_console_write("\n");
        return can_enqueue_result;
    }

    /*
     * 二重enqueueはremaining tickを二重に観測してしまうため拒否する。
     * 13.2ではdequeueがないので、同一taskの再登録は明確な不整合として扱う。
     */
    if (can_enqueue_result == DELAY_QUEUE_ERR_DUPLICATE) {
        hal_console_write("[delay-q] enqueue failed: reason=duplicate task id=");
        delay_queue_write_int(task_id);
        hal_console_write(" name=");
        hal_console_write((task->name != NULL) ? task->name : "(null)");
        hal_console_write(" delay_ticks=");
        delay_queue_write_uint(delay_ticks);
        hal_console_write("\n");
        return can_enqueue_result;
    }

    /* その他の事前確認失敗は呼び出し側でactionを決められるように、そのまま返す。 */
    if (can_enqueue_result != DELAY_QUEUE_OK) {
        return can_enqueue_result;
    }

    /* FIFO順の観測queueとして末尾へ追加する。priority順やdelta queue化は13.2では行わない。 */
    delay_queue_entries[delay_queue_count].task_id = task_id;
    delay_queue_entries[delay_queue_count].delay_ticks_remaining = delay_ticks;
    delay_queue_count++;

    hal_console_write("[delay-q] enqueue: task id=");
    delay_queue_write_int(task_id);
    hal_console_write(" name=");
    hal_console_write((task->name != NULL) ? task->name : "(null)");
    hal_console_write(" delay_ticks=");
    delay_queue_write_uint(delay_ticks);
    hal_console_write(" queue_count=");
    delay_queue_write_int(delay_queue_count);
    hal_console_write("\n");

    return DELAY_QUEUE_OK;
}

void delay_queue_dump(void)
{
    int index;

    /* dumpの開始とcountを先に出し、queue全体の観測範囲をログ上で区切る。 */
    hal_console_write("[delay-q] dump begin: count=");
    delay_queue_write_int(delay_queue_count);
    hal_console_write("\n");

    /*
     * queue entryごとにtask tableを読み直す。
     * queueはtask idとremainingだけを持ち、state/reason/nameの所有権はtask moduleに残す。
     */
    for (index = 0; index < delay_queue_count; index++) {
        const delay_queue_entry_t *entry = &delay_queue_entries[index];
        const tcb_t *task = task_get_by_id(entry->task_id);

        /* indexとtask idを先に出し、後続の詳細が欠落してもqueue上の位置を追跡できるようにする。 */
        hal_console_write("[delay-q] entry: index=");
        delay_queue_write_int(index);
        hal_console_write(" task id=");
        delay_queue_write_int(entry->task_id);

        /*
         * task table側にtaskが見つからない場合でもdumpを止めない。
         * 13.2では削除APIは未実装だが、観測ログとしてqueue側のentryを最後まで出す。
         */
        if (task == NULL) {
            hal_console_write(" name=(missing) remaining=");
            delay_queue_write_uint(entry->delay_ticks_remaining);
            hal_console_write(" reason=unknown state=UNKNOWN\n");
            continue;
        }

        /* task tableから読み直した名前、理由、状態を出し、delay queueとtask状態の整合性を確認する。 */
        hal_console_write(" name=");
        hal_console_write((task->name != NULL) ? task->name : "(null)");
        hal_console_write(" remaining=");
        delay_queue_write_uint(entry->delay_ticks_remaining);
        hal_console_write(" reason=");
        hal_console_write(delay_queue_wait_reason_name(task->wait_reason));
        hal_console_write(" state=");
        hal_console_write(delay_queue_task_state_name(task->state));
        hal_console_write("\n");
    }

    /* dumpの終端を明示し、後続のscheduler/dispatcherログと混ざらないようにする。 */
    hal_console_write("[delay-q] dump end\n");
}

/**
 * @brief delay queueのentryを指定indexから削除する。
 *
 * @param remove_index 削除するentry index。
 * @note 固定長配列内で後続entryを1つずつ詰める。delta queue最適化は行わない。
 */
static void delay_queue_remove_at(int remove_index)
{
    int index;

    /* 削除位置より後ろのentryを前へ詰め、queue順を保ったままcountを減らす。 */
    for (index = remove_index; index < delay_queue_count - 1; index++) {
        delay_queue_entries[index] = delay_queue_entries[index + 1];
    }

    /* 末尾に残る古いentryを消して、後続dumpでstale値が見えないようにする。 */
    if (delay_queue_count > 0) {
        delay_queue_entries[delay_queue_count - 1].task_id = 0;
        delay_queue_entries[delay_queue_count - 1].delay_ticks_remaining = 0U;
        delay_queue_count--;
    }
}

/**
 * @brief timeout付きsemaphore待ちtaskをdelay queueから削除する。
 *
 * @details
 * `sig_sem()` がtimeout付きsemaphore待ちtaskをREADYへ戻す前に呼ぶ。
 * delay queueはtask idとremaining tickだけを保持するため、task tableから
 * wait reasonを読み直してsemaphore-timeout待ちだけを削除する。
 *
 * @param task_id 削除対象task ID。
 * @return 成功時はDELAY_QUEUE_OK。失敗時はDELAY_QUEUE_ERR_*。
 */
int delay_queue_remove_sem_timeout_waiter(int task_id)
{
    int index;
    tcb_t *task = task_get_mutable_by_id(task_id);
    const char *task_name = (task != NULL && task->name != NULL) ? task->name : "(null)";

    /* 不正IDや未登録taskではqueueを変更しない。 */
    if (task_id <= 0 || task == NULL) {
        return DELAY_QUEUE_ERR_INVAL;
    }

    /*
     * timeout付きsemaphore待ちだけをsig_sem()側削除の対象にする。
     * delay待ちやsleep待ちをここで削除すると別APIの待ち状態を壊す。
     */
    if (task->state != TASK_STATE_WAITING ||
        task->wait_reason != TASK_WAIT_REASON_SEMAPHORE_TIMEOUT) {
        hal_console_write("[delay-q] remove rejected: task id=");
        delay_queue_write_int(task_id);
        hal_console_write(" name=");
        hal_console_write(task_name);
        hal_console_write(" reason=");
        hal_console_write(delay_queue_wait_reason_name(task->wait_reason));
        hal_console_write(" state=");
        hal_console_write(delay_queue_task_state_name(task->state));
        hal_console_write("\n");
        return DELAY_QUEUE_ERR_TASK_STATE;
    }

    /* task idでdelay queue entryを探す。 */
    index = delay_queue_find_index_by_task_id(task_id);
    if (index < 0) {
        return DELAY_QUEUE_ERR_INVAL;
    }

    /* 見つかったentryだけを削除し、後続entryを詰める。 */
    hal_console_write("[delay-q] remove: task id=");
    delay_queue_write_int(task_id);
    hal_console_write(" name=");
    hal_console_write(task_name);
    hal_console_write(" reason=semaphore-timeout");
    delay_queue_remove_at(index);
    hal_console_write(" queue_count=");
    delay_queue_write_int(delay_queue_count);
    hal_console_write("\n");

    return DELAY_QUEUE_OK;
}

/**
 * @brief timer tickに合わせてdelay queue上のremaining tickを1つ進める。
 *
 * @details
 * 13.4のtick到達READY復帰モデルである。queue上の各entryを順に確認し、
 * remaining tickを1減算する。0になったdelay待ちtaskはREADYへ戻し、
 * timeout付きsemaphore待ちtaskは対象semaphore wait queueから削除してからREADYへ戻す。
 *
 * この関数はtimer IRQ handlerから呼ばれても直接dispatcherへ進まない。
 * READY復帰後のpreemption判定とdispatch pending設定は、呼び出し側の後続境界へ委譲する。
 *
 * @return timeout到達として処理したentry数。
 */
int delay_queue_tick(void)
{
    int index = 0;
    int expired_count = 0;

    /* tick処理の開始時点でqueue件数を出し、今回のtickがどの待ち集合を対象にしたかを観測する。 */
    hal_console_write("[delay-q] tick begin: count=");
    delay_queue_write_int(delay_queue_count);
    hal_console_write("\n");

    /*
     * 削除時は同じindexへ次entryが詰められるため、timeout処理した場合はindexを進めない。
     * remainingが残るentryだけ次indexへ進める。
     */
    while (index < delay_queue_count) {
        delay_queue_entry_t *entry = &delay_queue_entries[index];
        tcb_t *task = task_get_mutable_by_id(entry->task_id);
        uint32_t before = entry->delay_ticks_remaining;
        uint32_t after = (before > 0U) ? (before - 1U) : 0U;
        const char *task_name = (task != NULL && task->name != NULL) ? task->name : "(null)";
        task_wait_reason_t reason = (task != NULL) ? task->wait_reason : TASK_WAIT_REASON_NONE;

        /* queue entryとTCBのremaining観測値を同じtick結果へ揃える。 */
        entry->delay_ticks_remaining = after;
        if (task != NULL) {
            task->delay_ticks_remaining = after;
        }

        hal_console_write("[delay-q] tick entry: task id=");
        delay_queue_write_int(entry->task_id);
        hal_console_write(" name=");
        hal_console_write(task_name);
        hal_console_write(" remaining ");
        delay_queue_write_uint(before);
        hal_console_write("->");
        delay_queue_write_uint(after);
        hal_console_write(" reason=");
        hal_console_write(delay_queue_wait_reason_name(reason));
        hal_console_write(" state=");
        hal_console_write((task != NULL) ? delay_queue_task_state_name(task->state) : "UNKNOWN");
        hal_console_write("\n");

        if (after > 0U) {
            index++;
            continue;
        }

        expired_count++;
        hal_console_write("[delay-q] timeout reached: task id=");
        delay_queue_write_int(entry->task_id);
        hal_console_write(" name=");
        hal_console_write(task_name);
        hal_console_write(" reason=");
        hal_console_write(delay_queue_wait_reason_name(reason));
        if (task != NULL && reason == TASK_WAIT_REASON_SEMAPHORE_TIMEOUT) {
            hal_console_write(" sem id=");
            delay_queue_write_int(task->wait_sem_id);
        }
        hal_console_write("\n");

        /*
         * timeout付きsemaphore待ちはsig_sem()のwakeup対象queueにも残っているため、
         * READY復帰前に対象semaphore wait queueから取り除く。
         */
        if (task != NULL && reason == TASK_WAIT_REASON_SEMAPHORE_TIMEOUT) {
            int sem_remove_result = sem_remove_waiter(task->wait_sem_id, task->id);
            if (sem_remove_result == SEM_OK) {
                (void)task_wake_waiting_on_sem_timeout_by_id(task->id, task->wait_sem_id);
            } else {
                hal_console_write("[delay-q] semaphore timeout cleanup failed: task id=");
                delay_queue_write_int(task->id);
                hal_console_write(" err=");
                delay_queue_write_int(sem_remove_result);
                hal_console_write("\n");
            }
        } else if (task != NULL && reason == TASK_WAIT_REASON_DELAY) {
            (void)task_wake_waiting_on_delay_by_id(task->id);
        } else {
            hal_console_write("[delay-q] timeout skipped: reason=invalid-task-state task id=");
            delay_queue_write_int(entry->task_id);
            hal_console_write("\n");
        }

        /*
         * timeout到達entryはdelay queueから削除する。
         * task状態復帰に失敗した場合も、0 tick entryを残して毎tick再処理しない。
         */
        hal_console_write("[delay-q] remove: task id=");
        delay_queue_write_int(entry->task_id);
        hal_console_write(" name=");
        hal_console_write(task_name);
        hal_console_write(" reason=");
        hal_console_write(delay_queue_wait_reason_name(reason));
        delay_queue_remove_at(index);
        hal_console_write(" queue_count=");
        delay_queue_write_int(delay_queue_count);
        hal_console_write("\n");
    }

    hal_console_write("[delay-q] tick end: expired=");
    delay_queue_write_int(expired_count);
    hal_console_write(" count=");
    delay_queue_write_int(delay_queue_count);
    hal_console_write("\n");

    return expired_count;
}
