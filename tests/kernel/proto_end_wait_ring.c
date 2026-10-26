// Prototype check, not for merging: four threads in a ring, each waiting for the next one's end
// with a short timeout over and over, so their looks at each other's status, their registrations
// and their timeout claims overlap on the four end queues. None of them ends during its rounds, so
// every wait times out. All four start their rounds together from a start gate, once every one of
// them runs, and each reports when it is done and then waits on a finish gate, which the test opens
// once all four are done, so no wait of the ring finds its target dormant. Vita3K must finish the
// rounds without its end queue and thread locks blocking each other.
#include <psp2/kernel/threadmgr.h>

#include "test.h"
#include "threadmgr_extra.h"

#define RING 4
#define ROUNDS 300
#define ROUND_US 1000

static SceUID ring[RING], start_gate, gate;
static volatile int started[RING], done[RING], timeouts[RING], others[RING];

static int ring_entry(SceSize args, void *argp) {
    (void)args;
    const int index = *(const int *)argp;
    const SceUID next = ring[(index + 1) % RING];
    started[index] = 1;
    SceUInt start_timeout = 5 * TEST_WAIT_US;
    sceKernelWaitSema(start_gate, 1, &start_timeout);
    for (int round = 0; round < ROUNDS; round++) {
        SceUInt timeout = ROUND_US;
        if (sceKernelWaitThreadEnd(next, NULL, &timeout) == (int)0x80028005)
            timeouts[index]++;
        else
            others[index]++;
    }
    done[index] = 1;
    SceUInt timeout = 5 * TEST_WAIT_US;
    sceKernelWaitSema(gate, 1, &timeout);
    return 0;
}

// Returns whether every thread of the ring set its flag
static int all_set(volatile int *flags) {
    for (int i = 0; i < RING; i++)
        if (!flags[i])
            return 0;
    return 1;
}

int main(void) {
    start_gate = sceKernelCreateSema("start gate", 0, 0, RING, NULL);
    gate = sceKernelCreateSema("gate", 0, 0, RING, NULL);
    for (int i = 0; i < RING; i++)
        ring[i] = sceKernelCreateThread("ring", ring_entry, SCE_KERNEL_DEFAULT_PRIORITY_USER, 0x4000, 0, 0, NULL);
    for (int i = 0; i < RING; i++)
        sceKernelStartThread(ring[i], sizeof(i), &i);
    TEST_WAIT_FOR(all_set(started));
    PRINT("all started", all_set(started));
    sceKernelSignalSema(start_gate, RING);
    TEST_WAIT_FOR(all_set(done));
    PRINT("all done", all_set(done));
    for (int i = 0; i < RING; i++) {
        PRINTF(timeouts[i], "thread %d: timeouts", i);
        PRINTF(others[i], "thread %d: other results", i);
    }
    sceKernelSignalSema(gate, RING);
    for (int i = 0; i < RING; i++)
        test_join_thread(ring[i]);
    sceKernelDeleteSema(start_gate);
    sceKernelDeleteSema(gate);
    test_exit();
    return 0;
}
