#include "kernel/panic.h"
#include "mm/kmalloc.h"
#include "mm/page_alloc.h"
#include "mm/new.h"

void* operator new(size_t size, mm::AllocFlags flags) noexcept {
    return mm::Kmalloc(size, flags);
}

void* operator new[](size_t size, mm::AllocFlags flags) noexcept {
    return mm::Kmalloc(size, flags);
}

void* operator new(size_t size, mm::ObjectAllocator& alloc, mm::AllocFlags flags) noexcept {
    BUG_ON(size != alloc.ObjectSize());
    return alloc.Alloc(flags);
}

void* operator new[](size_t size, mm::ObjectAllocator& alloc, mm::AllocFlags flags) noexcept {
    BUG_ON(size != alloc.ObjectSize());
    return alloc.Alloc(flags);
}

void* operator new(size_t size) noexcept {
    return operator new(size, mm::AllocFlags{});
}

void* operator new[](size_t size) noexcept {
    return operator new[](size, mm::AllocFlags{});
}

void* operator new(size_t size, mm::ObjectAllocator& alloc) noexcept {
    return operator new(size, alloc, mm::AllocFlags{});
}

void* operator new[](size_t size, mm::ObjectAllocator& alloc) noexcept {
    return operator new(size, alloc, mm::AllocFlags{});
}

void operator delete(void* ptr) noexcept {
    mm::Kfree(ptr);
}

void operator delete(void* ptr, size_t) noexcept {
    mm::Kfree(ptr);
}

void operator delete[](void* ptr) noexcept {
    mm::Kfree(ptr);
}

void operator delete[](void* ptr, size_t) {
    mm::Kfree(ptr);
}
