#pragma once

#include <cstddef>

#include "defs.h"
#include "lib/common.h"
#include "lib/list.h"
#include "lib/spinlock.h"
#include "mm/page_alloc.h"

namespace mm {

struct Forward_List_Node {
    Forward_List_Node* next;
};

class ObjectAllocator {
private:
    SpinLock lock_;
    size_t obj_size_ = 0;
    size_t obj_size_aligned = 0;

    size_t allocated_count_ = 0;
    ListHead<Page, &Page::oa_page_list> free_pages_head_;

protected:
    constexpr static size_t MIN_ALLOC_ALIGNMENT = 8; 
public:
    ObjectAllocator() noexcept = default;
    ObjectAllocator(size_t obj_size, size_t alignment = MIN_ALLOC_ALIGNMENT) noexcept
        : obj_size_(obj_size), 
           obj_size_aligned(ALIGN_UP(obj_size, alignment)) 
    {}

    static ObjectAllocator* FromPage(Page* page) noexcept;

    Page* AllocPage(AllocFlags flags) noexcept;
    void* Alloc(AllocFlags flags = {}) noexcept;
    void Free(void* t) noexcept;

    size_t ObjectSize() const noexcept {
        return obj_size_;
    }
};

template <typename T>
class TypedObjectAllocator : public ObjectAllocator {
public:
    TypedObjectAllocator()
        : ObjectAllocator(sizeof(T), std::max(alignof(T), MIN_ALLOC_ALIGNMENT))
    {}

    T* Alloc(AllocFlags flags) noexcept {
        return ObjectAllocator::Alloc(flags);
    }

    void Free(T* t) noexcept {
        return ObjectAllocator::Free(t);
    }
};

}
