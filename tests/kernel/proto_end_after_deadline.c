// Prototype scenario end_after_deadline, not for merging: T ends after W's end wait passed its
// deadline but before W claimed its timeout. With VITA3K_PROTO_SCENARIO=end_after_deadline W holds
// after its deadline, without T's end queue, until another party ended its wait, so T's end claims
// it: W's end wait returns 0 with T's status, and the time left is 0.
#include <psp2/kernel/threadmgr.h>

#include "test.h"
#include "threadmgr_extra.h"

#define WAIT_US 100000

static SceUID gate, target;
static volatile int w_ready;
static volatile int w_result = -1;
static volatile int w_status = 0xDEAD;
static volatile SceUInt w_time_left = 0xDEAD;

// Waits for the gate, then ends with 0x111
static int target_entry(SceSize args, void *argp) {
    (void)args;
    (void)argp;
    SceUInt timeout = 2 * TEST_WAIT_US;
    sceKernelWaitSema(gate, 1, &timeout);
    return 0x111;
}

static int w_entry(SceSize args, void *argp) {
    (void)args;
    (void)argp;
    w_ready = 1;
    SceUInt timeout = WAIT_US;
    w_result = sceKernelWaitThreadEnd(target, (int *)&w_status, &timeout);
    w_time_left = timeout;
    return 0;
}

// Returns whether W is in its end wait
static int waits_for_end(SceUID w) {
    SceKernelThreadInfo info = { .size = sizeof(info) };
    return sceKernelGetThreadInfo(w, &info) == 0 && info.status == SCE_THREAD_WAITING && info.waitType == WAIT_TYPE_THREAD_END;
}

int main(void) {
    gate = sceKernelCreateSema("gate", 0, 0, 1, NULL);
    target = test_start_thread(target_entry);
    const SceUID w = test_start_thread(w_entry);
    TEST_WAIT_FOR(w_ready);
    test_wait_until_waiting(w);
    // Well past W's deadline, while W holds before its timeout claim
    sceKernelDelayThread(3 * WAIT_US);
    PRINT("W still waits after its deadline", waits_for_end(w));
    PRINT("end T", sceKernelSignalSema(gate, 1));
    test_join_thread(w);
    PRINT("W's end wait", w_result);
    PRINT("W's status", w_status);
    PRINT("W's time left", w_time_left);
    test_join_thread(target);
    sceKernelDeleteSema(gate);
    test_exit();
    return 0;
}
