// Prototype scenario notify_then_set, not for merging: W waits with callbacks on event E, and S
// waits on E behind W's token. A notify ends W's wait. With VITA3K_PROTO_SCENARIO=notify_then_set W
// holds before its cleanup until E has no waiter left, so a set of E comes while W's token is still
// queued. The token must leave without the bits and S must get them. W then runs its callback,
// waits again and gets a second set.
#include <psp2/kernel/threadmgr.h>

#include "test.h"
#include "threadmgr_extra.h"

static SceUID e, callback;
static volatile int callback_runs, w_ready, w_done, s_done;
static volatile int w_result = -1, s_result = -1;
static SceUInt32 s_bits;
static ResultEvent results[1];

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
    WaitEvent events[1] = { { e, 0x01000000 } };
    SceUInt timeout = 2000000;
    w_ready = 1;
    w_result = sceKernelWaitMultipleEventsCB(events, 1, EVENT_WAIT_MODE_OR, results, &timeout);
    w_done = 1;
    return 0;
}

static int s_entry(SceSize args, void *argp) {
    (void)args;
    (void)argp;
    SceUInt timeout = 2000000;
    s_result = sceKernelWaitEvent(e, 0x01000000, &s_bits, NULL, &timeout);
    s_done = 1;
    return 0;
}

int main(void) {
    e = sceKernelCreateSimpleEvent("e", EVENT_ATTR_AUTO_RESET, 0, NULL);
    const SceUID w = test_start_thread(w_entry);
    TEST_WAIT_FOR(w_ready);
    test_wait_until_waiting(w);
    const SceUID s = test_start_thread(s_entry);
    test_wait_until_waiting(s);
    PRINT("notify W", sceKernelNotifyCallback(callback, 1));
    sceKernelDelayThread(100000);
    PRINT("W's callback runs before the set", callback_runs);
    PRINT("first set", sceKernelSetEvent(e, 0x01000000, 0x11));
    TEST_WAIT_FOR(s_done);
    PRINT("S's wait", s_result);
    PRINT("S's bits", s_bits);
    TEST_WAIT_FOR(callback_runs == 1);
    test_wait_until_waiting(w);
    PRINT("W's callback runs after the set", callback_runs);
    PRINT("second set", sceKernelSetEvent(e, 0x01000000, 0x22));
    TEST_WAIT_FOR(w_done);
    test_join_thread(w);
    test_join_thread(s);
    PRINT("W's wait", w_result);
    PRINT("W's result user data", (int)results[0].userData);

    test_exit();
    return 0;
}
