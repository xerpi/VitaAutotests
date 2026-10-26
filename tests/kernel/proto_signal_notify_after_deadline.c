// Prototype scenario notify_after_deadline, not for merging: a callback notify comes after W's signal
// wait with callbacks passed its deadline but before W claimed it. With
// VITA3K_PROTO_SCENARIO=notify_after_deadline W holds after its deadline until another party ended
// its wait, so the notify must end it: W runs the callback at its loop top, finds the deadline gone
// when it registers again and returns WAIT_TIMEOUT with a time left of 0 and nothing pending.
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/threadmgr/signal.h>

#include "test.h"
#include "threadmgr_extra.h"

#define WAIT_US 100000

static SceUID callback;
static volatile int callback_runs, w_ready, w_done;
static volatile int w_result = -1, w_check = -1;
static volatile SceUInt w_time_left = 0xDEAD;

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
    SceUInt timeout = WAIT_US;
    w_result = sceKernelWaitSignalCB(0, 0, &timeout);
    w_time_left = timeout;
    w_done = callback_runs;
    w_check = sceKernelCheckCallback();
    return 0;
}

// Returns whether W is in its signal wait with callbacks
static int waits_for_signal(SceUID w) {
    SceKernelThreadInfo info = { .size = sizeof(info) };
    return sceKernelGetThreadInfo(w, &info) == 0 && info.waitType == (WAIT_TYPE_SIGNAL | WAIT_TYPE_CB);
}

int main(void) {
    const SceUID w = test_start_thread(w_entry);
    TEST_WAIT_FOR(w_ready);
    test_wait_until_waiting(w);
    // Well past W's deadline, while W holds before its timeout claim
    sceKernelDelayThread(3 * WAIT_US);
    PRINT("W still waits after its deadline", waits_for_signal(w));
    PRINT("notify W's callback", sceKernelNotifyCallback(callback, 1));
    test_join_thread(w);
    PRINT("W's signal wait", w_result);
    PRINT("W's time left", w_time_left);
    PRINT("runs of the callback when the wait returned", w_done);
    PRINT("the check after the wait", w_check);
    test_exit();
    return 0;
}
