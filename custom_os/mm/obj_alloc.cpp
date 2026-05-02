#include "mm/obj_alloc.h"
#include "defs.h"
#include "lib/list.h"
#include "lib/locking.h"
#include "mm/page_alloc.h"
#include <cstddef>
#include <cstdint>

namespace mm {

Page* ObjectAllocator::AllocPage(AllocFlags flags) noexcept {
    size_t order = PageAllocOrderBySize(obj_size_aligned);
    Page* page = mm::AllocPage(order, flags);
    if (!page) {
        return nullptr;
    }
    page->SetFlag(Page::InObjAlloc);
    page->oa_owner = this;
    return page;
}

NO_KASAN inline void* fit_first (Page* page_ptr) noexcept {
    Forward_List_Node* first_free = (Forward_List_Node*)page_ptr->oa_freelist;
    page_ptr->oa_freelist = first_free->next;
    
    first_free->next = nullptr;
    page_ptr->oa_used_blocks++;
    if (!page_ptr->oa_freelist) {
        page_ptr->oa_page_list.Remove();
    }
    return first_free;
}

NO_KASAN void* ObjectAllocator::Alloc(AllocFlags flags) noexcept {
    bool can_split = PageAllocOrderBySize(obj_size_aligned * 2) == 0;
    if (!can_split) {
        Page* page = AllocPage(flags);
        if (!page) {
            return nullptr;
        }

        if (!flags.Has(AllocFlag::SkipKasan)) {
            kasan::RecordAlloc((uintptr_t)page->Virt(), obj_size_aligned);
        }
        return page->Virt();
    }
    IrqSafeScopeLocker locker (this->lock_);
    if (!free_pages_head_.Empty()) {
        void* result = fit_first(free_pages_head_.begin().Value());
        if (!flags.Has(AllocFlag::SkipKasan)) {
            kasan::RecordAlloc((uintptr_t)result, obj_size_aligned);
        }
        return result;
    }
    
    Page* page = AllocPage(flags);
    if (!page) {
        return nullptr;
    }
    page->oa_used_blocks = 0;

    Forward_List_Node* prev = (Forward_List_Node*) ((uintptr_t)page->Virt());
    prev->next = nullptr;

    size_t bytes_in_page = (1ull << page->Order()) * PAGE_SIZE;
    for (size_t offset = obj_size_aligned; offset + obj_size_aligned <= bytes_in_page; offset += obj_size_aligned) {
        Forward_List_Node* current = (Forward_List_Node*) ((uintptr_t)page->Virt() + offset);
        prev->next = current;
        prev = current;
    }
    prev->next = nullptr;


    if (!flags.Has(AllocFlag::SkipKasan)) {
        kasan::Poison((uintptr_t)page->Virt() + obj_size_aligned, bytes_in_page - obj_size_aligned);
    }

    void* result = (void*)page->Virt();
    page->oa_freelist = ((Forward_List_Node*)result)->next;
    ((Forward_List_Node*)result)->next = nullptr;
    page->oa_used_blocks = 1;

    if (page->oa_freelist) {
        free_pages_head_.InsertFirst(*page);
    }

    if (!flags.Has(AllocFlag::SkipKasan)) {
        kasan::RecordAlloc((uintptr_t)result, obj_size_aligned);
    }

    return result;
}

NO_KASAN void ObjectAllocator::Free(void* obj) noexcept {
    bool can_split = obj_size_aligned <= (PAGE_SIZE / 2);
    Page* page = Page::FromAddr(obj);
    BUG_ON(!page->HasFlag(Page::InObjAlloc));
    ObjectAllocator* owner = (ObjectAllocator*)(page->oa_owner);
    BUG_ON(owner != this);
    kasan::PreFree((uintptr_t)obj, obj_size_aligned);
    kasan::RecordFree((uintptr_t)obj, obj_size_aligned);

    if (!can_split) {
        FreePage(page);
        return;
    }
    Forward_List_Node* dealloc_block = (Forward_List_Node*) obj;

    IrqSafeScopeLocker locker (this->lock_);

    if (!page->oa_freelist) {
        free_pages_head_.InsertFirst(*page);
    }
    
    dealloc_block->next = (Forward_List_Node*)page->oa_freelist;
    page->oa_freelist = dealloc_block;
    
    
    page->oa_used_blocks--;
    
    if (page->oa_used_blocks == 0 && page->oa_page_list.next != page->oa_page_list.prev) {
        page->oa_page_list.Remove();
        locker.Unlock();
        FreePage(page);
        return;
    }
}

ObjectAllocator* ObjectAllocator::FromPage(Page* page) noexcept {
    return (ObjectAllocator*)page->oa_owner;
}

}