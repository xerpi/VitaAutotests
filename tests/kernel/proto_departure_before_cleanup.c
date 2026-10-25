// Prototype scenario departure_before_cleanup, not for merging: the semaphore has a count of 1,
// head A waits for 2 with callbacks and B behind it waits for 1. A callback notify takes A out.
// With VITA3K_PROTO_SCENARIO=departure_before_cleanup A doesn't go on until the count went to B and
// a later signal put some back, so B taking the count can only come from the notifier's own
// departure and serve, not from A's cleanup.
#include <psp2/kernel/threadmgr.h>

#include "test.h"

static SceUID sema, callback;
static volatile int callback_runs, a_ready, b_done, b_done_seen;
static volatile int a_result = -1, b_result = -1;

static int callback_entry(int notify_id, int count, int arg, void *common) {
    (void)notify_id;
    (void)count;
    (void)arg;
    (void)common;
    callback_runs++;
    b_done_seen = b_done;
    return 0;
}

static int a_entry(SceSize args, void *argp) {
    (void)args;
    (void)argp;
    callback = sceKernelCreateCallback("cb", 0, callback_entry, NULL);
    a_ready = 1;
    a_result = sceKernelWaitSemaCB(sema, 2, NULL);
    return 0;
}

static int b_entry(SceSize args, void *argp) {
    (void)args;
    (void)argp;
    b_result = sceKernelWaitSema(sema, 1, NULL);
    b_done = 1;
    return 0;
}

static SceKernelSemaInfo sema_info(void) {
    SceKernelSemaInfo info = { .size = sizeof(info) };
    sceKernelGetSemaInfo(sema, &info);
    return info;
}

int main(void) {
    sema = sceKernelCreateSema("s", 0, 1, 3, NULL);
    const SceUID a = test_start_thread(a_entry);
    TEST_WAIT_FOR(a_ready);
    test_wait_until_waiting(a);
    const SceUID b = test_start_thread(b_entry);
    test_wait_until_waiting(b);
    PRINT("waiters before the notify", sema_info().numWaitThreads);
    PRINT("notify A", sceKernelNotifyCallback(callback, 1));
    TEST_WAIT_FOR(b_done);
    PRINT("B's wait while A is held", b_result);
    PRINT("A's callback runs while A is held", callback_runs);
    PRINT("the count while A is held", sema_info().currentCount);
    PRINT("waiters while A is held", sema_info().numWaitThreads);
    PRINT("first signal", sceKernelSignalSema(sema, 1));
    TEST_WAIT_FOR(callback_runs);
    test_wait_until_waiting(a);
    PRINT("second signal", sceKernelSignalSema(sema, 1));
    test_join_thread(a);
    test_join_thread(b);
    PRINT("A's wait", a_result);
    PRINT("A's callback runs", callback_runs);
    PRINT("B was done when A's callback ran", b_done_seen);
    PRINT("the count after", sema_info().currentCount);

    test_exit();
    return 0;
}
