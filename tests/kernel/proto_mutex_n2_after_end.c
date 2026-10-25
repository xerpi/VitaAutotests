// Prototype scenario n2_after_end, not for merging: an unlock hands the mutex to W, which waits
// with callbacks, between the two steps of a callback notify. With
// VITA3K_PROTO_SCENARIO=n2_after_end the notify's second step waits until W's wait ended, so it
// must find it over and leave it. W gets the mutex, its callback runs once in the final check, and
// W itself sees that it owns the mutex. Heavy and lightweight mutexes.
#include <psp2/kernel/threadmgr.h>

#include "test.h"
#include "threadmgr_extra.h"

enum { HEAVY, LIGHT };

static int kind;
static SceUID mutex, callback;
static SceKernelLwMutexWork lw_mutex;
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
    if (kind == HEAVY) {
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
    w_ready = 1;
    w_result = kind == HEAVY ? sceKernelLockMutexCB(mutex, 1, NULL) : sceKernelLockLwMutexCB(&lw_mutex, 1, NULL);
    w_owned = owner() == sceKernelGetThreadId();
    if (kind == HEAVY)
        sceKernelUnlockMutex(mutex, 1);
    else
        sceKernelUnlockLwMutex(&lw_mutex, 1);
    return 0;
}

static int notifier_entry(SceSize args, void *argp) {
    (void)args;
    (void)argp;
    sceKernelNotifyCallback(callback, 1);
    notified = 1;
    return 0;
}

static void run_case(const char *name, int k) {
    kind = k;
    callback_runs = 0;
    w_ready = 0;
    notified = 0;
    w_owned = 0;
    w_result = -1;
    if (k == HEAVY) {
        mutex = sceKernelCreateMutex("m", 0, 0, NULL);
        sceKernelLockMutex(mutex, 1, NULL);
    } else {
        sceKernelCreateLwMutex(&lw_mutex, "m", 0, 0, NULL);
        sceKernelLockLwMutex(&lw_mutex, 1, NULL);
    }
    const SceUID w = test_start_thread(w_entry);
    TEST_WAIT_FOR(w_ready);
    test_wait_until_waiting(w);
    const SceUID notifier = test_start_thread(notifier_entry);
    sceKernelDelayThread(100000);
    PRINTF(notified, "%s: the notify returned before the unlock", name);
    PRINTF(k == HEAVY ? sceKernelUnlockMutex(mutex, 1) : sceKernelUnlockLwMutex(&lw_mutex, 1), "%s: unlock", name);
    test_join_thread(w);
    TEST_WAIT_FOR(notified);
    test_join_thread(notifier);
    PRINTF(w_result, "%s: W's lock", name);
    PRINTF(callback_runs, "%s: W's callback runs", name);
    PRINTF(w_owned, "%s: W owned the mutex", name);
    if (k == HEAVY)
        sceKernelDeleteMutex(mutex);
    else
        sceKernelDeleteLwMutex(&lw_mutex);
}

int main(void) {
    run_case("heavy", HEAVY);
    run_case("lightweight", LIGHT);

    test_exit();
    return 0;
}
