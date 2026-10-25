// Prototype scenario n2_after_rejoin, not for merging: a callback notify finds W's first wait, a
// signal ends it, and W waits again before the notify's second step runs. With
// VITA3K_PROTO_SCENARIO=n2_after_rejoin that step waits until W's second wait is registered, so it
// must leave that newer wait alone.
#include <psp2/kernel/threadmgr.h>

#include "test.h"

static SceUID sema, callback;
static volatile int callback_runs, w_ready, notified, w_first_done;
static volatile int w_first = -1, w_second = -1;

static int callback_entry(int notify_id, int count, int arg, void *common) {
    (void)notify_id;
    (void)count;
    (void)arg;
    (void)common;
    callback_runs++;
    return 0;
}

static int w_entry(SceSize args, void *argp) {
    (void)args;
    (void)argp;
    callback = sceKernelCreateCallback("cb", 0, callback_entry, NULL);
    w_ready = 1;
    w_first = sceKernelWaitSemaCB(sema, 1, NULL);
    w_first_done = 1;
    w_second = sceKernelWaitSemaCB(sema, 1, NULL);
    return 0;
}

static int notifier_entry(SceSize args, void *argp) {
    (void)args;
    (void)argp;
    sceKernelNotifyCallback(callback, 1);
    notified = 1;
    return 0;
}

int main(void) {
    sema = sceKernelCreateSema("s", 0, 0, 1, NULL);
    const SceUID w = test_start_thread(w_entry);
    TEST_WAIT_FOR(w_ready);
    test_wait_until_waiting(w);
    const SceUID notifier = test_start_thread(notifier_entry);
    sceKernelDelayThread(100000);
    PRINT("the notify returned before the first signal", notified);
    PRINT("first signal", sceKernelSignalSema(sema, 1));
    TEST_WAIT_FOR(notified);
    PRINT("the notify returned", notified);
    PRINT("W's first wait", w_first);
    sceKernelDelayThread(100000);
    SceKernelThreadInfo info = { .size = sizeof(info) };
    sceKernelGetThreadInfo(w, &info);
    PRINT("W's status after the notify", info.status);
    PRINT("W's wait type after the notify", info.waitType);
    PRINT("W's second wait so far", w_second);
    PRINT("second signal", sceKernelSignalSema(sema, 1));
    test_join_thread(w);
    test_join_thread(notifier);
    PRINT("W's second wait", w_second);
    PRINT("W's callback runs", callback_runs);

    test_exit();
    return 0;
}
