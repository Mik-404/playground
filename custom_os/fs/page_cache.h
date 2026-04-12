#pragma once

#include "kernel/wait.h"
#include "lib/list.h"
#include "lib/spinlock.h"
#include "mm/page_alloc.h"
#include "fs/buffer.h"

class PageCache {
private:
    SpinLock lock_;
    ListHead<mm::Page, &mm::Page::pc_list> pages_head_;

    mm::Page* GetPageLocked(size_t index) noexcept;

public:
    PageCache() = default;
    mm::Page* GetPage(size_t index) noexcept;

public:
    static void MarkDirty(mm::Page& page) noexcept;
    static void LockPage(mm::Page& page) noexcept;
    static void UnlockPage(mm::Page& page) noexcept;
};
