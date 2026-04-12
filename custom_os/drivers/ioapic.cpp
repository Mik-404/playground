#include "ioapic.h"

#include <cstddef>

#include "mm/kasan.h"
#include "mm/paging.h"
#include "kernel/panic.h"

namespace ioapic {

typedef struct {
    uint32_t reg;
    uint32_t pad[3];
    uint32_t data;
} MMIO;

#define IOAPIC_REG_TABLE  0x10

volatile MMIO* ioapic_ptr = nullptr;

void Init(const acpi::MadtEntryIoapic* entry) {
    ioapic_ptr = (volatile MMIO*)PHYS_TO_VIRT(entry->IoapicAddr);
}

NO_KASAN void Write(int reg, uint32_t data) {
    BUG_ON_NULL(ioapic_ptr);
    ioapic_ptr->reg = reg;
    ioapic_ptr->data = data;
}

void Enable(int irq, int targetIrq) {
    Write(IOAPIC_REG_TABLE + 2 * irq, targetIrq);
    Write(IOAPIC_REG_TABLE + 2 * irq + 1, 0);
}

} // namespace ioapic
