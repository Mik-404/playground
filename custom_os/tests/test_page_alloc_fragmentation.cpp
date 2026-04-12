#include "kernel/printk.h"
#include "mm/paging.h"
#include "tests/test_page_alloc_common.h"

void KernelPid1() noexcept {
    size_t allocated = 0;
    size_t allocated_user = 0;
    PageWithPointers* root = TestAllocAll(0, &allocated);

    // Free pages in cross order (first even pages, then odds). This is the worst case for linked list allocator.
    size_t freed = 0;
    for (size_t d = 0; d < 2; d++) {
        for (size_t i = 0; i < allocated; i++) {
            if (i % 2 != d) {
                continue;
            }
            size_t idx = i / MAX_PTRS;
            size_t pos = i % MAX_PTRS;
            void* ptr = ((PageWithPointers*)root->ptrs[idx])->ptrs[pos];
            if (!ptr) {
                continue;
            }
            mm::Page* page = mm::Page::FromAddr(ptr);
            FAIL_ON_NULL(page, "mm::Page::FromAddr returned NULL on address %p", ptr);
            mm::FreePage(page);
            freed++;
        }
    }

    // After this point, simple linked list allocator can't allocate two continous pages.
    for (size_t i = 0; i < 100; i++) {
        mm::Page* page = mm::AllocPage(1);
        FAIL_ON_NULL(page, "fragmentation occurred: allocated %lu out of %lu free pages", i * 2, allocated_user);
    }
    printk("[test] done\n");
    TEST_OK();
}
