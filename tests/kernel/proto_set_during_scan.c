// Prototype scenario set_during_scan, not for merging: an OR wait on E0 and E1. With
// VITA3K_PROTO_SCENARIO=set_during_scan W's scan holds after it queued E0's token until another
// party ended that entry, so the set of E0 comes during the scan. The scan still queues E1's token,
// and the arm must count only the tokens still waiting, so the wait ends without sleeping.
#include <psp2/kernel/threadmgr.h>

#include "test.h"
#include "threadmgr_extra.h"

static SceUID e0, e1;
static volatile int w_started, w_done;
static volatile int w_result = -1;
static ResultEvent results[2];

static int w_entry(SceSize args, void *argp) {
    (void)args;
    (void)argp;
    WaitEvent events[2] = { { e0, 0x01000000 }, { e1, 0x01000000 } };
    SceUInt timeout = 1000000;
    w_started = 1;
    w_result = sceKernelWaitMultipleEvents(events, 2, EVENT_WAIT_MODE_OR, results, &timeout);
    w_done = 1;
    return 0;
}

int main(void) {
    e0 = sceKernelCreateSimpleEvent("e0", EVENT_ATTR_AUTO_RESET, 0, NULL);
    e1 = sceKernelCreateSimpleEvent("e1", EVENT_ATTR_AUTO_RESET, 0, NULL);
    const SceUID w = test_start_thread(w_entry);
    TEST_WAIT_FOR(w_started);
    sceKernelDelayThread(100000);
    SceKernelThreadInfo info = { .size = sizeof(info) };
    sceKernelGetThreadInfo(w, &info);
    PRINT("W's wait type during the scan", info.waitType);
    PRINT("set E0 during the scan", sceKernelSetEvent(e0, 0x01000000, 0x55));
    TEST_WAIT_FOR(w_done);
    test_join_thread(w);
    PRINT("W's wait", w_result);
    PRINT("result 0 is for E0", results[0].eventId == e0);
    PRINT("result 0", results[0].result);
    PRINT("result 0 bits", results[0].resultPattern);
    PRINT("result 0 user data", (int)results[0].userData);
    SceUInt32 pattern = 0;
    sceKernelGetEventPattern(e0, &pattern);
    PRINT("E0's bits after", pattern & 0xFF000000);
    EventInfo e1_info = { .size = sizeof(e1_info) };
    sceKernelGetEventInfo(e1, &e1_info);
    PRINT("E1's waiters after", e1_info.numWaitThreads);

    test_exit();
    return 0;
}
