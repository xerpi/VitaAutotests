// Prototype scenario exit_after_completion, not for merging: a signal ends W's wait, then the
// process exits before W goes on. With VITA3K_PROTO_SCENARIO=exit_after_completion W waits after
// its wait ended until it is exiting, so its call must unwind with the exit and keep the count it
// took.
#include <psp2/kernel/threadmgr.h>

#include "test.h"

static SceUID sema;
static volatile int w_returned;

static int w_entry(SceSize args, void *argp) {
    (void)args;
    (void)argp;
    sceKernelWaitSema(sema, 1, NULL);
    w_returned = 1;
    return 0;
}

int main(void) {
    sema = sceKernelCreateSema("s", 0, 0, 1, NULL);
    const SceUID w = test_start_thread(w_entry);
    test_wait_until_waiting(w);
    PRINT("signal", sceKernelSignalSema(sema, 1));
    sceKernelDelayThread(100000);
    PRINT("W returned", w_returned);
    SceKernelSemaInfo info = { .size = sizeof(info) };
    sceKernelGetSemaInfo(sema, &info);
    PRINT("the count W took", info.currentCount);

    test_exit();
    return 0;
}
