#pragma once

#include "defs.h"
#ifndef __ASSEMBLER__
#include <cstddef>
#include <cstdint>
#endif

// Set by OS.
#define PTE_PRESENT      (1ull << 0)
#define PTE_WRITE        (1ull << 1)
#define PTE_USER         (1ull << 2)
#define PTE_WRITETHROUGH (1ull << 3)
#define PTE_NO_CACHE     (1ull << 4)
#define PTE_PAGE_SIZE    (1ull << 7)
#define PTE_GLOBAL       (1ull << 8)
#define PTE_NX           (1ull << 63)

// Set by CPU.
#define PTE_ACCESSED     (1ull << 5)
#define PTE_DIRTY        (1ull << 6)

#define PTE_COUNT      512

#define PTE_FLAGS_MASK (PTE_PRESENT | PTE_WRITE | PTE_USER | PTE_WRITETHROUGH | PTE_NO_CACHE | PTE_PAGE_SIZE | PTE_GLOBAL | PTE_NX)
#define PTE_ADDR_MASK  0x0000fffffffff000

#define P4E_ADDR_BITS       39
#define P4E_ADDR_MASK       (((uint64_t)(1) << P3E_ADDR_BITS) - 1)
#define P4E_FROM_ADDR(addr) (((uint64_t)(addr) >> 39) & 0x1ff)
#define P4E_START_ADDR(p4e) (((uint64_t)(p4e) << 39))
#define P4E_END_ADDR(p4e)   P4E_START_ADDR((p4e) + 1)

#define P3E_ADDR_BITS             30
#define P3E_ADDR_MASK             (((uint64_t)(1) << P3E_ADDR_BITS) - 1)
#define P3E_FROM_ADDR(addr)       (((uint64_t)(addr) >> P3E_ADDR_BITS) & 0x1ff)
#define P3E_START_ADDR(addr, p3e) (((uint64_t)(p3e) << P3E_ADDR_BITS) | ((addr) & ~P4E_ADDR_MASK))
#define P3E_END_ADDR(addr, p3e)   P3E_START_ADDR(addr, (p3e) + 1)

#define P2E_ADDR_BITS             21
#define P2E_ADDR_MASK             (((uint64_t)(1) << P2E_ADDR_BITS) - 1)
#define P2E_FROM_ADDR(addr)       (((uint64_t)(addr) >> P2E_ADDR_BITS) & 0x1ff)
#define P2E_START_ADDR(addr, p2e) (((uint64_t)(p2e) << P2E_ADDR_BITS) | ((addr) & ~P3E_ADDR_MASK))
#define P2E_END_ADDR(addr, p2e)   P2E_START_ADDR(addr, (p2e) + 1)

#define P1E_ADDR_BITS             12
#define P1E_ADDR_MASK             (((uint64_t)(1) << P1E_ADDR_BITS) - 1)
#define P1E_FROM_ADDR(addr)       (((uint64_t)(addr) >> P1E_ADDR_BITS) & 0x1ff)
#define P1E_START_ADDR(addr, p1e) (((uint64_t)(p1e) << P1E_ADDR_BITS) | ((addr) & ~P2E_ADDR_MASK))
#define P1E_END_ADDR(addr, p1e)   P1E_START_ADDR(addr, (p1e) + 1)

#define PTE_ADDR_FROM(p4e, p3e, p2e, p1e) (((uint64_t)(p4e) << P4E_ADDR_BITS) | ((uint64_t)(p3e) << P3E_ADDR_BITS) | ((uint64_t)(p2e) << P2E_ADDR_BITS) | ((uint64_t)(p1e) << P1E_ADDR_BITS))

#define PHYS_TO_VIRT(addr) ((void*)(KERNEL_DIRECT_PHYS_MAPPING_START + (uintptr_t)(addr)))
#define VIRT_TO_PHYS(addr) ((void*)((uintptr_t)(addr) - KERNEL_DIRECT_PHYS_MAPPING_START))

#define USERSPACE_ADDRESS_MAX 0x800000000000

#ifndef __ASSEMBLER__

namespace mm {

typedef uint64_t Pte;
typedef uint64_t PageTable[PTE_COUNT] __attribute__((aligned(PAGE_SIZE)));

inline void PteSet(mm::Pte* tbl, size_t idx, Pte pte) noexcept {
    ((volatile mm::Pte*)tbl)[idx] = pte;
}

inline void* PteAddr(Pte pte) noexcept {
    return (void*)(pte & PTE_ADDR_MASK);
}

inline uint64_t PteFlags(Pte pte) noexcept {
    return pte & PTE_FLAGS_MASK;
}

}

#endif
