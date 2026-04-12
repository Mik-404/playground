#include "kernel/multiboot.h"
#include "kernel/printk.h"
#include "kernel/panic.h"
#include "lib/common.h"
#include "mm/paging.h"
#include "mm/page_alloc.h"
#include "mm/kasan.h"
#include "kernel/elf.h"

extern const char* KernelCommandLine;

namespace multiboot {

BootInfo* BootInfoPointer = nullptr;

void BootInfoRelocate() {
    size_t size_in_pages = DIV_ROUNDUP(BootInfoPointer->total_size, PAGE_SIZE);
    BootInfo* new_info = (BootInfo*)mm::EarlyAllocPage(size_in_pages, mm::AllocFlag::SkipKasan);
    if (!new_info) {
        panic("multiboot: cannot allocate %lu pages for boot info", size_in_pages);
    }
    memmove(new_info, BootInfoPointer, BootInfoPointer->total_size);
    BootInfoPointer = new_info;
}

}
