// The module manager's start, stop, load and start, and stop and unload syscalls with their 12 byte
// argument block {flags, option, status} in the last 12 bytes of a memory block, when the page
// after it isn't mapped, and the same block 0x10 bytes from the end. The module id names no module
// and the path names no file, so a call that gets past its copy fails on those.
//
// On a PS Vita TV on firmware 3.65 the block at the end gives 0x80022005, as each call copies 0x10
// bytes of it. The block 0x10 bytes from the end gets past the copy.
#include <psp2/kernel/sysmem.h>

#include "syscalls.h"
#include "test.h"

#define NO_MODULE 0x7FFFFFF1

static int status;

// Tells if any memory block or other mapping holds addr, by both lookups
static int is_mapped(void *addr) {
    SceKernelMemBlockInfo info;
    info.size = sizeof(info);
    return sceKernelFindMemBlockByAddr(addr, 0) >= 0 || sceKernelGetMemBlockInfoByAddr(addr, &info) >= 0;
}

int main(void) {
    // Pages are placed where the allocator likes, so take the first one with no page mapped after it
    SceUID blocks[16];
    SceUID block = -1;
    char *page = NULL;
    int count = 0;
    while (count < 16 && block < 0) {
        blocks[count] = sceKernelAllocMemBlock("page", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, 0x1000, NULL);
        if (blocks[count] < 0) {
            PRINT("allocate a page", blocks[count]);
            test_exit();
            return 0;
        }
        void *base = NULL;
        sceKernelGetMemBlockBase(blocks[count], &base);
        if (!is_mapped((char *)base + 0x1000)) {
            block = blocks[count];
            page = base;
        }
        count++;
    }
    for (int i = 0; i < count; i++)
        if (blocks[i] != block)
            sceKernelFreeMemBlock(blocks[i]);
    if (block < 0) {
        test_skip("no page with nothing mapped after it");
        return 0;
    }
    ModuleCallBlock *const end = (ModuleCallBlock *)(page + 0x1000 - 12);
    ModuleCallBlock *const inside = (ModuleCallBlock *)(page + 0x1000 - 0x10);
    *end = (ModuleCallBlock){ 0, NULL, &status };
    PRINT("start with the block at the end", syscall_start_module(NO_MODULE, 0, NULL, end));
    PRINT("stop with the block at the end", syscall_stop_module(NO_MODULE, 0, NULL, end));
    PRINT("load and start with the block at the end", syscall_load_start_module("app0:missing.suprx", 0, NULL, end));
    PRINT("stop and unload with the block at the end", syscall_stop_unload_module(NO_MODULE, 0, NULL, end));
    *inside = (ModuleCallBlock){ 0, NULL, &status };
    PRINT("start with the block 0x10 from the end", syscall_start_module(NO_MODULE, 0, NULL, inside));
    PRINT("stop with the block 0x10 from the end", syscall_stop_module(NO_MODULE, 0, NULL, inside));
    PRINT("load and start with the block 0x10 from the end", syscall_load_start_module("app0:missing.suprx", 0, NULL, inside));
    PRINT("stop and unload with the block 0x10 from the end", syscall_stop_unload_module(NO_MODULE, 0, NULL, inside));

    sceKernelFreeMemBlock(block);
    test_exit();
    return 0;
}
