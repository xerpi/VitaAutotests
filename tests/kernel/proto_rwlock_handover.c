// Prototype scenario exit_after_completion, not for merging: what a write unlock lets in, read
// before anyone it let in runs. The main thread write locks three rwlocks. On the first two readers
// wait, then a writer behind them. On the second a writer waits, then two readers behind it. On the
// third only two readers wait. The main thread write unlocks all three. With
// VITA3K_PROTO_SCENARIO=exit_after_completion a waiter whose wait ended goes on only once it is
// exiting, so the main thread reads the infos while the waiters let in are alive and haven't run.
// On the first two the writer must have the lock wherever it is in the queue, with both readers
// still waiting, and on the third both readers share it.
#include <psp2/kernel/threadmgr.h>

#include "test.h"

enum { READERS_FIRST, WRITER_FIRST, READERS_ONLY, LOCKS };

typedef struct Waiter {
    int lock;
    int write;
} Waiter;

static SceUID rwlocks[LOCKS];
static const Waiter waiters[] = {
    { READERS_FIRST, 0 }, { READERS_FIRST, 0 }, { READERS_FIRST, 1 },
    { WRITER_FIRST, 1 }, { WRITER_FIRST, 0 }, { WRITER_FIRST, 0 },
    { READERS_ONLY, 0 }, { READERS_ONLY, 0 },
};
#define WAITERS (sizeof(waiters) / sizeof(waiters[0]))
static SceUID threads[WAITERS];
static volatile int returned[WAITERS];

static int waiter_entry(SceSize args, void *argp) {
    (void)args;
    const int i = *(const int *)argp;
    const SceUID rwlock = rwlocks[waiters[i].lock];
    if (waiters[i].write)
        sceKernelLockWriteRWLock(rwlock, NULL);
    else
        sceKernelLockReadRWLock(rwlock, NULL);
    returned[i] = 1;
    return 0;
}

static void print_info(const char *name, int lock, int writer) {
    SceKernelRWLockInfo info = { .size = sizeof(info) };
    sceKernelGetRWLockInfo(rwlocks[lock], &info);
    PRINTF(info.lockCount, "%s: the lock count", name);
    PRINTF(writer >= 0 ? info.writeOwnerId == threads[writer] : info.writeOwnerId == 0, "%s: the write owner is the writer, or nobody without one", name);
    PRINTF(info.numReadWaitThreads, "%s: read waiters", name);
    PRINTF(info.numWriteWaitThreads, "%s: write waiters", name);
}

int main(void) {
    for (int i = 0; i < LOCKS; i++) {
        rwlocks[i] = sceKernelCreateRWLock("rw", 0, NULL);
        sceKernelLockWriteRWLock(rwlocks[i], NULL);
    }
    for (unsigned int i = 0; i < WAITERS; i++) {
        threads[i] = test_start_thread_arg(waiter_entry, i);
        test_wait_until_waiting(threads[i]);
    }
    for (int i = 0; i < LOCKS; i++)
        PRINTF(sceKernelUnlockWriteRWLock(rwlocks[i]), "write unlock %d", i);
    print_info("readers, then a writer", READERS_FIRST, 2);
    print_info("a writer, then readers", WRITER_FIRST, 3);
    print_info("readers only", READERS_ONLY, -1);
    int any_returned = 0;
    for (unsigned int i = 0; i < WAITERS; i++)
        any_returned |= returned[i];
    PRINT("a waiter returned before the exit", any_returned);

    test_exit();
    return 0;
}
