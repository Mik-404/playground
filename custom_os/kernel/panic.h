#pragma once

#include <cstdint>
#include <source_location>

#include "kernel/printk.h"
#include "lib/compiler.h"

namespace arch {

[[noreturn]] void ExitTesting(uint8_t code) noexcept;

}

namespace kern {

void PanicInit() noexcept;
void DoPanic(const char* msg, ...);
void PanicStart(std::source_location location = std::source_location::current());
[[noreturn]] void PanicFinish();

#define panic(msg, ...) { \
    kern::PanicStart(); \
    kern::DoPanic(msg __VA_OPT__(,) __VA_ARGS__); \
    kern::PanicFinish(); \
}

#define BUG() { panic("bug"); }
#define BUG_ON(expr) if (expr) { panic("bug: '%s'", #expr); }
#define BUG_ON_NULL(expr) BUG_ON(expr == nullptr)

#define BUG_ON_ERROR(expr) { \
    int __err = expr; \
    if (__err < 0) { \
        panic("bug: %s == %d", expr, __err); \
    } \
}
#define BUG_ON_REACH() panic("unreachable code executed")

}
