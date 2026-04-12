#include "tests/common.h"
#include "kernel/multiboot.h"

void KernelPid1() noexcept {
#ifdef FAKE_MAGIC
    FAIL("incorrect magic was not handled");
#endif

    multiboot::MemoryMapIter it;

    size_t cnt = 0;
    size_t total_mem = 0;
    const multiboot::MemoryMapEntry* entry;
    while ((entry = it.Next())) {
        cnt++;
        if (entry->type == MULTIBOOT_MMAP_TYPE_RAM) {
            total_mem += entry->length;
        }
    }
    FAIL_ON(cnt != 7, "expected 7 memory regions");
    FAIL_ON(total_mem < 511 * (1 << 20), "expected ~512 Mbytes RAM, got %lu\n", total_mem);

    TEST_OK();
}
