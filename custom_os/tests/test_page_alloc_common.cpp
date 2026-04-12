#include "tests/test_page_alloc_common.h"

void* TestAlloc(size_t order) noexcept {
    mm::Page* page = mm::AllocPage(order);
    if (!page) {
        return nullptr;
    }
    FAIL_ON_INVALID_PTR(page->Virt());
    FAIL_IF_UNALIGNED(page->Virt(), PAGE_SIZE, "page_alloc returned non-aligned pointer: %p\n", page->Virt());
#ifdef CONFIG_TEST_PAGE_ALLOC_TRACE_PTRS
    printk<LogLevel::Trace>("[test] allocated ptr: %p\n", page->Virt());
#endif
    return page->Virt();
}

PageWithPointers* TestAllocAll(size_t order, size_t* allocated) noexcept {
    size_t total = 0;
    PageWithPointers* root = (PageWithPointers*)TestAlloc(0);
    FAIL_ON_NULL(root, "mm::AllocPage returned nullptr, but there are available pages");
    memset(root, '\0', sizeof(void*));
    total++;

    for (size_t i = 0; i < MAX_PTRS; i++) {
        root->ptrs[i] = TestAlloc(0);
        FAIL_ON_NULL(root->ptrs[i], "mm::AllocPage returned nullptr, but there is available memory");
        memset(root->ptrs[i], '\0', PAGE_SIZE);
        total++;
    }

    size_t curr_page = 0;
    size_t curr_ptr_idx = 0;
    for (;;) {
        void* page = TestAlloc(order);
        if (!page) {
            if (order == 0) {
                break;
            }
            order--;
            continue;
        }
        total += 1 << order;
        PageWithPointers* ptrpage = (PageWithPointers*)root->ptrs[curr_page];
        ptrpage->ptrs[curr_ptr_idx] = page;
        curr_ptr_idx++;
        if (curr_ptr_idx == MAX_PTRS) {
            curr_page++;
            FAIL_ON(curr_page == MAX_PTRS, "too many pages returned by mm::PageAlloc");
            curr_ptr_idx = 0;
        }
    }
    *allocated = total;
    return root;
}

void DoFree(void* addr) noexcept {
    FAIL_ON_INVALID_PTR(addr);
    FAIL_IF_UNALIGNED(addr, PAGE_SIZE, "addr is not page-size aligned");
    mm::Page* page = mm::Page::FromAddr(addr);
    FAIL_ON_NULL(page, "page_from_addr returned NULL for previously allocated address %p", addr)
    mm::FreePage(page);
#ifdef CONFIG_TEST_PAGE_ALLOC_TRACE_PTRS
    printk<LogLevel::Trace>("[test] free ptr: %p\n", addr);
#endif
}

void FreeAll(PageWithPointers* root) noexcept {
    for (size_t curr_page = 0; curr_page < MAX_PTRS; curr_page++) {
        PageWithPointers* page = (PageWithPointers*)root->ptrs[curr_page];
        for (size_t i = 0; i < MAX_PTRS; i++) {
            if (page->ptrs[i] == NULL) {
                break;
            }
            DoFree(page->ptrs[i]);
        }
        DoFree(page);
    }
    DoFree(root);
}
