// Prototype scenario n2_after_end, not for merging: a signal ends W's condition wait with
// callbacks between the two steps of a callback notify. With VITA3K_PROTO_SCENARIO=n2_after_end the
// notify's second step waits until W's wait ended, so it must find it over and leave it. W relocks
// the mutex, its callback runs once at the relock's loop top, and W itself sees that it owns the
// mutex. Heavy and lightweight conditions.
#include <psp2/kernel/threadmgr.h>

#include "test.h"
#include "threadmgr_extra.h"

static int lightweight;
static SceUID mutex, cond, callback;
static SceKernelLwMutexWork lw_mutex;
static SceKernelLwCondWork lw_cond;
static volatile int callback_runs, w_ready, notified, w_owned;
static volatile int w_result;

static int callback_entry(int notify_id, int count, int arg, void *common) {
    (void)notify_id;
    (void)count;
    (void)arg;
    (void)common;
    callback_runs++;
    return 0;
}

static SceUID owner(void) {
    if (!lightweight) {
        SceKernelMutexInfo info = { .size = sizeof(info) };
        sceKernelGetMutexInfo(mutex, &info);
        return info.currentOwnerId;
    }
    LwMutexInfo info = { .size = sizeof(info) };
    sceKernelGetLwMutexInfo(&lw_mutex, &info);
    return info.currentOwnerId;
}

static int w_entry(SceSize args, void *argp) {
    (void)args;
    (void)argp;
    callback = sceKernelCreateCallback("cb", 0, callback_entry, NULL);
    if (lightweight)
        sceKernelLockLwMutex(&lw_mutex, 1, NULL);
    else
        sceKernelLockMutex(mutex, 1, NULL);
    w_ready = 1;
    w_result = lightweight ? sceKernelWaitLwCondCB(&lw_cond, NULL) : sceKernelWaitCondCB(cond, NULL);
    w_owned = owner() == sceKernelGetThreadId();
    if (lightweight)
        sceKernelUnlockLwMutex(&lw_mutex, 1);
    else
        sceKernelUnlockMutex(mutex, 1);
    return 0;
}

static int notifier_entry(SceSize args, void *argp) {
    (void)args;
    (void)argp;
    sceKernelNotifyCallback(callback, 1);
    notified = 1;
    return 0;
}

static void run_case(const char *name, int lw) {
    lightweight = lw;
    callback_runs = 0;
    w_ready = 0;
    notified = 0;
    w_owned = 0;
    w_result = -1;
    if (lw) {
        sceKernelCreateLwMutex(&lw_mutex, "m", 0, 0, NULL);
        sceKernelCreateLwCond(&lw_cond, "c", 0, &lw_mutex, NULL);
    } else {
        mutex = sceKernelCreateMutex("m", 0, 0, NULL);
        cond = sceKernelCreateCond("c", 0, mutex, NULL);
    }
    const SceUID w = test_start_thread(w_entry);
    TEST_WAIT_FOR(w_ready);
    test_wait_until_waiting(w);
    const SceUID notifier = test_start_thread(notifier_entry);
    sceKernelDelayThread(100000);
    PRINTF(notified, "%s: the notify returned before the signal", name);
    PRINTF(lw ? sceKernelSignalLwCond(&lw_cond) : sceKernelSignalCond(cond), "%s: the signal", name);
    test_join_thread(w);
    TEST_WAIT_FOR(notified);
    test_join_thread(notifier);
    PRINTF(w_result, "%s: W's wait", name);
    PRINTF(callback_runs, "%s: W's callback runs", name);
    PRINTF(w_owned, "%s: W owned the mutex after its wait", name);
    if (lw) {
        sceKernelDeleteLwCond(&lw_cond);
        sceKernelDeleteLwMutex(&lw_mutex);
    } else {
        sceKernelDeleteCond(cond);
        sceKernelDeleteMutex(mutex);
    }
}

int main(void) {
    run_case("heavy", 0);
    run_case("lightweight", 1);
    test_exit();
    return 0;
}
