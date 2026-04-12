#include <stdint.h>

#include "arch/x86/arch/regs.h"
#include "arch/x86/exc_asm.h"
#include "arch/x86/lapic.h"
#include "arch/x86/msr.h"
#include "arch/x86/x86.h"
#include "drivers/acpi.h"
#include "drivers/hpet.h"
#include "kernel/irq.h"
#include "kernel/panic.h"
#include "kernel/time.h"
#include "lib/common.h"
#include "mm/paging.h"
#include "mm/kasan.h"

#define LAPIC_ID          0x20
#define LAPIC_VER         0x30
#define LAPIC_TASKPRIOR   0x80
#define LAPIC_EOI         0x0B0
#define LAPIC_LDR         0x0D0
#define LAPIC_DFR         0x0E0
#define LAPIC_SPURIOUS    0x0F0
#define LAPIC_ESR         0x280
#define LAPIC_ICRL        0x300
#define LAPIC_ICRH        0x310
#define LAPIC_LVT_TMR     0x320
#define LAPIC_LVT_PERF    0x340
#define LAPIC_LVT_LINT0   0x350
#define LAPIC_LVT_LINT1   0x360
#define LAPIC_LVT_ERR     0x370
#define LAPIC_LAST        0x38F
#define LAPIC_DISABLE     0x10000
#define LAPIC_SW_ENABLE   0x100
#define LAPIC_CPUFOCUS    0x200
#define LAPIC_NMI         (4<<8)
#define LAPIC_INIT        0x500
#define LAPIC_BCAST       0x80000
#define LAPIC_LEVEL       0x8000
#define LAPIC_DELIVS      0x1000

#define LAPIC_ICR_INIT            (0b101 << 8)
#define LAPIC_ICR_SIPI            (0b110 << 8)
#define LAPIC_ICR_ASSERT          (1 << 14)
#define LAPIC_ICR_TRIGGER_LEVEL   (1 << 15)
#define LAPIC_ICR_PENDING         (1 << 12)
#define LAPIC_ICR_ALL             (0b10 << 18)
#define LAPIC_ICR_SELF            (0b01 << 18)
#define LAPIC_ICR_ALL_EXCEPT_SELF (0b11 << 18)

#define LAPIC_TMR_MASK         1 << 16
#define LAPIC_TMR_ONESHOT      0b00 << 17
#define LAPIC_TMR_PERIODIC     0b01 << 17
#define LAPIC_TMR_TSC_DEADLINE 0b10 << 17
#define LAPIC_TMR_INIT_CNT     0x380
#define LAPIC_TMR_CURR_CNT     0x390
#define LAPIC_TMRDIV           0x3e0
#define LAPIC_TMR_BASEDIV      (1 << 20)
#define LAPIC_TMR_X1           0b1011
#define LAPIC_TMR_X2           0b0000
#define LAPIC_TMR_X4           0b0001
#define LAPIC_TMR_X8           0b0010
#define LAPIC_TMR_X16          0b0011
#define LAPIC_TMR_X32          0b1000
#define LAPIC_TMR_X64          0b1001
#define LAPIC_TMR_X128         0b1010

