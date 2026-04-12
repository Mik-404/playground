#include <cstdint>

#include "drivers/acpi.h"
#include "drivers/hpet.h"
#include "kernel/panic.h"
#include "kernel/time.h"
#include "kernel/printk.h"
#include "mm/kasan.h"
#include "mm/paging.h"


namespace hpet {

volatile uint8_t* hpet_base = nullptr;
uint32_t period_fs = 0;

NO_KASAN inline uint64_t ReadReg(uint32_t offset) {
    return *(volatile uint64_t*)(hpet_base + offset);
}

NO_KASAN inline void WriteReg(uint32_t offset, uint64_t value) {
    *(volatile uint64_t*)(hpet_base + offset) = value;
}

NO_KASAN void Init() {
    auto hpet_table = acpi::LookupRsdt<acpi::HpetHeader>("HPET");
    if (!hpet_table) {
        panic("HPET not found in ACPI!");
    }

    uint64_t phys_addr = hpet_table->base_address.Base;
    hpet_base = (volatile uint8_t*)PHYS_TO_VIRT(phys_addr);

    uint64_t config = ReadReg(GENERAL_CONFIGURATION_REGISTER);
    config |= ENABLE_CNF_BIT; 
    WriteReg(GENERAL_CONFIGURATION_REGISTER, config);
    
// the period at which the counter increments in femptoseconds (10^-15 seconds)
    period_fs = (uint32_t)(ReadReg(GENERAL_CAPABILITIES_REGISTER) >> 32);
}

inline uint64_t ReadCounter() {
    return ReadReg(MAIN_COUNTER_VALUE_REGISTER);
}

static inline bool CheckTmr(uint64_t cnt_val, uint64_t lhs) {
    return (cnt_val) * period_fs < lhs * time::MS_2_FS;
}

void BusySleepMs(uint64_t ms) {
    uint64_t initial_counter_value = ReadCounter();
    
    while (CheckTmr(ReadCounter() - initial_counter_value, ms)) {
        __asm__ volatile("pause"); // llm сказала что это немного оптимизирует
    }
}

} // namespace hpet
