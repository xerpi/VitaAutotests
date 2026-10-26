// Prototype scenario signal_notify_before_park, not for merging: W's signal wait with callbacks
// registers while its callback is pending, since the callback notified itself at the loop top, and
// a notify comes before W parks. With VITA3K_PROTO_SCENARIO=signal_notify_before_park each pass of
// W holds after it registered until another party ended it. The pending callback doesn't end the
// first pass, the notify does: W runs the callback a second time at the loop top and registers
// again, and the send ends that pass before it parks too.
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/threadmgr/signal.h>

#include "test.h"
#include "threadmgr_extra.h"

#define WAIT_US 3000000

static SceUID callback;
static volatile int callback_runs, w_ready, w_done;
static volatile int w_result = -1, w_check = -1;

// Notifies itself on its first run
static int callback_entry(int notify_id, int count, int arg, void *common) {
    (void)notify_id;
    (void)count;
    (void)arg;
    (void)common;
    if (++callback_runs == 1)
        sceKernelNotifyCallback(callback, 2);
    return 0;
}

static int w_entry(SceSize args, void *argp) {
    (void)args;
    (void)argp;
    callback = sceKernelCreateCallback("cb", 0, callback_entry, NULL);
    sceKernelNotifyCallback(callback, 1);
    w_ready = 1;
    SceUInt timeout = WAIT_US;
    w_result = sceKernelWaitSignalCB(0, 0, &timeout);
    w_done = callback_runs;
    w_check = sceKernelCheckCallback();
    return 0;
}

// Returns whether W is in its signal wait with callbacks
static int waits_for_signal(SceUID w) {
    SceKernelThreadInfo info = { .size = sizeof(info) };
    return sceKernelGetThreadInfo(w, &info) == 0 && info.waitType == (WAIT_TYPE_SIGNAL | WAIT_TYPE_CB);
}

// Returns the notifications of the callback that haven't run yet
static int pending_notifies(void) {
    CallbackInfo info = { .size = sizeof(info) };
    if (sceKernelGetCallbackInfo(callback, (SceKernelCallbackInfo *)&info) < 0)
        return -1;
    return info.notifyCount;
}

int main(void) {
    const SceUID w = test_start_thread(w_entry);
    TEST_WAIT_FOR(w_ready);
    test_wait_until_waiting(w);
    PRINT("W waits for a signal", waits_for_signal(w));
    PRINT("runs of the callback while W waits", callback_runs);
    PRINT("notifies of the callback pending while W waits", pending_notifies());
    PRINT("notify W's callback", sceKernelNotifyCallback(callback, 3));
    // W runs the callback again and registers its second pass
    TEST_WAIT_FOR(callback_runs == 2);
    test_wait_until_waiting(w);
    PRINT("W waits for a signal again", waits_for_signal(w));
    PRINT("notifies of the callback pending then", pending_notifies());
    PRINT("send W a signal", sceKernelSendSignal(w));
    test_join_thread(w);
    PRINT("W's signal wait", w_result);
    PRINT("runs of the callback when the wait returned", w_done);
    PRINT("the check after the wait", w_check);
    test_exit();
    return 0;
}
