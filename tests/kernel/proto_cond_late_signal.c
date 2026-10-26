// Prototype scenario notify_after_deadline, not for merging: a signal comes after W's condition
// wait passed its deadline but before W claimed the timeout. With
// VITA3K_PROTO_SCENARIO=notify_after_deadline W holds after its deadline until another party ended
// its wait, so the signal must win the claim: W returns 0 with a time left of 0, and owns the mutex.
// Heavy and lightweight conditions.
#include <psp2/kernel/threadmgr.h>

#include "test.h"
#include "threadmgr_extra.h"

#define TIMEOUT_US 100000

static int lightweight;
static SceUID mutex, cond;
static SceKernelLwMutexWork lw_mutex;
static SceKernelLwCondWork lw_cond;
static volatile int w_ready, w_owned;
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

// Returns whether thread waits for a signal of the condition, by its wait type
static int in_signal_wait(SceUID thread) {
    SceKernelThreadInfo info = { .size = sizeof(info) };
    return sceKernelGetThreadInfo(thread, &info) == 0 && info.waitType == (lightweight ? WAIT_TYPE_LW_COND : WAIT_TYPE_COND);
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
    w_owned = owner() == sceKernelGetThreadId();
    if (lightweight)
        sceKernelUnlockLwMutex(&lw_mutex, 1);
    else
        sceKernelUnlockMutex(mutex, 1);
    return 0;
}

static void run_case(const char *name, int lw) {
    lightweight = lw;
    w_ready = 0;
    w_owned = 0;
    w_result = -1;
    w_left = 0xDEAD;
    if (lw) {
        sceKernelCreateLwMutex(&lw_mutex, "m", 0, 0, NULL);
        sceKernelCreateLwCond(&lw_cond, "c", 0, &lw_mutex, NULL);
    } else {
        mutex = sceKernelCreateMutex("m", 0, 0, NULL);
        cond = sceKernelCreateCond("c", 0, mutex, NULL);
    }
    const SceUID w = test_start_thread(w_entry);
    TEST_WAIT_FOR(w_ready);
    test_wait_until_waiting(w);
    // Well past W's deadline, while W holds before its timeout claim
    sceKernelDelayThread(3 * TIMEOUT_US);
    PRINTF(in_signal_wait(w), "%s: W still waits on the condition after its deadline", name);
    PRINTF(lw ? sceKernelSignalLwCond(&lw_cond) : sceKernelSignalCond(cond), "%s: the signal", name);
    test_join_thread(w);
    PRINTF(w_result, "%s: W's wait", name);
    PRINTF(w_left, "%s: W's time left", name);
    PRINTF(w_owned, "%s: W owned the mutex after its wait", name);
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
