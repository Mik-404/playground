#pragma once

#include <cstdint>
namespace hpet {

constexpr uint32_t GENERAL_CONFIGURATION_REGISTER = 0x10;
constexpr uint32_t MAIN_COUNTER_VALUE_REGISTER = 0xF0;
constexpr uint32_t ENABLE_CNF_BIT = 0b1;
constexpr uint32_t GENERAL_CAPABILITIES_REGISTER = 0x0;

void Init();
void BusySleepMs(uint64_t ms);

}
