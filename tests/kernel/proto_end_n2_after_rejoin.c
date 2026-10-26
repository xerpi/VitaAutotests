// Prototype scenario n2_after_rejoin on a thread end wait, not for merging: a callback notify's
// claim of W's end wait on T comes only after W waits for T again. With
// VITA3K_PROTO_SCENARIO=n2_after_rejoin the notify holds before its claim until W has a newer wait
// that waits. Meanwhile W's first wait times out, and W's second wait runs the callback, which the
// notify left pending, and waits again. The late claim then finds a newer wait and must leave it
// waiting, which Vita3K checks under the end queue's lock, and W gets T's status when the test ends
// T.
#include <psp2/kernel/threadmgr.h>

#include "test.h"
#include "threadmgr_extra.h"

#define FIRST_WAIT_US 200000
#define WAIT_US 3000000

static SceUID gate, target, callback;
static volatile int callback_runs, w_ready, w_runs_after_first;
static volatile int w_first = -1, w_second = -1;
static volatile int w_status = 0xDEAD;

static int callback_entry(int notify_id, int count, int arg, void *common) {
    (void)notify_id;
    (void)count;
    (void)arg;
    (void)common;
    callback_runs++;
    return 0;
}

// Waits for the gate, then ends with 0x111
static int target_entry(SceSize args, void *argp) {
    (void)args;
    (void)argp;
    SceUInt timeout = WAIT_US;
    sceKernelWaitSema(gate, 1, &timeout);
    return 0x111;
}

static int w_entry(SceSize args, void *argp) {
    (void)args;
    (void)argp;
    callback = sceKernelCreateCallback("cb", 0, callback_entry, NULL);
    w_ready = 1;
    SceUInt timeout = FIRST_WAIT_US;
    w_first = sceKernelWaitThreadEndCB(target, NULL, &timeout);
    w_runs_after_first = callback_runs;
    timeout = WAIT_US;
    w_second = sceKernelWaitThreadEndCB(target, (int *)&w_status, &timeout);
    return 0;
}

// Returns whether thread waits with the type
static int waits_with(SceUID thread, SceUInt32 type) {
    SceKernelThreadInfo info = { .size = sizeof(info) };
    return sceKernelGetThreadInfo(thread, &info) == 0 && info.status == SCE_THREAD_WAITING && info.waitType == type;
}

int main(void) {
    gate = sceKernelCreateSema("gate", 0, 0, 1, NULL);
    target = test_start_thread(target_entry);
    const SceUID w = test_start_thread(w_entry);
    TEST_WAIT_FOR(w_ready);
    test_wait_until_waiting(w);
    // Returns once W waits again
    PRINT("notify W's callback", sceKernelNotifyCallback(callback, 1));
    PRINT("W waits for T's end", waits_with(w, WAIT_TYPE_THREAD_END | WAIT_TYPE_CB));
    PRINT("end T", sceKernelSignalSema(gate, 1));
    test_join_thread(w);
    PRINT("W's first end wait", w_first);
    PRINT("runs of the callback after it", w_runs_after_first);
    PRINT("W's second end wait", w_second);
    PRINT("W's status", w_status);
    PRINT("runs of the callback", callback_runs);
    test_join_thread(target);
    sceKernelDeleteSema(gate);
    test_exit();
    return 0;
}
