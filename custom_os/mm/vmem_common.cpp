#include "arch/ptr.h"
#include "arch/user.h"
#include "fs/inode.h"
#include "fs/vfs.h"
#include "kernel/irq.h"
#include "kernel/panic.h"
#include "kernel/sched.h"
#include "kernel/signal.h"
#include "kernel/syscall.h"
#include "lib/common.h"
#include "linker.h"
#include "mm/new.h"
#include "mm/obj_alloc.h"
#include "mm/page_alloc.h"
#include "mm/paging.h"
#include "mm/vmem.h"
#include "uapi/mm.h"

namespace mm {

TypedObjectAllocator<Vmem> vmem_alloc;
TypedObjectAllocator<Area> vmem_area_alloc;

typedef void* (*page_alloc_fn_t)(mm::AllocFlags);

void* vmem_page_alloc_early(mm::AllocFlags flags) {
    return EarlyAllocPage(1, flags);
}

void* vmem_page_alloc_normal(mm::AllocFlags flags) {
    return AllocPageSimple(1, flags);
}

page_alloc_fn_t pgalloc_fn = vmem_page_alloc_early;

Vmem Vmem::GLOBAL;

kern::Errno Vmem::Init(mm::AllocFlags flags) noexcept {
    p4_ = (mm::Pte*)pgalloc_fn(flags);
    if (!p4_) {
        return kern::ENOMEM;
    }
    memset(p4_, '\0', PTE_COUNT * sizeof(mm::Pte));

    for (size_t p4e = P4E_FROM_ADDR(KERNEL_HIGHER_HALF_START); p4e < PTE_COUNT; p4e++) {
        PteSet(p4_, p4e, Vmem::GLOBAL.p4_[p4e]);
    }

    return kern::ENOERR;
}

kern::Result<std::unique_ptr<Vmem>> Vmem::New(mm::AllocFlags flags) noexcept {
    std::unique_ptr<Vmem> vmem(new (vmem_alloc, flags) Vmem());
    if (!vmem) {
        return {};
    }

    auto err = vmem->Init(flags);
    if (!err.Ok()) {
        return err;
    }

    return vmem;
}

void InitGlobalVmem() noexcept {
    auto res = Vmem::GLOBAL.Init(AllocFlag::NoSleep | AllocFlag::SkipKasan);
    if (!res.Ok()) {
        panic("cannot init global vmem: %e\n", res.Code());
    }

    // Setup kernel sections mapping.
    uintptr_t virt_addr_curr = KERNEL_IMAGE_START;
    uintptr_t phys_addr_curr = (uintptr_t)&_phys_start_hh;
    uintptr_t phys_addr_end = (uintptr_t)&_phys_end_hh;
    while (phys_addr_curr < phys_addr_end) {
        auto err = Vmem::GLOBAL.Map2MbPage(virt_addr_curr, phys_addr_curr, PTE_WRITE, AllocFlag::SkipKasan);
        if (!err.Ok()) {
            panic("cannot setup global vmem: %e\n", err.Code());
        }
        phys_addr_curr += 2 * MB;
        virt_addr_curr += 2 * MB;
    }

    // Setup direct physical mapping.
    uintptr_t virt_addr = KERNEL_DIRECT_PHYS_MAPPING_START;
    uintptr_t phys_addr = (uintptr_t)0;
    for (size_t i = 0; i < (KERNEL_DIRECT_PHYS_MAPPING_SIZE / GB); i++) {
        auto err = Vmem::GLOBAL.Map1GbPage(virt_addr, phys_addr, PTE_WRITE, AllocFlag::SkipKasan);
        if (!err.Ok()) {
            panic("cannot setup global vmem: %e\n", err.Code());
        }
        virt_addr += GB;
        phys_addr += GB;
    }

    Vmem::GLOBAL.SwitchTo();
}

void InitVmem() noexcept {
    pgalloc_fn = vmem_page_alloc_normal;
}

mm::Pte* EnsureNextTable(mm::Pte* tbl, size_t idx, uint64_t raw_flags, mm::AllocFlags af_flags = {}) noexcept {
    mm::Pte pte = tbl[idx];
    mm::Pte* next_tbl = nullptr;
    if (pte & PTE_PRESENT) {
        next_tbl = static_cast<mm::Pte*>(PHYS_TO_VIRT(PteAddr(pte)));
        tbl[idx] |= raw_flags;
    } else {
        next_tbl = static_cast<mm::Pte*>(pgalloc_fn(af_flags));
        if (!next_tbl) {
            return nullptr;
        }
        for (size_t i = 0; i < PTE_COUNT; i++) {
            PteSet(next_tbl, i, 0);
        }
        tbl[idx] = (uint64_t)VIRT_TO_PHYS(next_tbl) | PTE_PRESENT | raw_flags;
    }
    return next_tbl;
}

kern::Errno Vmem::Map1GbPage(uintptr_t virt_addr, uintptr_t phys_addr, uint64_t raw_flags, mm::AllocFlags af_flags) noexcept {
    BUG_ON(((uint64_t)phys_addr) % GB);
    BUG_ON(((uint64_t)virt_addr) % GB);

    uint64_t raw_flags_top_level = raw_flags & ~PTE_NX;

    mm::Pte* p3 = EnsureNextTable(p4_, P4E_FROM_ADDR(virt_addr), raw_flags_top_level, af_flags);
    if (!p3) {
        return kern::ENOMEM;
    }

    uint64_t p3e = p3[P3E_FROM_ADDR(virt_addr)];
    BUG_ON((p3e & PTE_PRESENT) && !(p3e & PTE_PAGE_SIZE));
    p3[P3E_FROM_ADDR(virt_addr)] = (uint64_t)phys_addr | PTE_PRESENT | PTE_PAGE_SIZE | raw_flags;

    return kern::ENOERR;
}

kern::Errno Vmem::Map2MbPage(uintptr_t virt_addr, uintptr_t phys_addr, uint64_t raw_flags, mm::AllocFlags af_flags) noexcept {
    BUG_ON(((uint64_t)phys_addr) % (2*MB));
    BUG_ON(((uint64_t)virt_addr) % (2*MB));

    uint64_t raw_flags_top_level = raw_flags & ~PTE_NX;

    mm::Pte* p3 = EnsureNextTable(p4_, P4E_FROM_ADDR(virt_addr), raw_flags_top_level, af_flags);
    if (!p3) {
        return kern::ENOMEM;
    }

    mm::Pte* p2 = EnsureNextTable(p3, P3E_FROM_ADDR(virt_addr), raw_flags_top_level, af_flags);
    if (!p2) {
        return kern::ENOMEM;
    }

    uint64_t pde = p2[P2E_FROM_ADDR(virt_addr)];
    BUG_ON((pde & PTE_PRESENT) && !(pde & PTE_PAGE_SIZE));
    p2[P2E_FROM_ADDR(virt_addr)] = (uint64_t)phys_addr | PTE_PRESENT | PTE_PAGE_SIZE | raw_flags;

    return kern::ENOERR;
}

kern::Result<mm::Pte> Vmem::Map4KbPage(uintptr_t virt_addr, uintptr_t phys_addr, uint64_t raw_flags, mm::AllocFlags af_flags) noexcept {
    uint64_t raw_flags_top_level = raw_flags & ~PTE_NX;

    mm::Pte* p3 = EnsureNextTable(p4_, P4E_FROM_ADDR(virt_addr), raw_flags_top_level, af_flags);
    if (!p3) {
        return kern::ENOMEM;
    }

    mm::Pte* p2 = EnsureNextTable(p3, P3E_FROM_ADDR(virt_addr), raw_flags_top_level, af_flags);
    if (!p2) {
        return kern::ENOMEM;
    }

    mm::Pte* p1 = EnsureNextTable(p2, P2E_FROM_ADDR(virt_addr), raw_flags_top_level, af_flags);
    if (!p1) {
        return kern::ENOMEM;
    }

    size_t p1_idx = P1E_FROM_ADDR(virt_addr);
    mm::Pte prev_pte = p1[p1_idx];
    PteSet(p1, p1_idx, (uint64_t)phys_addr | PTE_PRESENT | raw_flags);
    return prev_pte;
}

kern::Result<Page*> Vmem::MapUserPage(uintptr_t virt_addr, Page* new_page, uint64_t raw_flags, mm::AllocFlags af_flags) noexcept {
    raw_flags |= PTE_USER;

    new_page->Ref();
    auto prev_pte = Map4KbPage(virt_addr, (uintptr_t)VIRT_TO_PHYS(new_page->Virt()), raw_flags, af_flags);
    if (!prev_pte.Ok()) {
        new_page->Unref();
        return prev_pte.Err();
    }
    if (*prev_pte & PTE_PRESENT) {
        return Page::FromAddr(PHYS_TO_VIRT(PteAddr(*prev_pte)));
    }
    return nullptr;
}

bool Area::Contains(uintptr_t addr) const noexcept {
    return start <= addr && addr < start + page_count * PAGE_SIZE;
}

bool Area::Intersects(const Area& other) const noexcept {
    return Contains(other.start) || other.Contains(start);
}

uint64_t GetPteFlags(AreaFlags flags) noexcept {
    uint64_t pte_flags = 0;
    if (flags.Has(AreaFlag::Write)) {
        pte_flags |= PTE_WRITE;
    }
    if (!flags.Has(AreaFlag::Exec)) {
        pte_flags |= PTE_NX;
    }
    return pte_flags;
}

Area* Vmem::FindAreaByAddr(uintptr_t addr) noexcept {
    auto it = areas_set_.lower_bound(addr);
    if (it == areas_set_.end()) {
        return nullptr;
    }
    if (!it->Contains(addr)) {
        return nullptr;
    }
    return &*it;
}

void Vmem::SwitchTo() noexcept {
    x86::WriteCr3((uint64_t)VIRT_TO_PHYS(p4_));
}

void DestroyPageTables(mm::Pte* p4) noexcept;

Vmem::~Vmem() noexcept {
    for (auto it = areas_set_.begin(); it != areas_set_.end(); ) {
        Area* area = &*it;
        area->file.Reset();
        it = areas_set_.erase(it);
        delete area;
    }

    if (p4_) {
        DestroyPageTables(p4_);
    }
}

static const char* PteFlagsToString(mm::Pte pte, char* flags) noexcept {
    int pos = 0;
#define WRITE(s) { \
    const char* st = s; \
    while (*st != '\0') { \
        flags[pos++] = *st; \
        st++; \
    } \
}
    if (pte & PTE_PRESENT) {
        WRITE("P ");
    }
    if (pte & PTE_USER) {
        WRITE("USER ");
    }
    if (pte & PTE_WRITE) {
        WRITE("WRITE ");
    }
    if (pte & PTE_PAGE_SIZE) {
        WRITE("PG ");
    }
    if (pte & PTE_NO_CACHE) {
        WRITE("NOCACHE ");
    }
    if (pte & PTE_NX) {
        WRITE("NX ");
    }
#undef WRITE
    flags[pos] = '\0';
    return flags;
}

