#include "kernel/per_cpu.h"
#include "linker.h"
#include "lib/common.h"
#include "mm/page_alloc.h"
#include "mm/paging.h"
#include "kernel/panic.h"

extern size_t cpu_count;
uintptr_t per_cpu_sections[MAX_CPUS];
PER_CPU_DEFINE(size_t, cpu_id);

namespace kern {

void InitPerCpu() {
    size_t per_cpu_section_sz = DIV_ROUNDUP((uint8_t*)&_phys_end_per_cpu - (uint8_t*)&_phys_start_per_cpu, CACHE_LINE_SIZE_BYTES);
    for (size_t i = 0; i < cpu_count; i++) {
        size_t pgcnt = DIV_ROUNDUP(per_cpu_section_sz, PAGE_SIZE);
        void* per_cpu_section_addr = mm::EarlyAllocPage(pgcnt, mm::AllocFlag::SkipKasan);
        if (!per_cpu_section_addr) {
            panic("cannot allocate memory for per-cpu section (CPU %d)", i);
        }
        per_cpu_sections[i] = (uintptr_t)per_cpu_section_addr;
    }
}

}
