// Prototype check, not for merging: a callback notify during a vblank wait of 60 vblanks, about a
// second. With callbacks the notify ends the sleep, the callback runs, and the wait goes on until
// its vblank. Without callbacks the notify changes nothing and the callback runs at the check after
// the wait. Each case prints whether the wait lasted at least half a second, its result and how
// often the callback ran when it returned and after the check.
#include <psp2/display.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>

#include "test.h"
#include "threadmgr_extra.h"

#define VBLANKS 60
#define NOTIFY_US 100000
#define LASTED_US 500000

static SceUID callback;
static int with_callbacks;
static volatile int callback_runs, w_ready, w_done;
static volatile int w_result = -1, w_check = -1;
static volatile SceUInt64 w_elapsed;

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
    // A display wait counts its vblanks from the thread's last one
    sceDisplayWaitVblankStart();
    w_ready = 1;
    const SceUInt64 start = sceKernelGetProcessTimeWide();
    w_result = with_callbacks ? sceDisplayWaitVblankStartMultiCB(VBLANKS) : sceDisplayWaitVblankStartMulti(VBLANKS);
    w_elapsed = sceKernelGetProcessTimeWide() - start;
    w_done = callback_runs;
    w_check = sceKernelCheckCallback();
    return 0;
}

static void run_case(const char *name, int callbacks) {
    with_callbacks = callbacks;
    callback_runs = 0;
    w_ready = 0;
    const SceUID w = test_start_thread(w_entry);
    TEST_WAIT_FOR(w_ready);
    test_wait_until_waiting(w);
    sceKernelDelayThread(NOTIFY_US);
    PRINTF(sceKernelNotifyCallback(callback, 1), "%s: notify W's callback", name);
    test_join_thread(w);
    PRINTF(w_elapsed >= LASTED_US, "%s: the wait lasted at least half a second", name);
    PRINTF(w_result, "%s: the wait", name);
    PRINTF(w_done, "%s: runs of the callback when the wait returned", name);
    PRINTF(w_check, "%s: the check after the wait", name);
}

int main(void) {
    run_case("with callbacks", 1);
    run_case("without callbacks", 0);
    test_exit();
    return 0;
}