namespace lapic {

constexpr uint64_t APIC_BASE_MASK = ~((uint64_t(1) << 12) - 1);

volatile uint32_t* lapic_ptr = nullptr;

NO_KASAN void Write(size_t idx, uint32_t x) noexcept {
    BUG_ON_NULL(lapic_ptr);
    lapic_ptr[idx / 4] = x;
    lapic_ptr[0];
}

NO_KASAN uint32_t Read(size_t idx) noexcept {
    BUG_ON_NULL(lapic_ptr);
    return lapic_ptr[idx / 4];
}

void Eoi() noexcept {
    Write(LAPIC_EOI, 0);
}

extern "C" void LapicSpuriousHandler(arch::Registers* regs) noexcept {
    UNUSED(regs);

    Eoi();
}

static uint64_t LapicTicksPer1Ms = 0;
static uint64_t TscTicksPer1Ms = 0;


void CalibrateTimer() noexcept {
    Write(LAPIC_TMRDIV, LAPIC_TMR_X1);
    Write(LAPIC_LVT_TMR, X86_TIMER_IRQ | LAPIC_TMR_PERIODIC);
    Write(LAPIC_TMR_INIT_CNT, 0xFFFFFFFF);

    uint64_t initial_count = 0xFFFFFFFF;
    hpet::BusySleepMs(10);
    uint64_t final_count = Read(LAPIC_TMR_CURR_CNT);

    LapicTicksPer1Ms = (initial_count - final_count) / 10;

    initial_count = x86::Rdtsc();
    hpet::BusySleepMs(10);
    final_count = x86::Rdtsc();

    TscTicksPer1Ms = (final_count - initial_count) / 10;

    printk("[lapic] %lu ticks per 1 ms\n", LapicTicksPer1Ms);
    printk("[tsc] %lu ticks per 1 ms\n", TscTicksPer1Ms);
}

void StartTimer() noexcept {
    Write(LAPIC_TMRDIV, LAPIC_TMR_X1);
    Write(LAPIC_LVT_TMR, X86_TIMER_IRQ | LAPIC_TMR_PERIODIC);
    Write(LAPIC_TMR_INIT_CNT, time::TICK_NS / time::NS_IN_MSEC * LapicTicksPer1Ms);
}

void Init() noexcept {
    // Determine LAPIC base address.
    uint64_t lapic_base_addr = x86::Rdmsr(IA32_APIC_BASE) & APIC_BASE_MASK;
    lapic_ptr = (volatile uint32_t*)PHYS_TO_VIRT(lapic_base_addr);

    // Enable APIC, by setting spurious interrupt vector and APIC Software Enabled/Disabled flag.
    Write(LAPIC_SPURIOUS, X86_SPURIOUS_IRQ | LAPIC_SW_ENABLE);

    // Disable performance monitoring counters.
    Write(LAPIC_LVT_PERF, LAPIC_DISABLE);

    // Disable local interrupt pins.
    Write(LAPIC_LVT_LINT0, LAPIC_DISABLE);
    Write(LAPIC_LVT_LINT1, LAPIC_DISABLE);

    // Signal EOI.
    Eoi();

    // Set highest priority for current task.
    Write(LAPIC_TASKPRIOR, 0);
}

bool IsInitialized() noexcept {
    return lapic_ptr != nullptr;
}

void StartCpu(uint32_t lapic_id) noexcept {
    // Assert INIT.
    Write(LAPIC_ICRH, lapic_id << 24);
    Write(LAPIC_ICRL, LAPIC_ICR_INIT | LAPIC_ICR_ASSERT | LAPIC_ICR_TRIGGER_LEVEL);
    while (Read(LAPIC_ICRL) & LAPIC_ICR_PENDING) {}

    // De-assert INIT.
    Write(LAPIC_ICRH, lapic_id << 24);
    Write(LAPIC_ICRL, LAPIC_ICR_INIT | LAPIC_ICR_TRIGGER_LEVEL);
    while (Read(LAPIC_ICRL) & LAPIC_ICR_PENDING) {}

    // Send SIPI twice.
    for (int i = 0; i < 2; i++) {
        Write(LAPIC_ICRH, lapic_id << 24);
        Write(LAPIC_ICRL, LAPIC_ICR_SIPI | 0x8);
        while (Read(LAPIC_ICRL) & LAPIC_ICR_PENDING) {}
    }
}

uint32_t CpuId() noexcept {
    return Read(LAPIC_ID) >> 24;
}

void BroadcastIpi(int ipi) noexcept {
    Write(LAPIC_ICRH, 0);
    Write(LAPIC_ICRL, LAPIC_ICR_ASSERT | LAPIC_ICR_ALL_EXCEPT_SELF | (ipi & 0xff));
}

void SendIpi(uint32_t lapic_id, int ipi) noexcept {
    Write(LAPIC_ICRH, (uint64_t)lapic_id << 56);
    Write(LAPIC_ICRL, LAPIC_ICR_ASSERT | (ipi & 0xff));
}

}

namespace arch {

uint64_t GetClockNs() noexcept {
    return x86::Rdtsc() / lapic::TscTicksPer1Ms * 1'000'000;
}

}
