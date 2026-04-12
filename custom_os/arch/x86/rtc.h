#pragma once

#include <stdint.h>

namespace rtc {

constexpr uint16_t ADDR_PORT = 0x70;
constexpr uint16_t DATA_PORT = 0x71;

constexpr uint8_t DISABLE_NMI = 1 << 7;

constexpr uint8_t REG_SECONDS = 0x0;
constexpr uint8_t REG_MINUTES = 0x2;
constexpr uint8_t REG_HOURS = 0x4;
constexpr uint8_t REG_WEEKDAY = 0x6;
constexpr uint8_t REG_DAY_OF_MONTH = 0x7;
constexpr uint8_t REG_MONTH = 0x8;
constexpr uint8_t REG_YEAR = 0x9;

// Register A: RTC status register.
constexpr uint8_t REG_A = 0xa;
constexpr uint8_t REG_A_UPDATE_IN_PROGRESS = 1 << 0;

// Register B: RTC settings register.
constexpr uint8_t REG_B = 0xb;
constexpr uint8_t REG_B_DST = 1 << 0;
constexpr uint8_t REG_B_HOUR_FORMAT_24 = 1 << 1;
constexpr uint8_t REG_B_BINARY_FORMAT = 1 << 2;
constexpr uint8_t REG_B_UPDATE_INT = 1 << 4;

// Register C: RTC interrupt status register.
constexpr uint8_t REG_C = 0xc;
constexpr uint8_t REG_C_INT_RAISED = 1 << 0;
constexpr uint8_t REG_C_UPDATE_INT_RAISED = 1 << 4;


uint8_t ReadReg(uint8_t reg) noexcept;
void WriteReg(uint8_t reg, uint8_t val) noexcept;
void Eoi() noexcept;
void Init() noexcept;

}
