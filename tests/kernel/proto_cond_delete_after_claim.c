// Prototype scenario cond_delete_after_claim, not for merging: a signal or the timeout ends W's
// condition wait, and the main thread deletes the condition before W resumes. With
// VITA3K_PROTO_SCENARIO=cond_delete_after_claim W holds after the claim, with the condition's lock
// let go, until the condition is deleted. W must return what the claim gave it, 0 or WAIT_TIMEOUT,
// without the mutex, so its unlock fails. Heavy and lightweight conditions.
#include <psp2/kernel/threadmgr.h>

#include "test.h"
#include "threadmgr_extra.h"

#define TIMEOUT_US 100000

static int lightweight, timed;
static SceUID mutex, cond;
static SceKernelLwMutexWork lw_mutex;
static SceKernelLwCondWork lw_cond;
static volatile int w_ready, w_done, w_owned;
static volatile int w_result, w_unlock;

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
    SceUInt32 timeout = timed ? TIMEOUT_US : 5 * TEST_WAIT_US;
    w_result = lightweight ? sceKernelWaitLwCond(&lw_cond, &timeout) : sceKernelWaitCond(cond, &timeout);
    w_owned = owner() == sceKernelGetThreadId();
    w_unlock = lightweight ? sceKernelUnlockLwMutex(&lw_mutex, 1) : sceKernelUnlockMutex(mutex, 1);
    w_done = 1;
    return 0;
}

static void run_case(const char *name, int lw, int timeout_case) {
    lightweight = lw;
    timed = timeout_case;
    w_ready = 0;
    w_done = 0;
    w_owned = 0;
    w_result = -1;
    w_unlock = -1;
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
    if (timeout_case)
        // Past W's deadline, so its timeout claimed the wait and W holds after it
        sceKernelDelayThread(3 * TIMEOUT_US);
    else
        PRINTF(lw ? sceKernelSignalLwCond(&lw_cond) : sceKernelSignalCond(cond), "%s: the signal", name);
    PRINTF(w_done, "%s: W is still held before the delete", name);
    PRINTF(lw ? sceKernelDeleteLwCond(&lw_cond) : sceKernelDeleteCond(cond), "%s: delete the condition", name);
    test_join_thread(w);
    PRINTF(w_result, "%s: W's wait", name);
    PRINTF(w_owned, "%s: W owned the mutex after its wait", name);
    PRINTF(w_unlock, "%s: W's unlock after its wait", name);
    if (lw)
        sceKernelDeleteLwMutex(&lw_mutex);
    else
        sceKernelDeleteMutex(mutex);
}

int main(void) {
    run_case("heavy, signal", 0, 0);
    run_case("heavy, timeout", 0, 1);
    run_case("lightweight, signal", 1, 0);
    run_case("lightweight, timeout", 1, 1);
    test_exit();
    return 0;
}
