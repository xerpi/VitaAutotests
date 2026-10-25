// Prototype scenario departure_before_cleanup, not for merging: the main thread holds a read lock,
// W waits for the write lock with callbacks, and R waits for a read lock behind W. The main thread
// notifies W. With VITA3K_PROTO_SCENARIO=departure_before_cleanup W does no cleanup of its own
// until the rwlock has no waiter left and two read locks, so R getting in can only come from the
// notify's departure and serve. R keeps its read lock until a gate opens, and W's callback blocks
// on another gate, so the main thread reads the info before W can wait again. Then it lets the
// callback return, waits until W waits for the write lock again, and it and R unlock, so W gets the
// lock.
#include <psp2/kernel/threadmgr.h>

#include "test.h"

static SceUID rwlock, callback, r_gate, cb_gate;
static volatile int callback_runs, w_ready, r_locked;
static volatile int w_result = -1, r_result = -1;

static int callback_entry(int notify_id, int count, int arg, void *common) {
    (void)notify_id;
    (void)count;
    (void)arg;
    (void)common;
    callback_runs++;
    sceKernelWaitSema(cb_gate, 1, NULL);
    return 0;
}

static int w_entry(SceSize args, void *argp) {
    (void)args;
    (void)argp;
    callback = sceKernelCreateCallback("cb", 0, callback_entry, NULL);
    w_ready = 1;
    w_result = sceKernelLockWriteRWLockCB(rwlock, NULL);
    if (w_result == 0)
        sceKernelUnlockWriteRWLock(rwlock);
    return 0;
}

static int r_entry(SceSize args, void *argp) {
    (void)args;
    (void)argp;
    r_result = sceKernelLockReadRWLock(rwlock, NULL);
    r_locked = 1;
    sceKernelWaitSema(r_gate, 1, NULL);
    if (r_result == 0)
        sceKernelUnlockReadRWLock(rwlock);
    return 0;
}

static SceKernelRWLockInfo rwlock_info(void) {
    SceKernelRWLockInfo info = { .size = sizeof(info) };
    sceKernelGetRWLockInfo(rwlock, &info);
    return info;
}

int main(void) {
    rwlock = sceKernelCreateRWLock("rw", 0, NULL);
    r_gate = sceKernelCreateSema("r gate", 0, 0, 1, NULL);
    cb_gate = sceKernelCreateSema("cb gate", 0, 0, 1, NULL);
    PRINT("read lock", sceKernelLockReadRWLock(rwlock, NULL));
    const SceUID w = test_start_thread(w_entry);
    TEST_WAIT_FOR(w_ready);
    test_wait_until_waiting(w);
    const SceUID r = test_start_thread(r_entry);
    test_wait_until_waiting(r);
    PRINT("read waiters before the notify", rwlock_info().numReadWaitThreads);
    PRINT("write waiters before the notify", rwlock_info().numWriteWaitThreads);
    PRINT("notify W", sceKernelNotifyCallback(callback, 1));
    TEST_WAIT_FOR(r_locked && callback_runs == 1);
    PRINT("R's lock", r_result);
    PRINT("W's callback ran", callback_runs);
    const SceKernelRWLockInfo during = rwlock_info();
    PRINT("the lock count while W's callback runs", during.lockCount);
    PRINT("the write owner while W's callback runs", during.writeOwnerId);
    PRINT("read waiters while W's callback runs", during.numReadWaitThreads);
    PRINT("write waiters while W's callback runs", during.numWriteWaitThreads);
    sceKernelSignalSema(cb_gate, 1);
    TEST_WAIT_FOR(rwlock_info().numWriteWaitThreads == 1);
    PRINT("write waiters after W's callback", rwlock_info().numWriteWaitThreads);
    sceKernelSignalSema(r_gate, 1);
    PRINT("read unlock", sceKernelUnlockReadRWLock(rwlock));
    test_join_thread(w);
    test_join_thread(r);
    PRINT("W's lock", w_result);
    PRINT("W's callback runs", callback_runs);
    PRINT("the lock count after", rwlock_info().lockCount);

    test_exit();
    return 0;
}
