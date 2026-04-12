#include "mm/obj_alloc.h"

namespace mm {

void* ObjectAllocator::Alloc(AllocFlags flags) noexcept {
    size_t order = PageAllocOrderBySize(obj_size_);
    Page* page = mm::AllocPage(order, flags);
    if (!page) {
        return nullptr;
    }
    page->SetFlag(Page::InObjAlloc);
    page->oa_owner = this;

    if (!flags.Has(AllocFlag::SkipKasan)) {
        kasan::RecordAlloc((uintptr_t)page->Virt(), obj_size_);
    }

    return page->Virt();
}

void ObjectAllocator::Free(void* obj) noexcept {
    Page* page = Page::FromAddr(obj);
    BUG_ON(!page->HasFlag(Page::InObjAlloc));
    ObjectAllocator* owner = (ObjectAllocator*)(page->oa_owner);
    BUG_ON(owner != this);

    kasan::PreFree((uintptr_t)obj, obj_size_);
    FreePage(page);
    kasan::RecordFree((uintptr_t)obj, obj_size_);
}

ObjectAllocator* ObjectAllocator::FromPage(Page* page) noexcept {
    return (ObjectAllocator*)page->oa_owner;
}

}
