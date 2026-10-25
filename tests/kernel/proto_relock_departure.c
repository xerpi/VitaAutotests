// Prototype check, not for merging: a callback notify takes a condition waiter that relocks the
// mutex out of the mutex's queue. W waits with callbacks on a condition, and the main thread locks
// the mutex, signals and waits until W reports the relock wait on the mutex. It then notifies W's
// callback, and W must be out of the mutex's queue when the notify returns. W's callback blocks on
// a gate, so W can't be back in the queue before the main thread looks. The main thread waits for
// the callback's marker, opens the gate, then waits until W is queued on the mutex again, since the
// wait type and UID alone don't tell the retry from the first relock, and unlocks. W's condition
// wait returns 0 with its callback run once, and W itself sees that it owns the mutex. Heavy and
// lightweight.
#include <psp2/kernel/threadmgr.h>

#include "test.h"
#include "threadmgr_extra.h"

enum { HEAVY, LIGHT };

static int kind;
static SceUID mutex, cond, callback, gate;
static SceKernelLwMutexWork lw_mutex;
static SceKernelLwCondWork lw_cond;
static volatile int callback_runs, w_ready, w_owned, w_done;
static volatile int w_result;

static int callback_entry(int notify_id, int count, int arg, void *common) {
    (void)notify_id;
    (void)count;
    (void)arg;
    (void)common;
    callback_runs++;
    sceKernelWaitSema(gate, 1, NULL);
    return 0;
}

static SceUID owner(void) {
    if (kind == HEAVY) {
        SceKernelMutexInfo info = { .size = sizeof(info) };
        sceKernelGetMutexInfo(mutex, &info);
        return info.currentOwnerId;
    }
    LwMutexInfo info = { .size = sizeof(info) };
    sceKernelGetLwMutexInfo(&lw_mutex, &info);
    return info.currentOwnerId;
}

static SceUInt32 mutex_waiters(void) {
    if (kind == HEAVY) {
        SceKernelMutexInfo info = { .size = sizeof(info) };
        sceKernelGetMutexInfo(mutex, &info);
        return info.numWaitThreads;
    }
    LwMutexInfo info = { .size = sizeof(info) };
    sceKernelGetLwMutexInfo(&lw_mutex, &info);
    return info.numWaitThreads;
}

static int in_wait(SceUID thread, SceUInt32 type) {
    SceKernelThreadInfo info = { .size = sizeof(info) };
    return sceKernelGetThreadInfo(thread, &info) == 0 && (info.waitType & ~WAIT_TYPE_CB) == type;
}

static int w_entry(SceSize args, void *argp) {
    (void)args;
    (void)argp;
    callback = sceKernelCreateCallback("cb", 0, callback_entry, NULL);
    if (kind == HEAVY)
        sceKernelLockMutex(mutex, 1, NULL);
    else
        sceKernelLockLwMutex(&lw_mutex, 1, NULL);
    w_ready = 1;
    w_result = kind == HEAVY ? sceKernelWaitCondCB(cond, NULL) : sceKernelWaitLwCondCB(&lw_cond, NULL);
    w_owned = owner() == sceKernelGetThreadId();
    if (kind == HEAVY)
        sceKernelUnlockMutex(mutex, 1);
    else
        sceKernelUnlockLwMutex(&lw_mutex, 1);
    w_done = 1;
    return 0;
}

static void run_case(const char *name, int k) {
    kind = k;
    callback_runs = 0;
    w_ready = 0;
    w_owned = 0;
    w_done = 0;
    w_result = -1;
    gate = sceKernelCreateSema("gate", 0, 0, 1, NULL);
    if (k == HEAVY) {
        mutex = sceKernelCreateMutex("m", 0, 0, NULL);
        cond = sceKernelCreateCond("c", 0, mutex, NULL);
    } else {
        sceKernelCreateLwMutex(&lw_mutex, "m", 0, 0, NULL);
        sceKernelCreateLwCond(&lw_cond, "c", 0, &lw_mutex, NULL);
    }
    const SceUInt32 cond_wait = k == HEAVY ? WAIT_TYPE_COND : WAIT_TYPE_LW_COND;
    const SceUInt32 relock_wait = k == HEAVY ? WAIT_TYPE_COND_MUTEX : WAIT_TYPE_LW_COND_MUTEX;
    const SceUID w = test_start_thread(w_entry);
    TEST_WAIT_FOR(w_ready && in_wait(w, cond_wait));
    if (k == HEAVY) {
        sceKernelLockMutex(mutex, 1, NULL);
        PRINTF(sceKernelSignalCond(cond), "%s: signal", name);
    } else {
        sceKernelLockLwMutex(&lw_mutex, 1, NULL);
        PRINTF(sceKernelSignalLwCond(&lw_cond), "%s: signal", name);
    }
    TEST_WAIT_FOR(in_wait(w, relock_wait));
    PRINTF(in_wait(w, relock_wait), "%s: W relocks", name);
    PRINTF(mutex_waiters(), "%s: the mutex's waiters before the notify", name);
    PRINTF(sceKernelNotifyCallback(callback, 1), "%s: notify W", name);
    PRINTF(mutex_waiters(), "%s: the mutex's waiters after the notify", name);
    TEST_WAIT_FOR(callback_runs == 1);
    PRINTF(callback_runs, "%s: W's callback ran", name);
    PRINTF(mutex_waiters(), "%s: the mutex's waiters while the callback runs", name);
    sceKernelSignalSema(gate, 1);
    TEST_WAIT_FOR(mutex_waiters() == 1);
    PRINTF(mutex_waiters(), "%s: the mutex's waiters after the callback", name);
    PRINTF(in_wait(w, relock_wait), "%s: W relocks again", name);
    PRINTF(k == HEAVY ? sceKernelUnlockMutex(mutex, 1) : sceKernelUnlockLwMutex(&lw_mutex, 1), "%s: unlock", name);
    TEST_WAIT_FOR(w_done);
    test_join_thread(w);
    PRINTF(w_result, "%s: W's condition wait", name);
    PRINTF(callback_runs, "%s: W's callback runs", name);
    PRINTF(w_owned, "%s: W owned the mutex", name);
    sceKernelDeleteSema(gate);
    if (k == HEAVY) {
        sceKernelDeleteCond(cond);
        sceKernelDeleteMutex(mutex);
    } else {
        sceKernelDeleteLwCond(&lw_cond);
        sceKernelDeleteLwMutex(&lw_mutex);
    }
}

int main(void) {
    run_case("heavy", HEAVY);
    run_case("lightweight", LIGHT);

    test_exit();
    return 0;
}
