// Prototype scenario departure_before_cleanup, not for merging: W waits with callbacks for 0x1 on
// an event flag without SCE_EVENT_WAITMULTIPLE, the main thread sets 0x2, which doesn't end the
// wait, and notifies W's callback. With VITA3K_PROTO_SCENARIO=departure_before_cleanup W doesn't go
// on until it left the queue keeping its hold, with 0x2 in its local out bits, and a later set
// changed the flags. So what the main thread sees while W is held comes from the notifier's
// departure alone.
#include <psp2/kernel/threadmgr.h>

#include "test.h"

static SceUID flag, callback;
static volatile int callback_runs, w_ready;
static volatile int w_result = -1;
static volatile unsigned int w_bits = 0xBEEF;

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
    unsigned int bits = 0xBEEF;
    w_result = sceKernelWaitEventFlagCB(flag, 0x1, SCE_EVENT_WAITAND, &bits, NULL);
    w_bits = bits;
    return 0;
}

static SceKernelEventFlagInfo flag_info(void) {
    SceKernelEventFlagInfo info = { .size = sizeof(info) };
    sceKernelGetEventFlagInfo(flag, &info);
    return info;
}

int main(void) {
    flag = sceKernelCreateEventFlag("f", 0, 0, NULL);
    const SceUID w = test_start_thread(w_entry);
    TEST_WAIT_FOR(w_ready);
    test_wait_until_waiting(w);
    PRINT("waiters before the notify", flag_info().numWaitThreads);
    PRINT("set 0x2", sceKernelSetEventFlag(flag, 0x2));
    PRINT("notify W", sceKernelNotifyCallback(callback, 1));
    unsigned int poll_bits = 0xBEEF;
    PRINT("a poll for 0x2 while W is held", sceKernelPollEventFlag(flag, 0x2, SCE_EVENT_WAITOR, &poll_bits));
    PRINT("waiters while W is held", flag_info().numWaitThreads);
    PRINT("W's callback runs while W is held", callback_runs);
    PRINT("set 0x1", sceKernelSetEventFlag(flag, 0x1));
    test_join_thread(w);
    PRINT("W's wait", w_result);
    PRINT("W's out bits", w_bits);
    PRINT("W's callback runs", callback_runs);
    PRINT("the flags after", flag_info().currentPattern);

    test_exit();
    return 0;
}
