// Prototype scenario notify_after_completion, not for merging: the claim that ends a message pipe
// wait writes the time left once, and the waiter doesn't write it again when it returns. W receives
// 4 bytes FULL with callbacks and the main thread sends 4. S sends 4 bytes FULL with callbacks into
// a full pipe and the main thread receives 4. Each has a timeout of 2 s in a global. With
// VITA3K_PROTO_SCENARIO=notify_after_completion a waiter whose wait ended goes on only once a
// callback of it is pending, so the main thread reads both times left, written by the claims, then
// overwrites them with a sentinel and notifies both callbacks. W and S return normally, and the
// sentinel must still be there.
#include <psp2/kernel/threadmgr.h>

#include "test.h"
#include "threadmgr_extra.h"

#define CAPACITY 0x1000
#define SENTINEL 0x5EED

enum { W, S, WAITERS };

static SceUID pipes[WAITERS], callbacks[WAITERS];
static volatile SceUInt time_left[WAITERS];
static volatile int results[WAITERS] = { -1, -1 }, callback_runs[WAITERS], ready[WAITERS];
static char fill[CAPACITY];

static int callback_entry(int notify_id, int count, int arg, void *common) {
    (void)notify_id;
    (void)count;
    (void)arg;
    callback_runs[(int)common]++;
    return 0;
}

static int waiter_entry(SceSize args, void *argp) {
    (void)args;
    const int i = *(const int *)argp;
    callbacks[i] = sceKernelCreateCallback("cb", 0, callback_entry, (void *)i);
    ready[i] = 1;
    char buffer[4] = { 0 };
    SceSize size = 0;
    SceUInt *timeout = (SceUInt *)&time_left[i];
    if (i == W)
        results[i] = sceKernelReceiveMsgPipeCB(pipes[i], buffer, 4, MSG_PIPE_MODE_FULL, &size, timeout);
    else
        results[i] = sceKernelSendMsgPipeCB(pipes[i], buffer, 4, MSG_PIPE_MODE_FULL, &size, timeout);
    return 0;
}

static int written_below_2s(int i) {
    return time_left[i] > 1000000 && time_left[i] < 2000000;
}

int main(void) {
    SceUID threads[WAITERS];
    for (int i = 0; i < WAITERS; i++) {
        pipes[i] = sceKernelCreateMsgPipe("pipe", MSG_PIPE_TYPE_USER, 0, CAPACITY, NULL);
        time_left[i] = 2000000;
    }
    SceSize size = 0;
    PRINT("fill S's pipe", sceKernelSendMsgPipe(pipes[S], fill, CAPACITY, MSG_PIPE_MODE_FULL, &size, NULL));
    for (int i = 0; i < WAITERS; i++) {
        threads[i] = test_start_thread_arg(waiter_entry, i);
        TEST_WAIT_FOR(ready[i]);
        test_wait_until_waiting(threads[i]);
    }
    sceKernelDelayThread(100000);
    char bytes[4] = { 0 };
    PRINT("send 4 bytes to W", sceKernelSendMsgPipe(pipes[W], bytes, 4, MSG_PIPE_MODE_FULL, &size, NULL));
    PRINT("receive 4 bytes from S's pipe", sceKernelReceiveMsgPipe(pipes[S], bytes, 4, MSG_PIPE_MODE_FULL, &size, NULL));
    PRINT("W's time left is written", written_below_2s(W));
    PRINT("S's time left is written", written_below_2s(S));
    for (int i = 0; i < WAITERS; i++)
        time_left[i] = SENTINEL;
    PRINT("notify W", sceKernelNotifyCallback(callbacks[W], 1));
    PRINT("notify S", sceKernelNotifyCallback(callbacks[S], 1));
    for (int i = 0; i < WAITERS; i++)
        test_join_thread(threads[i]);
    PRINT("W's receive", results[W]);
    PRINT("W's callback runs", callback_runs[W]);
    PRINT("W's time left is still the sentinel", time_left[W] == SENTINEL);
    PRINT("S's send", results[S]);
    PRINT("S's callback runs", callback_runs[S]);
    PRINT("S's time left is still the sentinel", time_left[S] == SENTINEL);

    test_exit();
    return 0;
}
