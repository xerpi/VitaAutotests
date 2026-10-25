// Prototype scenario exit_after_completion, not for merging: what the claim that ends a message
// pipe wait writes, read before the waiter runs. Four pipes, each waiter with a timeout of 2 s and
// its time left and size in globals. W1 receives 4 bytes FULL and the main thread sends 4, so the
// send ends a waiting receiver. S1 sends 4 bytes FULL into a full pipe and the main thread receives
// 4, so the receive ends a waiting sender. W2 receives with callbacks and the main thread notifies
// it, so the notify ends its wait. W3 receives 8 bytes FULL and the main thread sends 4, a partial
// transfer. With VITA3K_PROTO_SCENARIO=exit_after_completion a waiter whose wait ended goes on only
// once it is exiting, so the main thread reads the globals while W1, S1 and W2 are held after their
// claims: their time left is written, below 2 s, while W3 has its size but its time left untouched
// and still waits.
#include <psp2/kernel/threadmgr.h>

#include "test.h"
#include "threadmgr_extra.h"

#define CAPACITY 0x1000

enum { W1, S1, W2, W3, WAITERS };

static SceUID pipes[WAITERS], callback;
static volatile SceUInt time_left[WAITERS];
static volatile SceSize sizes[WAITERS];
static volatile int returned[WAITERS], callback_runs, w2_ready;
static char buffers[WAITERS][8];
static char fill[CAPACITY];

static int callback_entry(int notify_id, int count, int arg, void *common) {
    (void)notify_id;
    (void)count;
    (void)arg;
    (void)common;
    callback_runs++;
    return 0;
}

static int waiter_entry(SceSize args, void *argp) {
    (void)args;
    const int i = *(const int *)argp;
    SceUInt *timeout = (SceUInt *)&time_left[i];
    SceSize *size = (SceSize *)&sizes[i];
    switch (i) {
    case W1: sceKernelReceiveMsgPipe(pipes[i], buffers[i], 4, MSG_PIPE_MODE_FULL, size, timeout); break;
    case S1: sceKernelSendMsgPipe(pipes[i], buffers[i], 4, MSG_PIPE_MODE_FULL, size, timeout); break;
    case W2:
        callback = sceKernelCreateCallback("cb", 0, callback_entry, NULL);
        w2_ready = 1;
        sceKernelReceiveMsgPipeCB(pipes[i], buffers[i], 4, MSG_PIPE_MODE_FULL, size, timeout);
        break;
    case W3: sceKernelReceiveMsgPipe(pipes[i], buffers[i], 8, MSG_PIPE_MODE_FULL, size, timeout); break;
    }
    returned[i] = 1;
    return 0;
}

static SceKernelMppInfo pipe_info(SceUID pipe) {
    SceKernelMppInfo info = { .size = sizeof(info) };
    sceKernelGetMsgPipeInfo(pipe, &info);
    return info;
}

static int written_below_2s(int i) {
    return time_left[i] > 1000000 && time_left[i] < 2000000;
}

int main(void) {
    for (int i = 0; i < WAITERS; i++) {
        pipes[i] = sceKernelCreateMsgPipe("pipe", MSG_PIPE_TYPE_USER, 0, CAPACITY, NULL);
        time_left[i] = 2000000;
    }
    SceSize filled = 0;
    PRINT("fill S1's pipe", sceKernelSendMsgPipe(pipes[S1], fill, CAPACITY, MSG_PIPE_MODE_FULL, &filled, NULL));
    for (int i = 0; i < WAITERS; i++) {
        const SceUID thread = test_start_thread_arg(waiter_entry, i);
        if (i == W2)
            TEST_WAIT_FOR(w2_ready);
        test_wait_until_waiting(thread);
    }
    sceKernelDelayThread(100000);
    char bytes[8] = { 0 };
    SceSize size = 0;
    PRINT("send 4 bytes to W1", sceKernelSendMsgPipe(pipes[W1], bytes, 4, MSG_PIPE_MODE_FULL, &size, NULL));
    PRINT("receive 4 bytes from S1's pipe", sceKernelReceiveMsgPipe(pipes[S1], bytes, 4, MSG_PIPE_MODE_FULL, &size, NULL));
    PRINT("notify W2", sceKernelNotifyCallback(callback, 1));
    PRINT("send 4 bytes to W3", sceKernelSendMsgPipe(pipes[W3], bytes, 4, MSG_PIPE_MODE_FULL, &size, NULL));
    PRINT("W1's time left is written", written_below_2s(W1));
    PRINT("W1's size", sizes[W1]);
    PRINT("S1's time left is written", written_below_2s(S1));
    PRINT("S1's size", sizes[S1]);
    PRINT("W2's time left is written", written_below_2s(W2));
    PRINT("W2's callback runs", callback_runs);
    PRINT("W3's time left", time_left[W3]);
    PRINT("W3's size", sizes[W3]);
    PRINT("W3's pipe receivers", pipe_info(pipes[W3]).numReceiveWaitThreads);
    int any_returned = 0;
    for (int i = 0; i < WAITERS; i++)
        any_returned |= returned[i];
    PRINT("a waiter returned before the exit", any_returned);

    test_exit();
    return 0;
}
