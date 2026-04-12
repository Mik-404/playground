#include "arch/x86/x86.h"
#include "lib/common.h"
#include "mm/paging.h"
#include "linker.h"
#include "mm/kasan.h"

#define EARLY_TEXT NO_KASAN __attribute__((section(".early.text")))
#define EARLY_BSS __attribute__((section(".early.bss,\"wa\",@nobits#")))

#define KERNEL_DIRECT_PHYS_MAPPING_SIZE_GB (KERNEL_DIRECT_PHYS_MAPPING_SIZE / GB)
#define KERNEL_MAX_IMAGE_SIZE_GB 1

// Provided by early.S.
extern mm::PageTable early_p4;

namespace {

EARLY_BSS mm::PageTable direct_phys_mapping_p3[DIV_ROUNDUP(KERNEL_DIRECT_PHYS_MAPPING_SIZE_GB, 512)] = {};
EARLY_BSS mm::PageTable higher_half_p3 = {};
EARLY_BSS mm::PageTable higher_half_p2[KERNEL_MAX_IMAGE_SIZE_GB] = {};

}

extern "C" EARLY_TEXT void EarlyStart() noexcept {
    // Setup direct physical memory mapping at KERNEL_DIRECT_PHYS_MAPPING_START.
    for (size_t i = 0; i < DIV_ROUNDUP(KERNEL_DIRECT_PHYS_MAPPING_SIZE_GB, 512); i++) {
        uint64_t phys_addr = i * 512 * GB;
        uint64_t virt_addr = KERNEL_DIRECT_PHYS_MAPPING_START + phys_addr;
        for (size_t j = 0; j < 512; j++) {
            direct_phys_mapping_p3[i][j] = phys_addr | PTE_PAGE_SIZE | PTE_PRESENT | PTE_WRITE;
            phys_addr += GB;
        }
        uint64_t p4e = ((uint64_t)&direct_phys_mapping_p3[i]) | PTE_PRESENT | PTE_WRITE;
        early_p4[P4E_FROM_ADDR(virt_addr)] = p4e;
    }

    // Setup higher-half kernel sections mapping.
    uint64_t virt_addr_start = KERNEL_IMAGE_START;
    uint64_t virt_addr_curr = KERNEL_IMAGE_START;
    uint64_t phys_addr_curr = (uint64_t)&_phys_start_hh;
    uint64_t phys_addr_end = (uint64_t)&_phys_end_hh;
    while (phys_addr_curr < phys_addr_end) {
        size_t p2_idx = P3E_FROM_ADDR(virt_addr_curr) - P3E_FROM_ADDR(virt_addr_start);
        higher_half_p2[p2_idx][P2E_FROM_ADDR(virt_addr_curr)] = phys_addr_curr | PTE_PAGE_SIZE | PTE_PRESENT | PTE_WRITE;
        phys_addr_curr += 2 * MB;
        virt_addr_curr += 2 * MB;
        higher_half_p3[P3E_FROM_ADDR(virt_addr_curr)] = ((uint64_t)&higher_half_p2[p2_idx]) | PTE_PRESENT | PTE_WRITE;
    }
    early_p4[P4E_FROM_ADDR(virt_addr_curr)] = ((uint64_t)&higher_half_p3) | PTE_PRESENT | PTE_WRITE;
}
