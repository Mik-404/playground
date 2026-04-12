#pragma once

#include "arch/x86/x86.h"

namespace arch {

inline void IrqEnable() noexcept {
    x86::Sti();
}

inline void IrqDisable() noexcept {
    x86::Cli();
}

inline bool IsIrqEnabled() noexcept {
    return x86::GetRflags() & x86::RFLAGS_IF;
}

}
