// Prototype check, not for merging: a thread signal sent to a thread waiting on something else
// wakes its host thread to look again, and the wait must go on. For a semaphore, an event flag, a
// simple event and a timer, W waits with a timeout of 2 s, and the main thread sends W a signal at
// 50 ms. W must still be waiting at 100 ms. Then the object ends the wait: a signal, a set, or the
// timer's event due at 300 ms. W's signal wait after that returns at once with the signal sent
// before.
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/threadmgr/signal.h>

#include "test.h"
#include "threadmgr_extra.h"

enum { SEMA, FLAG, EVENT, TIMER };

static SceUID object;
static int kind;
static volatile int w_result = -1, w_done, w_signal_wait = -1;
static volatile SceUInt w_time_left;

static int w_entry(SceSize args, void *argp) {
    (void)args;
    (void)argp;
    SceUInt timeout = 2000000;
    unsigned int flag_bits = 0;
    SceUInt32 bits = 0;
    switch (kind) {
    case SEMA: w_result = sceKernelWaitSema(object, 1, &timeout); break;
    case FLAG: w_result = sceKernelWaitEventFlag(object, 0x1, SCE_EVENT_WAITAND, &flag_bits, &timeout); break;
    case EVENT: w_result = sceKernelWaitEvent(object, 0x01000000, &bits, NULL, &timeout); break;
    case TIMER: w_result = sceKernelWaitEvent(object, EVENT_TIMER, &bits, NULL, &timeout); break;
    }
    w_time_left = timeout;
    w_done = 1;
    SceUInt signal_timeout = 100000;
    w_signal_wait = sceKernelWaitSignal(0, 0, &signal_timeout);
    return 0;
}

static void run_case(const char *name, int k) {
    kind = k;
    w_result = -1;
    w_done = 0;
    w_signal_wait = -1;
    switch (k) {
    case SEMA: object = sceKernelCreateSema("s", 0, 0, 1, NULL); break;
    case FLAG: object = sceKernelCreateEventFlag("f", 0, 0, NULL); break;
    case EVENT: object = sceKernelCreateSimpleEvent("e", 0, 0, NULL); break;
    case TIMER: {
        object = sceKernelCreateTimer("t", 0, NULL);
        SceUInt64 due = 300000;
        sceKernelSetTimerEvent(object, TIMER_TYPE_SET_EVENT, &due, 0);
        sceKernelStartTimer(object);
        break;
    }
    }
    const SceUID w = test_start_thread(w_entry);
    test_wait_until_waiting(w);
    sceKernelDelayThread(50000);
    PRINTF(sceKernelSendSignal(w), "%s: send W a signal", name);
    sceKernelDelayThread(50000);
    PRINTF(w_done, "%s: W's wait ended after the signal", name);
    switch (k) {
    case SEMA: sceKernelSignalSema(object, 1); break;
    case FLAG: sceKernelSetEventFlag(object, 0x1); break;
    case EVENT: sceKernelSetEvent(object, 0x01000000, 0); break;
    case TIMER: break;
    }
    test_join_thread(w);
    PRINTF(w_result, "%s: W's wait", name);
    PRINTF(w_time_left > 1000000, "%s: W had over 1 s left", name);
    PRINTF(w_signal_wait, "%s: W's signal wait", name);
    switch (k) {
    case SEMA: sceKernelDeleteSema(object); break;
    case FLAG: sceKernelDeleteEventFlag(object); break;
    case EVENT: sceKernelDeleteSimpleEvent(object); break;
    case TIMER:
        sceKernelStopTimer(object);
        sceKernelDeleteTimer(object);
        break;
    }
}

int main(void) {
    run_case("semaphore", SEMA);
    run_case("event flag", FLAG);
    run_case("simple event", EVENT);
    run_case("timer", TIMER);

    test_exit();
    return 0;
}