void DumpPageTables(mm::Pte* p4, bool dump_kernel_pages = false) {
    char flags[128];
    printk("==== page tables dump ====\n");
    for (size_t p4e = 0; p4e < PTE_COUNT; p4e++) {
        if (!(p4[p4e] & PTE_PRESENT)) {
            continue;
        }
        if (!dump_kernel_pages && PTE_ADDR_FROM(p4e, 0, 0, 0) >= USERSPACE_ADDRESS_MAX) {
            break;
        }
        printk("  p4@%d 0x%lx-0x%lx %s\n", p4e, PTE_ADDR_FROM(p4e, 0, 0, 0), PTE_ADDR_FROM(p4e + 1, 0, 0, 0), PteFlagsToString(p4[p4e], flags));
        mm::Pte* p3 = (mm::Pte*)PHYS_TO_VIRT(mm::PteAddr(p4[p4e]));
        for (size_t p3e = 0; p3e < PTE_COUNT; p3e++) {
            if (!(p3[p3e] & PTE_PRESENT)) {
                continue;
            }
            printk("    p3@%d 0x%lx-0x%lx %s", p3e, PTE_ADDR_FROM(p4e, p3e, 0, 0), PTE_ADDR_FROM(p4e, p3e + 1, 0, 0), PteFlagsToString(p3[p3e], flags));
            if (p3[p3e] & PTE_PAGE_SIZE) {
                printk(" -> 0x%lx\n", mm::PteAddr(p3[p3e]));
                continue;
            }
            printk("\n");

            mm::Pte* p2 = (mm::Pte*)PHYS_TO_VIRT(mm::PteAddr(p3[p3e]));
            for (size_t p2e = 0; p2e < PTE_COUNT; p2e++) {
                if (!(p2[p2e] & PTE_PRESENT)) {
                    continue;
                }
                printk("      p2@%d 0x%lx-0x%lx %s", p2e, PTE_ADDR_FROM(p4e, p3e, p2e, 0), PTE_ADDR_FROM(p4e, p3e, p2e + 1, 0), PteFlagsToString(p2[p2e], flags));
                if (p2[p2e] & PTE_PAGE_SIZE) {
                    printk(" -> 0x%lx\n", mm::PteAddr(p2[p2e]));
                    continue;
                }
                printk("\n");

                mm::Pte* p1 = (mm::Pte*)PHYS_TO_VIRT(mm::PteAddr(p2[p2e]));
                for (size_t p1e = 0; p1e < PTE_COUNT; p1e++) {
                    if (!(p1[p1e] & PTE_PRESENT)) {
                        continue;
                    }
                    printk("        p1@%d 0x%lx-0x%lx %s", p1e, PTE_ADDR_FROM(p4e, p3e, p2e, p1e), PTE_ADDR_FROM(p4e, p3e, p2e, p1e + 1), PteFlagsToString(p1[p1e], flags));
                    printk(" -> 0x%lx\n", mm::PteAddr(p1[p1e]));
                }
            }
        }
    }
    printk("==== page tables dump ====\n");
}

