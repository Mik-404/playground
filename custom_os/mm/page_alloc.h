#pragma once

#include <cstddef>
#include <cstdint>

#include "kernel/printk.h"
#include "lib/atomic.h"
#include "lib/list.h"
#include "lib/shared_ptr.h"
#include "lib/flags.h"
#include "lib/spinlock.h"

namespace mm {

enum class AllocFlag {
    SkipKasan = 1 << 0,
    NoSleep = 1 << 1,
};
using AllocFlags = BitFlags<AllocFlag>;

struct PageAllocArea;

constexpr size_t PAGE_MAX_ALLOCATION_ORDER_BITS = 4;
constexpr size_t PAGE_MAX_ALLOCATION_AREAS_BITS = 4;
constexpr size_t PAGE_MAX_ALLOCATION_ORDER = 11;

// Page is a descriptor of a physical page.
struct Page {
public:
    static constexpr uint32_t Used = 1 << 0;
    static constexpr uint32_t Dirty = 1 << 2;
    static constexpr uint32_t UpToDate = 1 << 3;
    static constexpr uint32_t Locked = 1 << 4;

    static constexpr uint32_t InObjAlloc = 1 << 5;
    static constexpr uint32_t InFileCache = 1 << 6;

    static constexpr uint32_t HasBuffer = 1 << 7;

private:
    // Those fields are always in use by page allocator.
    std::atomic<uint64_t> flags;
    uint64_t virt:(64 - PAGE_MAX_ALLOCATION_ORDER_BITS - PAGE_MAX_ALLOCATION_AREAS_BITS);
    uint8_t order:PAGE_MAX_ALLOCATION_ORDER_BITS;
    uint8_t area_idx:PAGE_MAX_ALLOCATION_AREAS_BITS;

public:
    union {
        ListNode pa_free_list;
        ListNode oa_page_list;
        ListNode pc_list;
    };

    union {
        void* oa_freelist;
        void* buffer_owner;
    };

    union {
        void* oa_owner;
        size_t pc_index;
    };
    
    union {
        size_t oa_used_blocks;
        ListNode pc_dirty_list;
    };
    RefCounted ref_count;

public:
    Page(PageAllocArea* area) noexcept;

    static Page* FromAddr(void* addr) noexcept;

    PageAllocArea* Area() const noexcept;

    size_t Id() const noexcept;

    size_t Order() const noexcept {
        return order;
    }

    void SetOrder(size_t o) noexcept {
        order = o;
    }

    void SetFlag(uint32_t f) noexcept {
        flags.fetch_or(f, std::memory_order_relaxed);
    }

    void ClearFlag(uint32_t f) noexcept {
        flags.fetch_and(~f, std::memory_order_relaxed);
    }

    bool HasFlag(uint32_t f) const noexcept {
        return flags.load(std::memory_order_relaxed) & f;
    }

    bool TestAndSetFlag(uint32_t f) noexcept {
        return flags.fetch_or(f) & f;
    }

    void ResetFlags() noexcept {
        flags.store(0, std::memory_order_relaxed);
    }

    void* Virt() const noexcept {
        return (void*)((uintptr_t)virt << PAGE_SIZE_BITS);
    }

    void Ref() noexcept {
        ref_count.Ref();
    }

    bool Unref() noexcept {
        return ref_count.Unref() == 1;
    }
};

struct PageAllocArea {
    Page* pages = nullptr;
    uintptr_t base = 0;
    size_t size_in_pages = 0;
    size_t pages_total = 0;
    std::atomic<size_t> pages_allocated = 0;

    ListHead<Page, &Page::pa_free_list> free_lists[PAGE_MAX_ALLOCATION_ORDER + 1];
};

constexpr size_t MAX_ALLOCATION_AREAS = 1 << PAGE_MAX_ALLOCATION_AREAS_BITS;

struct PageAlloc {
    PageAllocArea areas[MAX_ALLOCATION_AREAS];
    size_t area_count;
    size_t page_count;

    Page* page_storage;
};

// AllocPage allocates continuous memory region contains at least 2^order pages.
Page* AllocPage(size_t order, mm::AllocFlags flags = {}) noexcept;

// FreePage frees pages at given base address.
void FreePage(Page* page) noexcept;

// FreePagesCount return number of free pages in system.
size_t FreePagesCount() noexcept;

// PageAllocOrderBySize returns minimal order that will satisfy allocation of given size.
inline size_t PageAllocOrderBySize(size_t size) noexcept {
    size_t order = 0;
    while (size > (1ull << order) * PAGE_SIZE) {
        order++;
    }
    return order;
}

// AllocPageSimple allocates page_count pages and returns address from direct mapping (not Page).
inline void* AllocPageSimple(size_t page_count, AllocFlags flags = {}) noexcept {
    Page* page = AllocPage(PageAllocOrderBySize(page_count * PAGE_SIZE), flags);
    if (!page) {
        return nullptr;
    }
    return page->Virt();
}

inline void FreePageSimple(void* addr) noexcept {
    FreePage(Page::FromAddr(addr));
}

void PreserveMemoryArea(uintptr_t addr, size_t sz) noexcept;

void InitEarlyPageAlloc() noexcept;
void* EarlyAllocPage(size_t pages, AllocFlags flags = {}) noexcept;

// InitPageAlloc initializes frame allocator. Must be called after direct physical memory mapping is created.
void InitPageAlloc() noexcept;

}
