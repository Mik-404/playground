#pragma once

#include <stddef.h>

#include "arch/x86/x86.h"
#include "kernel/time.h"

namespace arch {

uint64_t GetClockNs() noexcept;
time_t GetWallTimeSecs() noexcept;

}
