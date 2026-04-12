#include <array>

#include "kernel/panic.h"
#include "kernel/printk.h"
#include "lib/common.h"
#include "mm/kasan.h"
#include "mm/kmalloc.h"
#include "mm/obj_alloc.h"
#include "mm/page_alloc.h"
#include "mm/paging.h"

namespace mm {

static constexpr std::array<size_t, 9> KMALLOC_SIZES{
    16,
    32,
    64,
    96,
    128,
    256,
    512,
    1024,
    2048,
};

constexpr size_t MAX_KMALLOCS = KMALLOC_SIZES.size();

__attribute__((noinit)) static ObjectAllocator kallocs[MAX_KMALLOCS];

void InitKmalloc() {
    for (size_t i = 0; i < MAX_KMALLOCS; i++) {
        new (&kallocs[i]) ObjectAllocator(KMALLOC_SIZES[i]);
    }
}

void* Kmalloc(size_t sz, AllocFlags flags) {
    size_t curr = 0;
    while (curr < MAX_KMALLOCS && KMALLOC_SIZES[curr] < sz) {
        curr++;
    }
    if (curr >= MAX_KMALLOCS) {
        Page* page = AllocPage(PageAllocOrderBySize(sz), flags);
        if (!page) {
            return nullptr;
        }
        return page->Virt();
    }
    void* ptr = kallocs[curr].Alloc(flags);
    if (!ptr) {
        return nullptr;
    }
    if (!flags.Has(AllocFlag::SkipKasan)) {
        // TODO: why two calls?
        kasan::Poison((uintptr_t)ptr, KMALLOC_SIZES[curr]);
        kasan::Unpoison((uintptr_t)ptr, sz);
    }
    return ptr;
}

void Kfree(void* addr) {
    Page* page = Page::FromAddr(addr);
    BUG_ON_NULL(page);
    if (page->HasFlag(Page::InObjAlloc)) {
        ObjectAllocator* alloc = ObjectAllocator::FromPage(page);
        alloc->Free(addr);
    } else {
        FreePage(page);
    }
}

}
