// Prototype scenario exit_before_look, not for merging: W, then B, wait on an auto reset timer
// whose one shot event is due at 50 ms, and M waits with OR on a second such timer and an event of
// its own, all with a timeout of 2 s. W is the first waiter of its timer and M's token the first of
// M's, so each watches its clock and looks at its timer at 50 ms. With
// VITA3K_PROTO_SCENARIO=exit_before_look the first look of W and of M holds after its park until
// its thread is exiting, which comes with the process exit, so each finds its timer's event overdue
// only once it is exiting. Each must end its wait for the exit without firing the event, which a
// waiter still waiting may take, and Vita3K aborts if one got it.
#include <psp2/kernel/threadmgr.h>

#include "test.h"
#include "threadmgr_extra.h"

static SceUID timers[2], m_event;
static volatile int results[3] = { -1, -1, -1 };
static ResultEvent m_results[2];

static int plain_entry(SceSize args, void *argp) {
    (void)args;
    const int i = *(const int *)argp;
    SceUInt32 bits = 0;
    SceUInt timeout = 2000000;
    results[i] = sceKernelWaitEvent(timers[0], EVENT_TIMER, &bits, NULL, &timeout);
    return 0;
}

static int multi_entry(SceSize args, void *argp) {
    (void)args;
    (void)argp;
    WaitEvent events[2] = { { timers[1], EVENT_TIMER }, { m_event, 0x01000000 } };
    SceUInt timeout = 2000000;
    results[2] = sceKernelWaitMultipleEvents(events, 2, EVENT_WAIT_MODE_OR, m_results, &timeout);
    return 0;
}

static SceUID start_timer(void) {
    const SceUID timer = sceKernelCreateTimer("timer", EVENT_ATTR_AUTO_RESET, NULL);
    SceUInt64 due = 50000;
    sceKernelSetTimerEvent(timer, TIMER_TYPE_SET_EVENT, &due, 0);
    sceKernelStartTimer(timer);
    return timer;
}

int main(void) {
    m_event = sceKernelCreateSimpleEvent("m", 0, 0, NULL);
    timers[0] = start_timer();
    timers[1] = start_timer();
    const SceUID w = test_start_thread_arg(plain_entry, 0);
    test_wait_until_waiting(w);
    const SceUID b = test_start_thread_arg(plain_entry, 1);
    test_wait_until_waiting(b);
    const SceUID m = test_start_thread(multi_entry);
    test_wait_until_waiting(m);
    // Nothing looks at the timers from here, so only W and M can find their events overdue
    sceKernelDelayThread(200000);
    PRINT("W returned before the exit", results[0] != -1);
    PRINT("B returned before the exit", results[1] != -1);
    PRINT("M returned before the exit", results[2] != -1);

    test_exit();
    return 0;
}
