// Prototype scenario end_after_completion, not for merging: T's end claims W's end wait with
// callbacks, and T starts again before W returns. With VITA3K_PROTO_SCENARIO=end_after_completion W
// holds after the end claimed its wait until a callback of it is notified. Meanwhile the main thread
// starts T again, which in Vita3K can only happen once T's end finished with its end queue, so W's
// wait was claimed by then. It then reads W's status variable, which still holds its sentinel, since
// the status goes to the guest when the wait returns. W then returns the first run's status, and the
// callback stays pending for W's check, since an end wait runs no callbacks after its wait ended.
#include <psp2/kernel/threadmgr.h>

#include "test.h"
#include "threadmgr_extra.h"

#define SENTINEL 0xDEAD
#define WAIT_US 3000000

static SceUID gate, target, callback;
static volatile int callback_runs, w_ready, w_returned, w_done;
static volatile int w_result = -1, w_check = -1;
static volatile int w_status = SENTINEL;

static int callback_entry(int notify_id, int count, int arg, void *common) {
    (void)notify_id;
    (void)count;
    (void)arg;
    (void)common;
    callback_runs++;
    return 0;
}

// Waits for the gate, then ends with 0x111 on its first run and 0x222 on its second
static int target_entry(SceSize args, void *argp) {
    (void)args;
    SceUInt timeout = WAIT_US;
    sceKernelWaitSema(gate, 1, &timeout);
    return *(const int *)argp == 1 ? 0x111 : 0x222;
}

static int w_entry(SceSize args, void *argp) {
    (void)args;
    (void)argp;
    callback = sceKernelCreateCallback("cb", 0, callback_entry, NULL);
    w_ready = 1;
    SceUInt timeout = WAIT_US;
    w_result = sceKernelWaitThreadEndCB(target, (int *)&w_status, &timeout);
    w_returned = 1;
    w_done = callback_runs;
    w_check = sceKernelCheckCallback();
    return 0;
}

// Returns whether T is dormant
static int target_dormant(void) {
    SceKernelThreadInfo info = { .size = sizeof(info) };
    return sceKernelGetThreadInfo(target, &info) == 0 && info.status == SCE_THREAD_DORMANT;
}

int main(void) {
    gate = sceKernelCreateSema("gate", 0, 0, 1, NULL);
    target = sceKernelCreateThread("T", target_entry, SCE_KERNEL_DEFAULT_PRIORITY_USER, 0x4000, 0, 0, NULL);
    int run = 1;
    sceKernelStartThread(target, sizeof(run), &run);
    const SceUID w = test_start_thread(w_entry);
    TEST_WAIT_FOR(w_ready);
    test_wait_until_waiting(w);
    PRINT("end T's first run", sceKernelSignalSema(gate, 1));
    TEST_WAIT_FOR(target_dormant());
    run = 2;
    PRINT("start T again", sceKernelStartThread(target, sizeof(run), &run));
    PRINT("W's status variable while W holds", w_status);
    PRINT("W returned before the notify", w_returned);
    PRINT("notify W's callback", sceKernelNotifyCallback(callback, 1));
    test_join_thread(w);
    PRINT("W's end wait", w_result);
    PRINT("W's status", w_status);
    PRINT("runs of the callback when the wait returned", w_done);
    PRINT("the check after the wait", w_check);
    sceKernelSignalSema(gate, 1);
    int second = 0;
    PRINT("wait for T's second run", sceKernelWaitThreadEnd(target, &second, NULL));
    PRINT("T's second status", second);
    sceKernelDeleteThread(target);
    sceKernelDeleteSema(gate);
    test_exit();
    return 0;
}
