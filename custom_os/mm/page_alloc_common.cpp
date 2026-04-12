#include <algorithm>
#include <array>

#include "defs.h"
#include "kernel/multiboot.h"
#include "kernel/panic.h"
#include "lib/common.h"
#include "lib/list.h"
#include "lib/spinlock.h"
#include "mm/page_alloc.h"
#include "mm/paging.h"
#include "mm/kasan.h"
#include "linker.h"

namespace mm {

PageAlloc page_allocator;

void DoFreePage(Page* page) noexcept;
Page* DoAllocPage(size_t order, AllocFlags flags) noexcept;
void DoInitArea(PageAllocArea* area) noexcept;

static bool early_page_alloc_enabled = true;

Page* Page::FromAddr(void* ptr) noexcept {
    uintptr_t addr = (uintptr_t)ptr;
    for (size_t i = 0; i < page_allocator.area_count; i++) {
        PageAllocArea* area = &page_allocator.areas[i];
        if (area->base <= addr && addr < area->base + area->size_in_pages * PAGE_SIZE) {
            size_t page_idx = (addr - area->base) / PAGE_SIZE;
            Page* page = &area->pages[page_idx];
            BUG_ON(!page->HasFlag(Page::Used));
            BUG_ON(page->Area() != area);
            return page;
        }
    }
    return nullptr;
}

size_t Page::Id() const noexcept {
    return this - Area()->pages;
}

PageAllocArea* Page::Area() const noexcept {
    return &page_allocator.areas[area_idx];
}

Page::Page(PageAllocArea* area) noexcept {
    area_idx = area - &page_allocator.areas[0];
    virt = (area->base + Id() * PAGE_SIZE) / PAGE_SIZE;
    flags.store(0, std::memory_order_relaxed);
}

Page* AllocPage(size_t order, mm::AllocFlags flags) noexcept {
    UNUSED(flags);

    if (order >= PAGE_MAX_ALLOCATION_ORDER) {
        return nullptr;
    }

    Page* page = DoAllocPage(order, flags);

    if (page) {
        page->Area()->pages_allocated.fetch_add(1 << page->Order(), std::memory_order_relaxed);
        BUG_ON(Page::FromAddr(page->Virt()) != page);

        if (!flags.Has(AllocFlag::SkipKasan)) {
            kasan::Unpoison((uintptr_t)page->Virt(), (1 << page->Order()) * PAGE_SIZE);
        }
    }

    return page;
}

void FreePage(Page* page) noexcept {
    BUG_ON_NULL(page);
    BUG_ON(!page->HasFlag(Page::Used));

    kasan::Poison((uintptr_t)page->Virt(), (1 << page->Order()) * PAGE_SIZE);
    page->Area()->pages_allocated.fetch_sub(1 << page->Order(), std::memory_order_relaxed);

    DoFreePage(page);
}


constexpr int START_FREE = 0;
constexpr int END_FREE = 1;
constexpr int START = 2;
constexpr int END = 3;
constexpr size_t MAX_PRESERVED_AREAS = 10;

static size_t preserved_area_points_total = 0;
struct PreservedAreaPoint {
    uintptr_t addr;
    int type;
};
static std::array<PreservedAreaPoint, MAX_PRESERVED_AREAS * 2> preserved_area_points;

// PreserveMemoryArea marks memory area as allocated, all pages in this area are reserved and not allocated by page allocator.
void PreserveMemoryArea(uintptr_t addr, size_t sz) noexcept {
    if (preserved_area_points_total >= preserved_area_points.size()) {
        panic("cannot preserve more than %lu areas\n", MAX_PRESERVED_AREAS);
    }
    addr = ALIGN_DOWN(addr, PAGE_SIZE);
    sz = ALIGN_UP(sz, PAGE_SIZE);
    preserved_area_points[preserved_area_points_total++] = (PreservedAreaPoint){ .addr = addr, .type = START };
    preserved_area_points[preserved_area_points_total++] = (PreservedAreaPoint){ .addr = addr + sz, .type = END };
    printk("[mm] preserve %p-%p\n", addr, addr + sz);
}

static void AddArea(uintptr_t start, size_t size_in_pages) noexcept {
    if (size_in_pages == 0) {
        return;
    }

    if (page_allocator.area_count >= MAX_ALLOCATION_AREAS) {
        panic("trying to add too many allocation areas");
    }

    PageAllocArea* area = new (&page_allocator.areas[page_allocator.area_count++]) PageAllocArea();
    area->base = start;
    area->size_in_pages = size_in_pages;
    area->pages_total = size_in_pages;
    area->pages_allocated = 0;

    page_allocator.page_count += size_in_pages;
}

// CollectAreas obtains memory areas of usable RAM from Multiboot2 and adds them to the page allocator.
static void CollectAreas() noexcept {
    multiboot::MemoryMapIter it;
    for (;;) {
        const multiboot::MemoryMapEntry* mmap_entry = it.Next();
        if (!mmap_entry) {
            break;
        }

        if (mmap_entry->type != MULTIBOOT_MMAP_TYPE_RAM) {
            continue;
        }

        preserved_area_points[preserved_area_points_total++] = (PreservedAreaPoint){
            .addr = (uintptr_t)PHYS_TO_VIRT(mmap_entry->base_addr),
            .type = START_FREE
        };
        preserved_area_points[preserved_area_points_total++] = (PreservedAreaPoint){
            .addr = (uintptr_t)PHYS_TO_VIRT(mmap_entry->base_addr) + ALIGN_UP(mmap_entry->length, PAGE_SIZE),
            .type = END_FREE
        };
    }

    std::sort(preserved_area_points.begin(), preserved_area_points.begin() + preserved_area_points_total, [&](const PreservedAreaPoint& a, const PreservedAreaPoint& b) {
        return a.addr < b.addr || (a.addr == b.addr && a.type < b.type);
    });

    int preserved_cnt = 0;
    bool in_free_ram = false;
    uintptr_t start = 0;
    for (size_t i = 0; i < preserved_area_points_total; i++) {
        PreservedAreaPoint p = preserved_area_points[i];
        if (p.type == START_FREE) {
            start = p.addr;
            in_free_ram = true;
        } else if (p.type == END_FREE) {
            if (in_free_ram && preserved_cnt == 0) {
                AddArea(start, (p.addr - start) / PAGE_SIZE);
            }
            in_free_ram = false;
        } else if (p.type == START) {
            if (in_free_ram && preserved_cnt == 0) {
                AddArea(start, (p.addr - start) / PAGE_SIZE);
            }
            preserved_cnt++;
        } else if (p.type == END) {
            BUG_ON(preserved_cnt == 0);
            if (in_free_ram) {
                start = p.addr;
            }
            preserved_cnt--;
        }
    }
}

void InitEarlyPageAlloc() noexcept {
    page_allocator.page_count = 0;
    CollectAreas();
    early_page_alloc_enabled = true;
}

void* EarlyAllocPage(size_t pages, mm::AllocFlags flags) noexcept {
    BUG_ON(!early_page_alloc_enabled);
    UNUSED(flags);

    for (size_t i = 0; i < page_allocator.area_count; i++) {
        PageAllocArea* area = &page_allocator.areas[i];
        if (area->size_in_pages < pages) {
            continue;
        }

        area->size_in_pages -= pages;
        page_allocator.page_count -= pages;
        uintptr_t addr = area->base + area->size_in_pages * PAGE_SIZE;

        if (!flags.Has(AllocFlag::SkipKasan)) {
            kasan::Unpoison(addr, pages * PAGE_SIZE);
        }

        return (void*)addr;
    }
    return nullptr;
}

void AllocPageStorage() noexcept {
    size_t pagesNeeded = DIV_ROUNDUP(page_allocator.page_count * sizeof(Page), sizeof(Page) + PAGE_SIZE);
    page_allocator.page_storage = (Page*)EarlyAllocPage(pagesNeeded);
    if (!page_allocator.page_storage) {
        panic("cannot allocate %lu pages for page storage (%lu total pages)", pagesNeeded, page_allocator.page_count);
        return;
    }

    printk("[mm] page storage at %p-%p (%lu pages)\n", page_allocator.page_storage, ALIGN_UP(page_allocator.page_storage + page_allocator.page_count, PAGE_SIZE), pagesNeeded);
}

void InitArea(PageAllocArea* area) noexcept {
    area->pages = page_allocator.page_storage;

    DoInitArea(area);

    page_allocator.page_storage += area->size_in_pages;

    printk("[mm] alloc area: %p-%p (%lu pages)\n", area->base, area->base + area->size_in_pages * PAGE_SIZE, area->size_in_pages);
}

void InitPageAlloc() noexcept {
    AllocPageStorage();
    early_page_alloc_enabled = false;

    for (size_t i = 0; i < page_allocator.area_count; i++) {
        InitArea(&page_allocator.areas[i]);
    }

    printk("[kernel] initialized page alloc with %lu pages (%lu MBytes)\n", page_allocator.page_count, page_allocator.page_count * PAGE_SIZE / MB);
}

size_t FreePagesCount() noexcept {
    size_t pages_free = page_allocator.page_count;
    for (size_t i = 0; i < page_allocator.area_count; i++) {
        PageAllocArea* area = &page_allocator.areas[i];
        BUG_ON(area->pages_allocated > pages_free);
        pages_free -= area->pages_allocated.load(std::memory_order_relaxed);
    }

    return pages_free;
}

}

#ifdef CONFIG_KASAN
namespace kasan {

void Init() {
    InitAreas(mm::page_allocator.areas, mm::page_allocator.area_count);
    for (size_t i = 0; i < mm::page_allocator.area_count; i++) {
        mm::PageAllocArea* area = &mm::page_allocator.areas[i];

        // Unpoison early allocated pages in this area.
        size_t early_pages = area->pages_total - area->size_in_pages;
        if (early_pages > 0) {
            Unpoison(area->base + area->size_in_pages * PAGE_SIZE, (area->pages_total - area->size_in_pages) * PAGE_SIZE);
        }
    }
    InitFinish();
    Enable();
}

}
#endif
