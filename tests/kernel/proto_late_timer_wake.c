// Prototype scenario late_timer_wake, not for merging: W waits on a timer's event with a timeout of
// 100 ms. With VITA3K_PROTO_SCENARIO=late_timer_wake W's first wake from its park holds until 50 ms
// past its deadline, as a host that ran W late would. A timer event due at 30 ms must still end the
// wait, and one due at 150 ms, after the deadline, must not. With two timers listed in the reverse
// order of their events, due at 60 ms and 30 ms, the one due first must end the OR wait. A plain
// wait on one timer, the same with events due at 30 ms and at 120 ms.
#include <psp2/kernel/threadmgr.h>

#include "test.h"
#include "threadmgr_extra.h"

static ResultEvent results[2];

static void wait_for_timer(const char *name, SceUInt64 due) {
    const SceUID timer = sceKernelCreateTimer("timer", 0, NULL);
    sceKernelSetTimerEvent(timer, TIMER_TYPE_SET_EVENT, &due, 0);
    sceKernelStartTimer(timer);
    WaitEvent events[1] = { { timer, EVENT_TIMER } };
    SceUInt timeout = 100000;
    sceClibMemset(results, 0, sizeof(results));
    PRINTF(sceKernelWaitMultipleEvents(events, 1, EVENT_WAIT_MODE_OR, results, &timeout), "%s: the wait", name);
    PRINTF(results[0].resultPattern, "%s: result 0 bits", name);
    PRINTF(timeout, "%s: time left", name);
    sceKernelStopTimer(timer);
    sceKernelDeleteTimer(timer);
}

static SceUID start_timer(SceUInt64 due) {
    const SceUID timer = sceKernelCreateTimer("timer", 0, NULL);
    sceKernelSetTimerEvent(timer, TIMER_TYPE_SET_EVENT, &due, 0);
    sceKernelStartTimer(timer);
    return timer;
}

static void wait_for_two_timers(void) {
    const SceUID late = start_timer(60000);
    const SceUID early = start_timer(30000);
    WaitEvent events[2] = { { late, EVENT_TIMER }, { early, EVENT_TIMER } };
    SceUInt timeout = 100000;
    sceClibMemset(results, 0, sizeof(results));
    PRINT("two timers listed late first: the wait", sceKernelWaitMultipleEvents(events, 2, EVENT_WAIT_MODE_OR, results, &timeout));
    PRINT("two timers listed late first: result 0 is the timer due first", results[0].eventId == early);
    PRINT("two timers listed late first: result 0 bits", results[0].resultPattern);
    sceKernelStopTimer(late);
    sceKernelStopTimer(early);
    sceKernelDeleteTimer(late);
    sceKernelDeleteTimer(early);
}

static void wait_on_timer(const char *name, SceUInt64 due) {
    const SceUID timer = start_timer(due);
    SceUInt32 bits = 0;
    SceUInt timeout = 100000;
    PRINTF(sceKernelWaitEvent(timer, EVENT_TIMER, &bits, NULL, &timeout), "%s: the wait", name);
    PRINTF(bits, "%s: the bits", name);
    PRINTF(timeout, "%s: time left", name);
    sceKernelStopTimer(timer);
    sceKernelDeleteTimer(timer);
}

int main(void) {
    wait_for_timer("due before the deadline", 30000);
    wait_for_timer("due after the deadline", 150000);
    wait_for_two_timers();
    wait_on_timer("one timer, due before the deadline", 30000);
    wait_on_timer("one timer, due after the deadline", 120000);

    test_exit();
    return 0;
}