void Vmem::Dump() const noexcept {
    DumpPageTables(p4_);
}

void* GetTaskIP() {
    return (void*)sched::Current()->arch_thread.Regs().Ip();
}

void TracePageFault(uintptr_t virt_addr, PageFaultFlags pf_flags) noexcept {
#ifdef CONFIG_TRACE_PAGE_FAULTS
#define WRITE(s) { \
    const char* st = s; \
    while (*st != '\0') { \
        flags[pos++] = *st; \
        st++; \
    } \
}
    char flags[64];
    int pos = 0;
    if (pf_flags.Has(PageFaultFlag::NoPage)) {
        WRITE("NOPAGE ");
    }
    if (pf_flags.Has(PageFaultFlag::Write)) {
        WRITE("WRITE ");
    }
    if (pf_flags.Has(PageFaultFlag::Exec)) {
        WRITE("EXECUTE ");
    }
    flags[pos] = '\0';
#undef WRITE
    printk("[fault] rip=%p addr=%p page=%p pid=%lu %s\n", GetTaskIP(), virt_addr, virt_addr / PAGE_SIZE * PAGE_SIZE, sched::Current()->pid, flags);
#else
    UNUSED(virt_addr);
    UNUSED(pf_flags);
#endif
}

KASAN_INTERCEPT_DEFINE(bool, CopyToUser)(void* uptr, const void* kptr, size_t n) noexcept {
    return arch::CopyToUser(uptr, kptr, n) == n;
}

