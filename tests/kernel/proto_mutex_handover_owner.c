// Prototype scenario exit_after_completion, not for merging: the main thread owns a recursive
// lightweight mutex and a recursive heavy mutex, L waits on the lightweight one and H on the heavy
// one, each for a count of 2, and the main thread unlocks both. With
// VITA3K_PROTO_SCENARIO=exit_after_completion a waiter whose wait ended goes on only once it is
// exiting, so L and H are alive and haven't run while the main thread reads what the unlocks handed
// over: the lightweight mutex's work area words and info, and the heavy mutex's info. Nothing joins
// them first, since a heavy mutex goes to its next waiter when its owner ends.
#include <psp2/kernel/threadmgr.h>

#include "test.h"
#include "threadmgr_extra.h"

static SceUID heavy;
static SceKernelLwMutexWork lw_mutex;
static volatile int l_result = -1, h_result = -1;

static int l_entry(SceSize args, void *argp) {
    (void)args;
    (void)argp;
    l_result = sceKernelLockLwMutex(&lw_mutex, 2, NULL);
    return 0;
}

static int h_entry(SceSize args, void *argp) {
    (void)args;
    (void)argp;
    h_result = sceKernelLockMutex(heavy, 2, NULL);
    return 0;
}

int main(void) {
    sceKernelCreateLwMutex(&lw_mutex, "lw", LW_MUTEX_ATTR_RECURSIVE, 1, NULL);
    heavy = sceKernelCreateMutex("heavy", SCE_KERNEL_MUTEX_ATTR_RECURSIVE, 1, NULL);
    const SceUID l = test_start_thread(l_entry);
    test_wait_until_waiting(l);
    const SceUID h = test_start_thread(h_entry);
    test_wait_until_waiting(h);
    PRINT("unlock the lightweight mutex", sceKernelUnlockLwMutex(&lw_mutex, 1));
    PRINT("unlock the heavy mutex", sceKernelUnlockMutex(heavy, 1));
    const volatile SceInt32 *words = (const volatile SceInt32 *)&lw_mutex;
    PRINT("the work area's owner word is L", words[0] == l);
    PRINT("the work area's count word", words[2]);
    LwMutexInfo lw_info = { .size = sizeof(lw_info) };
    sceKernelGetLwMutexInfo(&lw_mutex, &lw_info);
    PRINT("the lightweight info's owner is L", lw_info.currentOwnerId == l);
    PRINT("the lightweight info's count", lw_info.currentCount);
    PRINT("the lightweight info's waiters", lw_info.numWaitThreads);
    SceKernelMutexInfo info = { .size = sizeof(info) };
    sceKernelGetMutexInfo(heavy, &info);
    PRINT("the heavy info's owner is H", info.currentOwnerId == h);
    PRINT("the heavy info's count", info.currentCount);
    PRINT("the heavy info's waiters", info.numWaitThreads);
    PRINT("L returned", l_result != -1);
    PRINT("H returned", h_result != -1);

    test_exit();
    return 0;
}
