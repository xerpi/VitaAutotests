// Prototype scenario notify_after_deadline, not for merging: W's wait with callbacks times out, and
// a callback notify comes after the deadline but before W claimed the timeout. With
// VITA3K_PROTO_SCENARIO=notify_after_deadline W holds after its deadline until another party ended
// its wait, so W must still show waiting on the semaphore and the notify must find and end the
// wait.
#include <psp2/kernel/threadmgr.h>

#include "test.h"

static SceUID sema, callback;
static volatile int callback_runs, w_ready, w_done;
static volatile int w_result = -1;
static volatile SceUInt w_time_left;

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
    SceUInt timeout = 100000;
    w_result = sceKernelWaitSemaCB(sema, 1, &timeout);
    w_time_left = timeout;
    w_done = 1;
    return 0;
}

static SceKernelSemaInfo sema_info(void) {
    SceKernelSemaInfo info = { .size = sizeof(info) };
    sceKernelGetSemaInfo(sema, &info);
    return info;
}

int main(void) {
    sema = sceKernelCreateSema("s", 0, 0, 1, NULL);
    const SceUID w = test_start_thread(w_entry);
    TEST_WAIT_FOR(w_ready);
    test_wait_until_waiting(w);
    sceKernelDelayThread(300000);
    SceKernelThreadInfo info = { .size = sizeof(info) };
    sceKernelGetThreadInfo(w, &info);
    PRINT("W's status after its deadline", info.status);
    PRINT("W's wait type after its deadline", info.waitType);
    PRINT("waiters after W's deadline", sema_info().numWaitThreads);
    PRINT("notify", sceKernelNotifyCallback(callback, 1));
    TEST_WAIT_FOR(w_done);
    test_join_thread(w);
    PRINT("W's wait", w_result);
    PRINT("W's callback runs", callback_runs);
    PRINT("W's time left", w_time_left);
    PRINT("waiters after", sema_info().numWaitThreads);

    test_exit();
    return 0;
}
