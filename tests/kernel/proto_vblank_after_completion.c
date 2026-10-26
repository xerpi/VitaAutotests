// Prototype scenario vblank_after_completion, not for merging: a callback notify comes right after
// the vblank ended W's vblank wait with callbacks, before W goes on. With
// VITA3K_PROTO_SCENARIO=vblank_after_completion W holds after its wait ended until a callback of it
// is notified. The main thread waits until W no longer waits, then notifies. W runs the callback
// before its vblank wait returns, and nothing is left for its check.
#include <psp2/display.h>
#include <psp2/kernel/threadmgr.h>

#include "test.h"
#include "threadmgr_extra.h"

#define VBLANKS 6

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
    // A display wait counts its vblanks from the thread's last one
    sceDisplayWaitVblankStart();
    w_ready = 1;
    w_result = sceDisplayWaitVblankStartMultiCB(VBLANKS);
    w_done = callback_runs;
    w_check = sceKernelCheckCallback();
    return 0;
}

// Returns whether W is in a wait
static int waits(SceUID w) {
    SceKernelThreadInfo info = { .size = sizeof(info) };
    return sceKernelGetThreadInfo(w, &info) == 0 && info.status == SCE_THREAD_WAITING;
}

int main(void) {
    const SceUID w = test_start_thread(w_entry);
    TEST_WAIT_FOR(w_ready);
    test_wait_until_waiting(w);
    // The vblank ended W's wait, and W holds
    TEST_WAIT_FOR(!waits(w));
    PRINT("W returned before the notify", w_done != 0 || w_result != -1);
    PRINT("notify W's callback", sceKernelNotifyCallback(callback, 1));
    test_join_thread(w);
    PRINT("W's vblank wait", w_result);
    PRINT("runs of the callback when the wait returned", w_done);
    PRINT("the check after the wait", w_check);
    test_exit();
    return 0;
}
