// Prototype scenario notify_after_deadline, not for merging: a callback notify comes after W's delay
// with callbacks passed its deadline but before W claimed it. With
// VITA3K_PROTO_SCENARIO=notify_after_deadline W holds after its deadline until another party ended
// its wait, so the notify must end it: W runs the callback in the delay, finds the deadline gone at
// its next pass and returns 0 with nothing pending.
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
    // Well past W's deadline, while W holds before its timeout claim
    sceKernelDelayThread(3 * DELAY_US);
    PRINT("W still waits after its deadline", test_in_wait(w, WAIT_TYPE_DELAY | WAIT_TYPE_CB, -1));
    PRINT("notify W's callback", sceKernelNotifyCallback(callback, 1));
    test_join_thread(w);
    PRINT("W's delay", w_result);
    PRINT("runs of the callback when the delay returned", w_done);
    PRINT("the check after the delay", w_check);
    test_exit();
    return 0;
}
