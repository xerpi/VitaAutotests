// Prototype scenario notify_after_completion, not for merging: a thread ends itself from a callback
// during a semaphore wait with callbacks. A1's wait is ended by the notify, and its callback exits
// it at the next pass. A2's wait is ended by a signal, and with
// VITA3K_PROTO_SCENARIO=notify_after_completion A2 doesn't go on until its callback was notified,
// so the callback exits it in the final check and the count A2 took stays taken.
#include <psp2/kernel/threadmgr.h>

#include "test.h"

static SceUID sema, callbacks[2];
static volatile int ready[2], returned[2];

static int exit_callback(int notify_id, int count, int arg, void *common) {
    (void)notify_id;
    (void)count;
    (void)common;
    sceKernelExitThread(arg);
    return 0;
}

static int waiter_entry(SceSize args, void *argp) {
    (void)args;
    const int i = *(const int *)argp;
    callbacks[i] = sceKernelCreateCallback("exit", 0, exit_callback, NULL);
    ready[i] = 1;
    sceKernelWaitSemaCB(sema, 1, NULL);
    returned[i] = 1;
    return 0;
}

static SceKernelSemaInfo sema_info(void) {
    SceKernelSemaInfo info = { .size = sizeof(info) };
    sceKernelGetSemaInfo(sema, &info);
    return info;
}

int main(void) {
    sema = sceKernelCreateSema("s", 0, 0, 1, NULL);
    int status = 0;

    const SceUID a1 = test_start_thread_arg(waiter_entry, 0);
    TEST_WAIT_FOR(ready[0]);
    test_wait_until_waiting(a1);
    PRINT("A1: notify", sceKernelNotifyCallback(callbacks[0], 0x77));
    PRINT("A1: end", sceKernelWaitThreadEnd(a1, &status, NULL));
    PRINT("A1: exit status", status);
    PRINT("A1: its wait returned", returned[0]);
    PRINT("A1: waiters after", sema_info().numWaitThreads);
    PRINT("A1: the count after", sema_info().currentCount);
    sceKernelDeleteThread(a1);

    const SceUID a2 = test_start_thread_arg(waiter_entry, 1);
    TEST_WAIT_FOR(ready[1]);
    test_wait_until_waiting(a2);
    PRINT("A2: signal", sceKernelSignalSema(sema, 1));
    PRINT("A2: notify", sceKernelNotifyCallback(callbacks[1], 0x88));
    PRINT("A2: end", sceKernelWaitThreadEnd(a2, &status, NULL));
    PRINT("A2: exit status", status);
    PRINT("A2: its wait returned", returned[1]);
    PRINT("A2: waiters after", sema_info().numWaitThreads);
    PRINT("A2: the count after", sema_info().currentCount);
    sceKernelDeleteThread(a2);

    test_exit();
    return 0;
}
