// Prototype scenario notify_during_scan, not for merging: W waits with callbacks on event E, which
// isn't set. With VITA3K_PROTO_SCENARIO=notify_during_scan W's first scan holds before its arm
// until a callback is pending, so the first notify comes during the scan and the arm must still
// sleep. A second notify of the same callback while W waits must end the wait, without counting the
// callback twice: after the multiple wait, the thread shows no callback pending and a semaphore
// wait with callbacks times out.
#include <psp2/kernel/threadmgr.h>

#include "test.h"
#include "threadmgr_extra.h"

static SceUID e, sema, callback;
static volatile int callback_runs, callback_count, w_ready, w_done;
static volatile int w_result = -1, w_sema = -1, w_runs_after = -1, w_notify_flag = -1;
static ResultEvent results[1];

static int callback_entry(int notify_id, int count, int arg, void *common) {
    (void)notify_id;
    (void)arg;
    (void)common;
    callback_runs++;
    callback_count = count;
    return 0;
}

static int w_entry(SceSize args, void *argp) {
    (void)args;
    (void)argp;
    callback = sceKernelCreateCallback("cb", 0, callback_entry, NULL);
    WaitEvent events[1] = { { e, 0x01000000 } };
    SceUInt timeout = 2000000;
    w_ready = 1;
    w_result = sceKernelWaitMultipleEventsCB(events, 1, EVENT_WAIT_MODE_OR, results, &timeout);
    SceKernelThreadInfo info = { .size = sizeof(info) };
    sceKernelGetThreadInfo(sceKernelGetThreadId(), &info);
    w_notify_flag = info.fNotifyCallback;
    SceUInt sema_timeout = 100000;
    w_sema = sceKernelWaitSemaCB(sema, 1, &sema_timeout);
    w_runs_after = callback_runs;
    w_done = 1;
    return 0;
}

static SceUInt32 wait_type(SceUID thread) {
    SceKernelThreadInfo info = { .size = sizeof(info) };
    sceKernelGetThreadInfo(thread, &info);
    return info.waitType;
}

int main(void) {
    e = sceKernelCreateSimpleEvent("e", EVENT_ATTR_AUTO_RESET, 0, NULL);
    sema = sceKernelCreateSema("s", 0, 0, 1, NULL);
    const SceUID w = test_start_thread(w_entry);
    TEST_WAIT_FOR(w_ready);
    sceKernelDelayThread(100000);
    PRINT("W's wait type during the scan", wait_type(w));
    PRINT("first notify, during the scan", sceKernelNotifyCallback(callback, 1));
    test_wait_until_waiting(w);
    PRINT("W's wait type after the arm", wait_type(w));
    PRINT("callback runs after the arm", callback_runs);
    PRINT("second notify, while W waits", sceKernelNotifyCallback(callback, 2));
    TEST_WAIT_FOR(callback_runs == 1);
    PRINT("callback runs after the second notify", callback_runs);
    PRINT("notifications the callback got", callback_count);
    test_wait_until_waiting(w);
    PRINT("set E", sceKernelSetEvent(e, 0x01000000, 0));
    TEST_WAIT_FOR(w_done);
    test_join_thread(w);
    PRINT("W's multiple wait", w_result);
    PRINT("W's callback notify flag after the multiple wait", w_notify_flag);
    PRINT("W's semaphore wait after", w_sema);
    PRINT("callback runs at the end", w_runs_after);

    test_exit();
    return 0;
}
