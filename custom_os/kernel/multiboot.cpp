#include "kernel/multiboot.h"
#include "kernel/printk.h"
#include "kernel/panic.h"
#include "lib/common.h"
#include "mm/paging.h"
#include "mm/page_alloc.h"
#include "mm/kasan.h"
#include "kernel/elf.h"
#include <cstdint>

namespace multiboot {

extern BootInfo* BootInfoPointer;

static inline bool IsTerminal (const Tag* tag) {
    return tag->type == 0 && tag->size == 8;
}

static inline Tag* GoNextAligned (Tag* tag) {
    return reinterpret_cast<Tag*>(
            reinterpret_cast<uint8_t*>(tag) + ((tag->size + 7) & ~7));
}

const Tag* LookupTag(uint32_t tag_code) {
    if (!BootInfoPointer) return nullptr;
    Tag* current_tag_ptr = BootInfoPointer->tags; // still 8-bytes aligned
    while (!IsTerminal(current_tag_ptr) && current_tag_ptr->type != tag_code) {
        current_tag_ptr = GoNextAligned(current_tag_ptr);
    }
    if (IsTerminal(current_tag_ptr)) return nullptr;
    return current_tag_ptr;
}

void Init(uint32_t magic, void* info) {
    if (magic != MULTIBOOT_MAGIC) {
        panic("The OS was not loaded via Multiboot2");
    }
    BootInfoPointer = reinterpret_cast<BootInfo*>(PHYS_TO_VIRT(info));
}

MemoryMapIter::MemoryMapIter() {
    mmap_tag_ = reinterpret_cast<const MemoryMapTag*> (LookupTag(MULTIBOOT_TAG_MMAP));
    if (!mmap_tag_) panic("Memory Map Entry nor found!");
    curr_ = mmap_tag_->entries;
}

const MemoryMapEntry* MemoryMapIter::Next() noexcept {
    if (!curr_) return nullptr;

    const MemoryMapEntry* result = curr_;
    if (reinterpret_cast<const uint8_t*>(curr_) 
                >=  reinterpret_cast<const uint8_t*>(mmap_tag_ ) + mmap_tag_->base.size) {
        curr_ = nullptr;
        return nullptr;
    }
    curr_ = reinterpret_cast<const MemoryMapEntry*> (
        reinterpret_cast<const uint8_t*>(curr_) + mmap_tag_->entry_size
    );
    return result;
}

}
