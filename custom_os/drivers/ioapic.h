#pragma once

#include <cstdint>

#include "drivers/acpi.h"

namespace ioapic {

void Init(const acpi::MadtEntryIoapic* entry);
void Write(int reg, uint32_t data);
void Enable(int irq, int targetIrq);

}
