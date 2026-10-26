// Prototype scenario, not for merging, no barrier: W's condition wait times out before any signal,
// so the timeout's claim wins. W relocks the free mutex and then waits on a gate, alive and still
// owning the mutex, while the main thread checks the owner, signals the condition (no waiter) and
// signals W directly (not waiting). Only then the gate opens. Heavy and lightweight conditions.
#include <psp2/kernel/threadmgr.h>

#include "test.h"
#include "threadmgr_extra.h"

#define TIMEOUT_US 100000

static int lightweight;
static SceUID mutex, cond, gate;
static SceKernelLwMutexWork lw_mutex;
static SceKernelLwCondWork lw_cond;
static volatile int w_ready, w_done;
static volatile int w_result;
static volatile SceUInt32 w_left;

static SceUID owner(void) {
    if (!lightweight) {
        SceKernelMutexInfo info = { .size = sizeof(info) };
        sceKernelGetMutexInfo(mutex, &info);
        return info.currentOwnerId;
    }
    LwMutexInfo info = { .size = sizeof(info) };
    sceKernelGetLwMutexInfo(&lw_mutex, &info);
    return info.currentOwnerId;
}

static int w_entry(SceSize args, void *argp) {
    (void)args;
    (void)argp;
    if (lightweight)
        sceKernelLockLwMutex(&lw_mutex, 1, NULL);
    else
        sceKernelLockMutex(mutex, 1, NULL);
    w_ready = 1;
    SceUInt32 timeout = TIMEOUT_US;
    w_result = lightweight ? sceKernelWaitLwCond(&lw_cond, &timeout) : sceKernelWaitCond(cond, &timeout);
    w_left = timeout;
    w_done = 1;
    SceUInt32 gate_timeout = 2 * TEST_WAIT_US;
    sceKernelWaitSema(gate, 1, &gate_timeout);
    if (lightweight)
        sceKernelUnlockLwMutex(&lw_mutex, 1);
    else
        sceKernelUnlockMutex(mutex, 1);
    return 0;
}

static void run_case(const char *name, int lw) {
    lightweight = lw;
    w_ready = 0;
    w_done = 0;
    w_result = -1;
    w_left = 0xDEAD;
    gate = sceKernelCreateSema("gate", 0, 0, 1, NULL);
    if (lw) {
        sceKernelCreateLwMutex(&lw_mutex, "m", 0, 0, NULL);
        sceKernelCreateLwCond(&lw_cond, "c", 0, &lw_mutex, NULL);
    } else {
        mutex = sceKernelCreateMutex("m", 0, 0, NULL);
        cond = sceKernelCreateCond("c", 0, mutex, NULL);
    }
    const SceUID w = test_start_thread(w_entry);
    TEST_WAIT_FOR(w_ready);
    TEST_WAIT_FOR(w_done);
    // W is alive behind the gate here, so nothing has released the mutex for it
    const int owned = owner() == w;
    const int signal = lw ? sceKernelSignalLwCond(&lw_cond) : sceKernelSignalCond(cond);
    const int signal_to = lw ? sceKernelSignalLwCondTo(&lw_cond, w) : sceKernelSignalCondTo(cond, w);
    sceKernelSignalSema(gate, 1);
    test_join_thread(w);
    PRINTF(w_result, "%s: W's wait", name);
    PRINTF(w_left, "%s: W's time left", name);
    PRINTF(owned, "%s: W owns the mutex after its wait, before the gate opens", name);
    PRINTF(signal, "%s: a signal after the timeout", name);
    PRINTF(signal_to, "%s: SignalCondTo(W) after the timeout", name);
    sceKernelDeleteSema(gate);
    if (lw) {
        sceKernelDeleteLwCond(&lw_cond);
        sceKernelDeleteLwMutex(&lw_mutex);
    } else {
        sceKernelDeleteCond(cond);
        sceKernelDeleteMutex(mutex);
    }
}

int main(void) {
    run_case("heavy", 0);
    run_case("lightweight", 1);
    test_exit();
    return 0;
}
