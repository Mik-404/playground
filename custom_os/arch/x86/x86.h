#pragma once

#include <cstdint>
#include <cstddef>

#include "lib/compiler.h"

namespace x86 {

constexpr uint64_t RFLAGS_IF = 1 << 9;
constexpr uint64_t RFLAGS_SAFE_MASK = 0b1000000111111010111;
constexpr uint64_t RFLAGS_RESERVED_BITS = 0b10;

constexpr uint64_t CR0_MP = 1 << 1;
constexpr uint64_t CR0_EM = 1 << 2;
constexpr uint64_t CR0_TS = 1 << 3;
constexpr uint64_t CR0_WP = 1 << 16;

constexpr uint64_t CR4_OSFXSR = 1 << 9;
constexpr uint64_t CR4_OSXMMEXCPT = 1 << 10;
constexpr uint64_t CR4_OSXSAVE = 1 << 18;

constexpr size_t FXSAVE_STATE_SIZE = 512;

inline uint64_t ReadCr0() noexcept {
    uint64_t ret;
    __asm__ volatile (
        "mov %%cr0, %0"
        : "=r"(ret)
    );
    return ret;
}

inline void WriteCr0(uint64_t x) noexcept {
    __asm__ volatile (
        "mov %0, %%cr0"
        : : "r"(x)
    );
}

inline uint64_t ReadCr2() noexcept {
    uint64_t ret;
    __asm__ volatile (
        "mov %%cr2, %0"
        : "=r"(ret)
    );
    return ret;
}

inline uint64_t ReadCr3() noexcept {
    uint64_t ret;
    __asm__ volatile (
        "mov %%cr3, %0"
        : "=r"(ret)
    );
    return ret;
}

inline void WriteCr3(uint64_t x) noexcept {
    __asm__ volatile (
        "mov %0, %%cr3"
        : : "r"(x)
    );
}

inline uint64_t ReadCr4() noexcept {
    uint64_t ret;
    __asm__ volatile (
        "mov %%cr4, %0"
        : "=r"(ret)
    );
    return ret;
}

inline void WriteCr4(uint64_t x) noexcept {
    __asm__ volatile (
        "mov %0, %%cr4"
        : : "r"(x)
    );
}

inline void Hlt() noexcept {
    __asm__ volatile ("hlt");
}

[[noreturn]] inline void HltForever() noexcept {
    for (;;) {
        Hlt();
    }
}

struct DtPtr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

inline void Lidt(DtPtr ptr) noexcept {
    __asm__ volatile ( "lidt %0" : : "m" (ptr) );
}

inline void Lgdt(DtPtr ptr) noexcept {
    __asm__ volatile ( "lgdtq %0" : : "m" (ptr) );
}

inline void Outb(uint16_t port, uint8_t data) noexcept {
    __asm__ volatile ("out %0,%1" : : "a" (data), "d" (port));
}

inline void Outl(uint16_t port, uint32_t data) noexcept {
    __asm__ volatile ("out %0, %1" : : "a" (data), "d" (port));
}

inline uint32_t Inl(uint16_t port) noexcept {
    uint32_t data;
    __asm__ volatile ("in %1, %0" : "=a" (data) : "d" (port));
    return data;
}

inline uint8_t Inb(uint16_t port) noexcept {
    uint8_t data;
    __asm__ volatile ("in %1, %0" : "=a" (data) : "d" (port));
    return data;
}

inline uint16_t Inw(uint16_t port) noexcept {
    uint16_t data;
    __asm__ volatile ("in %1, %0" : "=a" (data) : "d" (port));
    return data;
}

inline void Outw(uint16_t port, uint16_t data) noexcept {
    __asm__ volatile ("out %0, %1" : : "a" (data), "d" (port));
}

inline uint64_t GetRflags() noexcept {
    uint64_t ret;
    __asm__ volatile (
        "pushf\n"
        "pop %0"
        : "=r"(ret)
    );
    return ret;
}

struct CpuidResult {
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
};

inline CpuidResult Cpuid(uint32_t code) noexcept {
    CpuidResult res;
    __asm__ volatile ( "cpuid"
        : "=a" (res.eax), "=b" (res.ebx), "=c" (res.ecx), "=d" (res.edx)
        : "a" (code)
    );
    return res;
}

inline void Sti() noexcept {
    __asm__ volatile ( "sti" );
}

inline void Cli() noexcept {
    __asm__ volatile ( "cli" );
}

inline void Int3() noexcept {
    __asm__ volatile ( "int3" );
}

struct Tss {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t io_map_addr;
} __attribute__((packed));

inline uint64_t Rdtsc() noexcept {
    uint32_t lo = 0;
    uint32_t hi = 0;
    __asm__ volatile ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((uint64_t)hi << 32) | lo;
}

inline void Fxsave(void* state) noexcept {
    __asm__ volatile ("fxsave (%0)" : : "r"(state) );
}

inline void Fxrstor(void* state) noexcept {
    __asm__ volatile ("fxrstor (%0)" : : "r"(state) );
}

inline void Invlpg(uint64_t addr) noexcept {
    __asm__ volatile ("invlpg (%0)" : : "r"(addr) : "memory");
}

}
