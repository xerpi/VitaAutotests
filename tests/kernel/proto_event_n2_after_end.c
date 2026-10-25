// Prototype scenario n2_after_end on a simple event, not for merging: a set ends W's wait with
// callbacks between the two steps of a callback notify. With VITA3K_PROTO_SCENARIO=n2_after_end the
// notify's second step waits until W's wait ended, so it must find it over and leave it.
#include <psp2/kernel/threadmgr.h>

#include "test.h"
#include "threadmgr_extra.h"

static SceUID event, callback;
static volatile int callback_runs, w_ready, notified;
static volatile int w_result = -1;
static SceUInt32 w_bits;

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
    w_result = sceKernelWaitEventCB(event, 0x01000000, &w_bits, NULL, NULL);
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
    event = sceKernelCreateSimpleEvent("e", EVENT_ATTR_AUTO_RESET, 0, NULL);
    const SceUID w = test_start_thread(w_entry);
    TEST_WAIT_FOR(w_ready);
    test_wait_until_waiting(w);
    const SceUID notifier = test_start_thread(notifier_entry);
    sceKernelDelayThread(100000);
    PRINT("the notify returned before the set", notified);
    PRINT("set", sceKernelSetEvent(event, 0x01000000, 0));
    test_join_thread(w);
    TEST_WAIT_FOR(notified);
    test_join_thread(notifier);
    PRINT("W's wait", w_result);
    PRINT("W's bits", w_bits);
    PRINT("W's callback runs", callback_runs);
    SceUInt32 pattern = 0;
    sceKernelGetEventPattern(event, &pattern);
    PRINT("the event's bits after", pattern & 0xFF000000);

    test_exit();
    return 0;
}
