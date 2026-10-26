// Prototype scenario notify_after_completion, not for merging: a callback notify comes right after
// W's delay with callbacks claimed its own timeout. With
// VITA3K_PROTO_SCENARIO=notify_after_completion W holds after that claim until a callback is
// notified, so the notify finds the wait over: W's delay returns 0 without running the callback,
// which stays pending for W's check.
#include <psp2/kernel/threadmgr.h>

#include "test.h"
#include "threadmgr_extra.h"

#define DELAY_US 100000

static SceUID callback;
static volatile int callback_runs, w_ready, w_done;
static volatile int w_result = -1, w_check = -1;

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
    w_result = sceKernelDelayThreadCB(DELAY_US);
    w_done = callback_runs;
    w_check = sceKernelCheckCallback();
    return 0;
}

int main(void) {
    const SceUID w = test_start_thread(w_entry);
    TEST_WAIT_FOR(w_ready);
    test_wait_until_waiting(w);
    // Past W's deadline, so W has claimed its timeout and holds
    sceKernelDelayThread(3 * DELAY_US);
    PRINT("notify W's callback", sceKernelNotifyCallback(callback, 1));
    test_join_thread(w);
    PRINT("W's delay", w_result);
    PRINT("runs of the callback when the delay returned", w_done);
    PRINT("the check after the delay", w_check);
    PRINT("runs of the callback after the check", callback_runs);
    test_exit();
    return 0;
}
