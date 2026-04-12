#pragma once

#include <boost/intrusive/set.hpp>

#include "mm/paging.h"
#include "fs/vfs.h"
#include "kernel/error.h"
#include "lib/flags.h"

namespace mm {

enum class AreaFlag {
    // An area could be read.
    Read = 1 << 0,
    // An area could be written.
    Write = 1 << 1,
    // Instruction on area could be executed.
    Exec = 1 << 2,
    // An area creation should be at exact address.
    Fixed = 1 << 3,
    // An area is shared: changes on anonymous pages are propagated to children, changes on file pages are visible across all processes.
    Shared = 1 << 4,
};

using AreaFlags = BitFlags<AreaFlag>;

enum class PageFaultFlag {
    // Page fault occurred during memory write.
    Write = 1 << 0,
    // No page found in page table.
    NoPage = 1 << 1,
    // Page fault occured during instruction fetch (NX bit set).
    Exec = 1 << 2,
};
using PageFaultFlags = BitFlags<PageFaultFlag>;

struct Area {
    uintptr_t start = 0;
    uintptr_t end = 0;
    size_t page_count = 0;
    AreaFlags flags;
    vfs::FilePtr file;
    size_t offset = 0;

    boost::intrusive::set_member_hook<boost::intrusive::optimize_size<true>> areas_set_node;

    Area() = default;
    Area(const Area&) = default;

    bool Contains(uintptr_t addr) const noexcept;
    bool Intersects(const Area& other) const noexcept;
};
static_assert(sizeof(Area::areas_set_node) <= 24);

struct AreaKey {
    using type = uintptr_t;

    uintptr_t operator()(const Area& area) const noexcept {
        return area.end;
    }
};

enum class FaultStatus {
    Ok = 0,
    InvalidAddress = 1,
    AccessViolation = 2,
};

// Vmem manages the virtual address space, including all its page tables.
class Vmem {
private:
    Pte* p4_ = nullptr;

    // Ordered set of all vmem areas, ordered by area.end.
    boost::intrusive::set<
        Area,
        boost::intrusive::member_hook<Area, boost::intrusive::set_member_hook<boost::intrusive::optimize_size<true>>, &Area::areas_set_node>,
        boost::intrusive::key_of_value<AreaKey>
    > areas_set_;

    kern::Result<Page*> MapUserPage(uintptr_t virt_addr, Page* new_page, uint64_t raw_flags, AllocFlags af_flags = {}) noexcept;

    // FindAreaByAddr return mm::Area to which given address belongs.
    Area* FindAreaByAddr(uintptr_t addr) noexcept;

public:
    static Vmem GLOBAL;

    static kern::Result<std::unique_ptr<Vmem>> New(AllocFlags flags = {}) noexcept;
    kern::Errno Init(AllocFlags flags) noexcept;

    Vmem() = default;
    ~Vmem() noexcept;

    // SwitchTo switches current cpu to this address space.
    void SwitchTo() noexcept;

    kern::Result<void*> MapPages(uintptr_t virt_addr, size_t pgcnt, AreaFlags flags, vfs::FilePtr file, size_t offset) noexcept;

    // Clone returns a copy of this address space.
    kern::Result<std::unique_ptr<Vmem>> Clone() noexcept;

    kern::Errno Map2MbPage(uintptr_t virt_addr, uintptr_t phys_addr, uint64_t raw_flags, AllocFlags af_flags = {}) noexcept;
    kern::Errno Map1GbPage(uintptr_t virt_addr, uintptr_t phys_addr, uint64_t raw_flags, AllocFlags af_flags = {}) noexcept;
    kern::Result<Pte> Map4KbPage(uintptr_t virt_addr, uintptr_t phys_addr, uint64_t raw_flags, AllocFlags af_flags = {}) noexcept;

    kern::Result<FaultStatus> HandlePageFault(uintptr_t virt_addr, PageFaultFlags pf_flags) noexcept;

    // Dump prints page tables content in human-readable format.
    void Dump() const noexcept;
};

void TracePageFault(uintptr_t virt_addr, PageFaultFlags pf_flags) noexcept;

void InitVmem() noexcept;
void InitGlobalVmem() noexcept;

// CopyToUser copies n bytes from kernel pointer kptr to user pointer uptr.
[[nodiscard]] KASAN_INTERCEPT_DECLARE(bool, CopyToUser, void* uptr, const void* kptr, size_t n) noexcept;

// CopyFromUser copies n bytes from user pointer uptr to kernel pointer kptr.
[[nodiscard]] KASAN_INTERCEPT_DECLARE(bool, CopyFromUser, void* kptr, const void* uptr, size_t n) noexcept;

// CopyStringFromUser copies at most n bytes of null-terminated string from user pointer.
[[nodiscard]] KASAN_INTERCEPT_DECLARE(kern::Result<size_t>, CopyStringFromUser, char* kptr, const char* uptr, size_t n) noexcept;

template <typename T>
[[nodiscard]] std::enable_if_t<std::is_trivial_v<T>, bool> CopyToUser(T* uptr, const T& kptr) noexcept {
    return CopyToUser(uptr, &kptr, sizeof(T)) > 0;
}

template <typename T>
[[nodiscard]] std::enable_if_t<std::is_trivial_v<T>, bool> CopyFromUser(T& kptr, const T* uptr) noexcept {
    return CopyFromUser(&kptr, uptr, sizeof(T)) > 0;
}

}
