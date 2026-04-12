#include "mm/page_alloc.h"
#include "mm/new.h"
#include "lib/locking.h"

namespace mm {

extern PageAlloc page_allocator;

namespace {

SpinLock page_alloc_lock;

}
void DoInitArea(PageAllocArea* area) noexcept {
    for (size_t i = 0; i < area->size_in_pages; i++) {
        new (&area->pages[i]) Page(area);
        area->pages[i].SetFlag(Page::Used);
    }

    size_t current_idx = 0;

    while (current_idx < area->size_in_pages) {
        size_t order = 0;
        
        while (order < PAGE_MAX_ALLOCATION_ORDER) {
            size_t next_order_size = 1 << (order + 1);
            if ((current_idx % next_order_size == 0) && 
                (current_idx + next_order_size <= area->size_in_pages)) {
                order++;
            } else {
                break;
            }
        }

        Page* page = &area->pages[current_idx];
        page->SetOrder(order);

        size_t block_size = 1 << order;
        for (size_t i = 0; i < block_size; i++) {
            area->pages[current_idx + i].ClearFlag(Page::Used);
        }

        area->free_lists[order].InsertLast(*page);

        current_idx += block_size;
    }
}

Page* DoAllocPage(size_t order, AllocFlags) noexcept {
    IrqSafeScopeLocker locker(page_alloc_lock);

    for (size_t i = 0; i < page_allocator.area_count; i++) {
        PageAllocArea& area = page_allocator.areas[i];

        size_t current_order = order;
        while (current_order <= PAGE_MAX_ALLOCATION_ORDER && area.free_lists[current_order].Empty()) {
            current_order++;
        }

        if (current_order > PAGE_MAX_ALLOCATION_ORDER) {
            continue;
        }

        Page* page = &area.free_lists[current_order].First();
        page->pa_free_list.Remove();

        while (current_order > order) {
            current_order--;
            size_t half_size = 1 << current_order;
            
            Page* buddy = page + half_size;
            
            buddy->SetOrder(current_order);
            area.free_lists[current_order].InsertLast(*buddy);
        }

        page->SetOrder(order);
        
        size_t page_cnt = 1 << order;
        for (size_t j = 0; j < page_cnt; j++) {
            page[j].SetFlag(Page::Used);
        }

        return page;
    }

    return nullptr;
}

void DoFreePage(Page* page) noexcept {
    IrqSafeScopeLocker locker(page_alloc_lock);

    PageAllocArea* area = page->Area();
    size_t order = page->Order();
    
    size_t page_cnt = 1 << order;
    for (size_t i = 0; i < page_cnt; i++) {
        page[i].ClearFlag(Page::Used);
    }

    while (order < PAGE_MAX_ALLOCATION_ORDER) {
        size_t page_idx = page->Id();
        size_t buddy_idx = page_idx ^ (1 << order);
        
        if (buddy_idx >= area->size_in_pages) break; 

        Page* buddy = &area->pages[buddy_idx];

        if (buddy->HasFlag(Page::Used) || buddy->Order() != order) {
            break; 
        }

        buddy->pa_free_list.Remove();
        
        order++;
        
        if (buddy_idx < page_idx) {
            page = buddy;
        }
    }

    page->SetOrder(order);
    area->free_lists[order].InsertLast(*page);
}

}

/*
==LEGACY(0_0)==

Page* DoAllocPage(size_t order, AllocFlags) noexcept {
    IrqSafeScopeLocker locker(page_alloc_lock);

    size_t page_cnt = 1 << order;
    for (size_t i = 0; i < page_allocator.area_count; i++) {
        PageAllocArea& area = page_allocator.areas[i];
        for (size_t page_idx = 0; page_idx + page_cnt < area.size_in_pages; page_idx++) {
            bool free = true;
            for (size_t j = 0; j < page_cnt; j++) {
                if (area.pages[page_idx + j].HasFlag(Page::Used)) {
                    free = false;
                    break;
                }
            }
            if (free) {
                for (size_t j = 0; j < page_cnt; j++) {
                    area.pages[page_idx + j].SetFlag(Page::Used);
                }
                area.pages[page_idx].SetOrder(order);
                return &area.pages[page_idx];
            }
        }
    }

    return nullptr;
}

void DoFreePage(Page* page) noexcept {
    size_t page_cnt = 1 << page->Order();
    for (size_t i = 0; i < page_cnt; i++) {
        page[i].ClearFlag(Page::Used);
    }
}

void DoInitArea(PageAllocArea* area) noexcept {
    for (size_t i = 0; i < area->size_in_pages; i++) {
        new (&area->pages[i]) Page(area);
        area->pages[i].ClearFlag(Page::Used);
    }
}
*/