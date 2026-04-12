#pragma once

#include <stdarg.h>
#include <cstddef>

enum class LogLevel {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warning = 3,
    Error = 4,
    Panic = 5,
};

void vprintk(LogLevel level, const char* fmt, va_list args) noexcept;

template <LogLevel Level = LogLevel::Info>
void printk(const char* fmt, ...) noexcept {
    va_list args;
    va_start(args, fmt);
    vprintk(Level, fmt, args);
    va_end(args);
}

struct PrintkPrinter {
    void (*print)(const char* buf, size_t sz);
    LogLevel min_level = LogLevel::Info;
};

void PrintkRegisterPrinter(PrintkPrinter writer) noexcept;
