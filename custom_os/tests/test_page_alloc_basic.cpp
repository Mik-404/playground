#include <cstring>

#include "mm/paging.h"
#include "mm/page_alloc.h"
#include "kernel/printk.h"
#include "tests/test_page_alloc_common.h"

void KernelPid1() noexcept {
    size_t available = mm::FreePagesCount();
    FAIL_ON(available < 32000, "too few free pages (%lu) reported by page allocator", available);

    for (size_t order = 0; order < mm::PAGE_MAX_ALLOCATION_ORDER; order++) {
        size_t new_allocated = 0;
        PageWithPointers* root = TestAllocAll(order, &new_allocated);
        printk("[test] allocated: %d\n", new_allocated);
        FAIL_ON(available != new_allocated, "memory leak: allocated %lu out of %lu pages", new_allocated, available);
        size_t free_cnt = mm::FreePagesCount();
        FAIL_ON(free_cnt != 0, "page_alloc returned NULL, but there are %lu free pages reported", free_cnt);
        FreeAll(root);
    }

    printk("[test] done\n");
    TEST_OK();
}
