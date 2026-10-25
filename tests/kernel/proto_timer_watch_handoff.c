// Prototype check, not for merging: W1 and W2 wait on timer T, due at 200 ms, W2 with a timeout of
// 1 s. W1 is T's first waiter, so only W1 watches T's clock. W1 stops waiting at 50 ms, and W2
// becomes T's first waiter. T must wake W2 to watch its clock, so W2 gets T's event at 200 ms, with
// most of its timeout left, and not only at its deadline. A wait is either a multiple wait with OR
// on T and an event of its own, ended by a set of that event at 50 ms, or a plain wait on T, which
// for W1 times out at 50 ms. The cases: two multiple waits, a plain wait behind a multiple wait,
// and a multiple wait behind a plain wait.
#include <psp2/kernel/threadmgr.h>

#include "test.h"
#include "threadmgr_extra.h"

enum { MULTI, PLAIN };

static SceUID timer, events_of[2];
static int kinds[2];
static volatile int w_result[2];
static volatile int w_got_timer[2];
static volatile SceUInt w_time_left[2];
static ResultEvent results[2][2];

static int w_entry(SceSize args, void *argp) {
    (void)args;
    const int i = *(const int *)argp;
    SceUInt timeout = i == 0 && kinds[0] == PLAIN ? 50000 : 1000000;
    if (kinds[i] == MULTI) {
        WaitEvent events[2] = { { timer, EVENT_TIMER }, { events_of[i], 0x01000000 } };
        w_result[i] = sceKernelWaitMultipleEvents(events, 2, EVENT_WAIT_MODE_OR, results[i], &timeout);
        w_got_timer[i] = results[i][0].eventId == timer;
    } else {
        SceUInt32 bits = 0;
        w_result[i] = sceKernelWaitEvent(timer, EVENT_TIMER, &bits, NULL, &timeout);
        w_got_timer[i] = bits == EVENT_TIMER;
    }
    w_time_left[i] = timeout;
    return 0;
}

static void run_case(const char *name, int first, int second) {
    kinds[0] = first;
    kinds[1] = second;
    sceClibMemset(results, 0, sizeof(results));
    timer = sceKernelCreateTimer("timer", 0, NULL);
    events_of[0] = sceKernelCreateSimpleEvent("e1", 0, 0, NULL);
    events_of[1] = sceKernelCreateSimpleEvent("e2", 0, 0, NULL);
    SceUInt64 due = 200000;
    sceKernelSetTimerEvent(timer, TIMER_TYPE_SET_EVENT, &due, 0);
    sceKernelStartTimer(timer);
    const SceUID w1 = test_start_thread_arg(w_entry, 0);
    test_wait_until_waiting(w1);
    const SceUID w2 = test_start_thread_arg(w_entry, 1);
    test_wait_until_waiting(w2);
    sceKernelDelayThread(50000);
    if (first == MULTI)
        PRINTF(sceKernelSetEvent(events_of[0], 0x01000000, 0), "%s: set W1's event", name);
    test_join_thread(w1);
    test_join_thread(w2);
    PRINTF(w_result[0], "%s: W1's wait", name);
    PRINTF(w_got_timer[0], "%s: W1 got the timer", name);
    PRINTF(w_result[1], "%s: W2's wait", name);
    PRINTF(w_got_timer[1], "%s: W2 got the timer", name);
    PRINTF(w_time_left[1] > 500000, "%s: W2 had over 500 ms left", name);
    sceKernelStopTimer(timer);
    sceKernelDeleteTimer(timer);
    sceKernelDeleteSimpleEvent(events_of[0]);
    sceKernelDeleteSimpleEvent(events_of[1]);
}

int main(void) {
    run_case("two multiple waits", MULTI, MULTI);
    run_case("a plain wait behind a multiple wait", MULTI, PLAIN);
    run_case("a multiple wait behind a plain wait", PLAIN, MULTI);

    test_exit();
    return 0;
}
