#include "mm/vmem.h"

namespace mm {

uint64_t GetPteFlags(AreaFlags flags) noexcept;
extern TypedObjectAllocator<Area> vmem_area_alloc;
mm::Pte* EnsureNextTable(mm::Pte* tbl, size_t idx, uint64_t raw_flags, mm::AllocFlags af_flags = {}) noexcept;

namespace {

kern::Errno ClonePageTables(mm::Pte* dst_p4, mm::Pte* src_p4) noexcept {
    for (size_t p4e = 0; p4e < P4E_FROM_ADDR(USERSPACE_ADDRESS_MAX); p4e++) {
        if (!(src_p4[p4e] & PTE_PRESENT)) {
            continue;
        }
        BUG_ON(src_p4[p4e] & PTE_PAGE_SIZE);

        mm::Pte* src_p3 = static_cast<mm::Pte*>(PHYS_TO_VIRT(PteAddr(src_p4[p4e])));
        mm::Pte* dst_p3 = EnsureNextTable(dst_p4, p4e, src_p4[p4e] & PTE_FLAGS_MASK);
        if (!dst_p3) {
            return kern::ENOMEM;
        }

        for (size_t p3e = 0; p3e < PTE_COUNT; p3e++) {
            if (!(src_p3[p3e] & PTE_PRESENT)) {
                continue;
            }
            // HellOS doesn't map huge pages into userspace.
            BUG_ON(src_p3[p3e] & PTE_PAGE_SIZE);

            mm::Pte* src_p2 = static_cast<mm::Pte*>(PHYS_TO_VIRT(PteAddr(src_p3[p3e])));
            mm::Pte* dst_p2 = EnsureNextTable(dst_p3, p3e, src_p3[p3e] & PTE_FLAGS_MASK);
            if (!dst_p2) {
                return kern::ENOMEM;
            }

            for (size_t p2e = 0; p2e < PTE_COUNT; p2e++) {
                if (!(src_p2[p2e] & PTE_PRESENT)) {
                    continue;
                }
                BUG_ON(src_p2[p2e] & PTE_PAGE_SIZE);

                mm::Pte* src_p1 = static_cast<mm::Pte*>(PHYS_TO_VIRT(PteAddr(src_p2[p2e])));
                mm::Pte* dst_p1 = EnsureNextTable(dst_p2, p2e, src_p2[p2e] & PTE_FLAGS_MASK);
                if (!dst_p1) {
                    return kern::ENOMEM;
                }

                for (size_t p1e = 0; p1e < PTE_COUNT; p1e++) {
                    if (!(src_p1[p1e] & PTE_PRESENT)) {
                        continue;
                    }

                    void* old_page_virt = PHYS_TO_VIRT(PteAddr(src_p1[p1e]));
                    BUG_ON_NULL(old_page_virt);

                    Page* new_page = AllocPage(0);
                    if (!new_page) {
                        return kern::ENOMEM;
                    }
                    memcpy(new_page->Virt(), old_page_virt, PAGE_SIZE);

                    mm::Pte new_pte = (uintptr_t)VIRT_TO_PHYS(new_page->Virt());
                    new_pte |= src_p1[p1e] & PTE_FLAGS_MASK;
                    PteSet(dst_p1, p1e, new_pte);
                }
            }
        }
    }

    return kern::ENOERR;
}

}

void DestroyPageTables(mm::Pte* p4) noexcept {
    for (size_t p4e = 0; p4e < P4E_FROM_ADDR(KERNEL_HIGHER_HALF_START); p4e++) {
        if (!(p4[p4e] & PTE_PRESENT)) {
            continue;
        }
        mm::Pte* p3 = static_cast<mm::Pte*>(PHYS_TO_VIRT(PteAddr(p4[p4e])));
        for (size_t p3e = 0; p3e < PTE_COUNT; p3e++) {
            if (!(p3[p3e] & PTE_PRESENT)) {
                continue;
            }
            if (p3[p3e] & PTE_PAGE_SIZE) {
                continue;
            }

            mm::Pte* p2 = static_cast<mm::Pte*>(PHYS_TO_VIRT(PteAddr(p3[p3e])));
            for (size_t p2e = 0; p2e < PTE_COUNT; p2e++) {
                if (!(p2[p2e] & PTE_PRESENT)) {
                    continue;
                }
                if (p2[p2e] & PTE_PAGE_SIZE) {
                    continue;
                }

                mm::Pte* p1 = static_cast<mm::Pte*>(PHYS_TO_VIRT(PteAddr(p2[p2e])));
                for (size_t p1e = 0; p1e < PTE_COUNT; p1e++) {
                    if (!(p1[p1e] & PTE_PRESENT)) {
                        continue;
                    }

                    Page* page = Page::FromAddr(PHYS_TO_VIRT(PteAddr(p1[p1e])));
                    BUG_ON_NULL(page);
                    if (page->Unref()) {
                        FreePage(page);
                    }
                }
                FreePageSimple(p1);
            }
            FreePageSimple(p2);
        }
        FreePageSimple(p3);
    }
    FreePageSimple(p4);
}

kern::Result<std::unique_ptr<Vmem>> Vmem::Clone() noexcept {
    auto dst = Vmem::New();
    if (!dst.Ok()) {
        return dst.Err();
    }

    auto err = ClonePageTables(dst->p4_, p4_);
    if (!err.Ok()) {
        return err;
    }

    return dst;
}

kern::Result<void*> Vmem::MapPages(uintptr_t virt_addr, size_t page_count, AreaFlags flags, vfs::FilePtr file, size_t offset) noexcept {
    if (virt_addr >= USERSPACE_ADDRESS_MAX) {
        return kern::EINVAL;
    }

    if (flags.Has(AreaFlag::Shared)) {
        return kern::EINVAL;
    }

    if (!flags.Has(AreaFlag::Fixed)) {
        return kern::EINVAL;
    }

    if (offset % PAGE_SIZE != 0) {
        return kern::EINVAL;
    }

    size_t file_start_page = offset / PAGE_SIZE;
    for (size_t i = 0; i < page_count; i++) {
        Page* page = mm::AllocPage(0);
        if (!page) {
            return kern::ENOMEM;
        }
        uint64_t pte_flags = GetPteFlags(flags) | PTE_USER;
        auto pte = Map4KbPage(virt_addr + i * PAGE_SIZE, (uintptr_t)VIRT_TO_PHYS(page->Virt()), pte_flags);
        if (!pte.Ok()) {
            return pte.Err();
        }

        if (file) {
            auto file_page = file->LoadPage(file_start_page + i);
            if (!file_page.Ok()) {
                return file_page.Err();
            }
            memcpy(page->Virt(), file_page->Virt(), PAGE_SIZE);
        }
    }

    return (void*)virt_addr;
}

kern::Result<FaultStatus> Vmem::HandlePageFault(uintptr_t virt_addr, PageFaultFlags pf_flags) noexcept {
    TracePageFault(virt_addr, pf_flags);
    return FaultStatus::AccessViolation;
}

}
