#pragma once

#include <cstring>

#include "mm/paging.h"
#include "mm/page_alloc.h"
#include "kernel/printk.h"
#include "tests/common.h"

constexpr size_t MAX_PTRS = PAGE_SIZE / sizeof(void*);

struct PageWithPointers  {
    void* ptrs[MAX_PTRS];
};

PageWithPointers* TestAllocAll(size_t order, size_t* allocated) noexcept;
void FreeAll(PageWithPointers* root) noexcept;
