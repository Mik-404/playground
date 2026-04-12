#pragma once

#include "arch/x86/x86.h"

namespace arch {

// Use non-canonical address for error pointers.
constexpr uintptr_t InvalidPointer = 0xc000000000000000;

inline constexpr uintptr_t ErrorPointer(int code) noexcept {
    return 0x8000000000000000 | (code & 0xff);
}

inline constexpr bool IsErrorPointer(uintptr_t ptr) noexcept {
    return (ptr & 0xffff000000000000) == 0x8000000000000000;
}

inline constexpr int ErrorFromPointer(uintptr_t ptr) noexcept {
    return (int)(ptr & 0xff);
}

inline void TlbInvalidate(uintptr_t addr) noexcept {
    x86::Invlpg(addr);
}

}
