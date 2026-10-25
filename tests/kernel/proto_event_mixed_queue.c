// Prototype check, not for merging: a plain wait P and a multiple event wait M on one auto reset
// simple event, queued in both orders. A set goes to the first in the queue and the other keeps
// waiting, a cancel then counts and ends the other one. With both waiting, a cancel counts and ends
// both, and a delete ends both.
#include <psp2/kernel/threadmgr.h>

#include "test.h"
#include "threadmgr_extra.h"

#define BIT 0x01000000

static SceUID event;
static volatile int p_done, m_done;
static volatile int p_result, m_result;
static ResultEvent m_results[1];

static int p_entry(SceSize args, void *argp) {
    (void)args;
    (void)argp;
    SceUInt timeout = 2000000;
    p_result = sceKernelWaitEvent(event, BIT, NULL, NULL, &timeout);
    p_done = 1;
    return 0;
}

static int m_entry(SceSize args, void *argp) {
    (void)args;
    (void)argp;
    WaitEvent events[1] = { { event, BIT } };
    SceUInt timeout = 2000000;
    m_result = sceKernelWaitMultipleEvents(events, 1, EVENT_WAIT_MODE_OR, m_results, &timeout);
    m_done = 1;
    return 0;
}

// Starts P and M, in that order or the other, each once the one before waits
static void start_both(int m_first, SceUID *p, SceUID *m) {
    p_done = m_done = 0;
    p_result = m_result = -1;
    sceClibMemset(m_results, 0, sizeof(m_results));
    event = sceKernelCreateSimpleEvent("e", EVENT_ATTR_AUTO_RESET, 0, NULL);
    if (m_first) {
        *m = test_start_thread(m_entry);
        test_wait_until_waiting(*m);
        *p = test_start_thread(p_entry);
        test_wait_until_waiting(*p);
    } else {
        *p = test_start_thread(p_entry);
        test_wait_until_waiting(*p);
        *m = test_start_thread(m_entry);
        test_wait_until_waiting(*m);
    }
}

static void set_then_cancel(int m_first) {
    const char *const name = m_first ? "M queued first" : "P queued first";
    SceUID p, m;
    start_both(m_first, &p, &m);
    PRINTF(sceKernelSetEvent(event, BIT, 0), "%s: set", name);
    TEST_WAIT_FOR(p_done || m_done);
    sceKernelDelayThread(50000);
    PRINTF(p_done, "%s: P's wait ended with the set", name);
    PRINTF(m_done, "%s: M's wait ended with the set", name);
    SceUInt32 count = 0;
    PRINTF(sceKernelCancelEvent(event, &count), "%s: cancel", name);
    PRINTF(count, "%s: the waits the cancel ended", name);
    test_join_thread(p);
    test_join_thread(m);
    PRINTF(p_result, "%s: P's wait", name);
    PRINTF(m_result, "%s: M's wait", name);
    PRINTF(m_results[0].result, "%s: M's result 0", name);
    sceKernelDeleteSimpleEvent(event);
}

static void end_both(int delete_event) {
    const char *const name = delete_event ? "both waiting, delete" : "both waiting, cancel";
    SceUID p, m;
    start_both(0, &p, &m);
    SceUInt32 count = 0;
    if (delete_event) {
        PRINTF(sceKernelDeleteSimpleEvent(event), "%s: the delete", name);
    } else {
        PRINTF(sceKernelCancelEvent(event, &count), "%s: the cancel", name);
        PRINTF(count, "%s: the waits the cancel ended", name);
    }
    test_join_thread(p);
    test_join_thread(m);
    PRINTF(p_result, "%s: P's wait", name);
    PRINTF(m_result, "%s: M's wait", name);
    PRINTF(m_results[0].result, "%s: M's result 0", name);
    if (!delete_event)
        sceKernelDeleteSimpleEvent(event);
}

int main(void) {
    set_then_cancel(0);
    set_then_cancel(1);
    end_both(0);
    end_both(1);

    test_exit();
    return 0;
}
