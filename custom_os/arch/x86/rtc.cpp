#include "arch/x86/rtc.h"
#include "arch/x86/x86.h"
#include "drivers/ioapic.h"
#include "kernel/irq.h"
#include "kernel/time.h"

namespace rtc {

uint8_t ReadReg(uint8_t reg) noexcept {
    x86::Outb(ADDR_PORT, reg | DISABLE_NMI);
    return x86::Inb(DATA_PORT);
}

void WriteReg(uint8_t reg, uint8_t val) noexcept {
    x86::Outb(ADDR_PORT, reg | DISABLE_NMI);
    x86::Outb(DATA_PORT, val);
}

void RtcHandler(unsigned int) noexcept {
    // Mark end-of-interrupt.
    ReadReg(REG_C);
    time::Update();
}

void Init() noexcept {
    ioapic::Enable(8, 44);

    kern::IrqAssign(44, RtcHandler);

    kern::WithoutIrqs([&]() {
        uint8_t flags = ReadReg(REG_B);
        flags |= REG_B_UPDATE_INT | REG_B_BINARY_FORMAT | REG_B_HOUR_FORMAT_24;
        flags &= ~REG_B_DST;
        WriteReg(REG_B, flags);
    });
}

}

namespace arch {

time_t GetWallTimeSecs() noexcept {
    uint64_t year = 2000 + rtc::ReadReg(rtc::REG_YEAR);
    uint64_t month = rtc::ReadReg(rtc::REG_MONTH);
    uint64_t dayOfMonth = rtc::ReadReg(rtc::REG_DAY_OF_MONTH);
    uint64_t hours = rtc::ReadReg(rtc::REG_HOURS);
    uint64_t minutes = rtc::ReadReg(rtc::REG_MINUTES);
    uint64_t seconds = rtc::ReadReg(rtc::REG_SECONDS);
    return time::FromGregorian(year, month, dayOfMonth, hours, minutes, seconds);
}

}