KASAN_INTERCEPT_DEFINE(bool, CopyFromUser)(void* kptr, const void* uptr, size_t n) noexcept {
    return arch::CopyFromUser(kptr, uptr, n) == n;
}

KASAN_INTERCEPT_DEFINE(kern::Result<size_t>, CopyStringFromUser)(char* kptr, const char* uptr, size_t n) noexcept {
    size_t total = 0;
    while (n-- != 0) {
        size_t copied = arch::CopyFromUser(kptr, uptr, 1);
        if (copied == 0) {
            return kern::EFAULT;
        }
        total++;
        if (*kptr == '\0') {
            break;
        }
        kptr++;
        uptr++;
    }
    return total;
}

kern::Result<void*> SysMmap(sched::Task* task, uintptr_t addr, size_t sz, int prot, int flags, int fd, size_t offset) noexcept {
    vfs::FilePtr f;
    if (!(flags & MAP_ANONYMOUS)) {
        auto res = task->file_table->ResolveFd(fd);
        if (!res.Ok()) {
            return res.Err();
        }
        f = std::move(*res);
        if (!f->flags_.Has(vfs::FileFlag::Mappable)) {
            return kern::EINVAL;
        }
    }

    AreaFlags mp_flags;

    if (prot & PROT_READ) {
        mp_flags |= AreaFlag::Read;
    }
    if (prot & PROT_WRITE) {
        mp_flags |= AreaFlag::Write;
    }
    if (prot & PROT_EXEC) {
        mp_flags |= AreaFlag::Exec;
    }
    if (flags & MAP_FIXED) {
        mp_flags |= AreaFlag::Fixed;
    }
    if (flags & MAP_SHARED) {
        mp_flags |= AreaFlag::Shared;
    }

    return task->vmem->MapPages(addr, DIV_ROUNDUP(sz, PAGE_SIZE), mp_flags, std::move(f), offset);
}
REGISTER_SYSCALL(mmap, SysMmap);

kern::Errno SysMunmap(sched::Task*) noexcept {
    return kern::ENOSYS;
}
REGISTER_SYSCALL(munmap, SysMunmap);

kern::Errno SysMprotect(sched::Task*) noexcept {
    return kern::ENOSYS;
}
REGISTER_SYSCALL(mprotect, SysMprotect);

}
