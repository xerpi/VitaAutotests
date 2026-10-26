// Prototype check, not for merging: a callback notify takes W out of its end wait on T, and T ends
// and starts again while W's callback runs. The callback blocks on a gate, so W is out of T's end
// queue for as long as the test wants, and T's first end finds no waiter. Once the callback
// returns, W waits for T again, and gets the second run's status when the test ends T.
#include <psp2/kernel/threadmgr.h>

#include "test.h"
#include "threadmgr_extra.h"

#define SENTINEL 0xDEAD
#define WAIT_US 5000000

static SceUID gate, callback_gate, target, callback;
static volatile int callback_runs, in_callback, w_ready;
static volatile int w_result = -1;
static volatile int w_status = SENTINEL;

// Blocks until the test opens its gate
static int callback_entry(int notify_id, int count, int arg, void *common) {
    (void)notify_id;
    (void)count;
    (void)arg;
    (void)common;
    callback_runs++;
    in_callback = 1;
    SceUInt timeout = WAIT_US;
    sceKernelWaitSema(callback_gate, 1, &timeout);
    in_callback = 0;
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
    return 0;
}

// Returns whether thread waits with the type
static int waits_with(SceUID thread, SceUInt32 type) {
    SceKernelThreadInfo info = { .size = sizeof(info) };
    return sceKernelGetThreadInfo(thread, &info) == 0 && info.status == SCE_THREAD_WAITING && info.waitType == type;
}

// Returns whether T is dormant
static int target_dormant(void) {
    SceKernelThreadInfo info = { .size = sizeof(info) };
    return sceKernelGetThreadInfo(target, &info) == 0 && info.status == SCE_THREAD_DORMANT;
}

int main(void) {
    gate = sceKernelCreateSema("gate", 0, 0, 1, NULL);
    callback_gate = sceKernelCreateSema("callback gate", 0, 0, 1, NULL);
    target = sceKernelCreateThread("T", target_entry, SCE_KERNEL_DEFAULT_PRIORITY_USER, 0x4000, 0, 0, NULL);
    int run = 1;
    sceKernelStartThread(target, sizeof(run), &run);
    const SceUID w = test_start_thread(w_entry);
    TEST_WAIT_FOR(w_ready);
    test_wait_until_waiting(w);
    PRINT("W waits for T's end", waits_with(w, WAIT_TYPE_THREAD_END | WAIT_TYPE_CB));
    PRINT("notify W's callback", sceKernelNotifyCallback(callback, 1));
    TEST_WAIT_FOR(in_callback);
    PRINT("end T's first run", sceKernelSignalSema(gate, 1));
    TEST_WAIT_FOR(target_dormant());
    PRINT("W's status variable after T's first end", w_status);
    run = 2;
    PRINT("start T again", sceKernelStartThread(target, sizeof(run), &run));
    PRINT("let the callback return", sceKernelSignalSema(callback_gate, 1));
    TEST_WAIT_FOR(waits_with(w, WAIT_TYPE_THREAD_END | WAIT_TYPE_CB));
    PRINT("W waits for T's end again", waits_with(w, WAIT_TYPE_THREAD_END | WAIT_TYPE_CB));
    PRINT("end T's second run", sceKernelSignalSema(gate, 1));
    test_join_thread(w);
    PRINT("W's end wait", w_result);
    PRINT("W's status", w_status);
    PRINT("runs of the callback", callback_runs);
    sceKernelWaitThreadEnd(target, NULL, NULL);
    sceKernelDeleteThread(target);
    sceKernelDeleteSema(gate);
    sceKernelDeleteSema(callback_gate);
    test_exit();
    return 0;
}
