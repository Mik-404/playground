#pragma once
#include "kernel/sched.h"

namespace x86 {

void IdtInit() noexcept;
void IdtLoad() noexcept;

} // namespace x86
