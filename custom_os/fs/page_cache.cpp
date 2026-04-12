#include "fs/page_cache.h"
#include "kernel/wait.h"
#include "kernel/panic.h"
#include "kernel/time.h"

constexpr size_t MAX_PAGE_WAIT_QUEUES_BITS = 8;

namespace {

kern::WaitQueue page_wait_queues[1 << MAX_PAGE_WAIT_QUEUES_BITS];

kern::WaitQueue& PageWaitQueue(mm::Page& page) {
    uintptr_t idx = (((uintptr_t)page.Virt()) >> PAGE_SIZE_BITS) & ((1 << MAX_PAGE_WAIT_QUEUES_BITS) - 1);
    return page_wait_queues[idx];
}

SpinLock ditry_pages_lock;
ListHead<mm::Page, &mm::Page::pc_dirty_list> dirty_pages;

}

void PageCache::MarkDirty(mm::Page& page) noexcept {
    if (page.TestAndSetFlag(mm::Page::Dirty)) {
        // Page already was dirty.
        return;
    }

    IrqSafeScopeLocker locker(ditry_pages_lock);
    dirty_pages.InsertLast(page);
}

void PageCache::LockPage(mm::Page& page) noexcept {
    PageWaitQueue(page).WaitCond([&]() {
        return !page.TestAndSetFlag(mm::Page::Locked);
    });
}

void PageCache::UnlockPage(mm::Page& page) noexcept {
    page.ClearFlag(mm::Page::Locked);
    PageWaitQueue(page).WakeAll();
}

mm::Page* PageCache::GetPageLocked(size_t index) noexcept {
    for (mm::Page& page : pages_head_) {
        if (index == page.pc_index) {
            return &page;
        }
    }
    return nullptr;
}

mm::Page* PageCache::GetPage(size_t index) noexcept {
    mm::Page* page = WithIrqSafeLocked(lock_, [&]() {
        return GetPageLocked(index);
    });

    if (page) {
        return page;
    }

    mm::Page* new_page = mm::AllocPage(0);
    if (!new_page) {
        return nullptr;
    }
    new_page->pc_index = index;
    new_page->SetFlag(mm::Page::InFileCache);
    new_page->Ref();

    // Check if someone have inserted page while we were in the allocator.
    page = WithIrqSafeLocked(lock_, [&]() {
        mm::Page* p = GetPageLocked(index);
        if (p) {
            return p;
        }
        pages_head_.InsertFirst(*new_page);
        return new_page;
    });

    if (page != new_page) {
        mm::FreePage(new_page);
    }

    return page;
}
