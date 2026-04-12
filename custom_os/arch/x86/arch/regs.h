#pragma once

#include "arch/x86/gdt.h"
#include "arch/x86/x86.h"

namespace arch {

struct Registers {
public:
    uint64_t r15 = 0;
    uint64_t r14 = 0;
    uint64_t r13 = 0;
    uint64_t r12 = 0;
    uint64_t r11 = 0;
    uint64_t r10 = 0;
    uint64_t r9 = 0;
    uint64_t r8 = 0;
    uint64_t rbp = 0;
    uint64_t rsi = 0;
    uint64_t rdx = 0;
    uint64_t rcx = 0;
    uint64_t rbx = 0;
    uint64_t rax = 0;
    uint64_t rdi = 0;
    uint64_t errcode = 0;
    uint64_t rip = 0;
    uint64_t cs = 0;
    uint64_t rflags = 0;
    uint64_t rsp = 0;
    uint64_t ss = 0;

public:
    inline uint64_t& Retval() noexcept {
        return rax;
    }

    inline uint64_t& Ip() noexcept {
        return rip;
    }

    inline uint64_t Ip() const noexcept {
        return rip;
    }

    inline uint64_t& Sp() noexcept {
        return rsp;
    }

    inline uint64_t Sp() const noexcept {
        return rsp;
    }

    inline bool IsUser() const noexcept {
        return cs == GDT_SEGMENT_SELECTOR(USER_CODE_SEG, RPL_RING3);
    }

    inline bool IsKernel() const noexcept {
        return cs == GDT_SEGMENT_SELECTOR(KERNEL_CODE_SEG, RPL_RING0);
    }

public:
    static Registers MakeCleanUser() noexcept {
        Registers regs;
        regs.ss = GDT_SEGMENT_SELECTOR(USER_DATA_SEG, RPL_RING3);
        regs.cs = GDT_SEGMENT_SELECTOR(USER_CODE_SEG, RPL_RING3);
        regs.rflags = x86::RFLAGS_IF;
        return regs;
    }

public:
    template <typename T>
    T SyscallArg0() noexcept {
        return (T)rdi;
    }

    template <typename T>
    T SyscallArg1() noexcept {
        return (T)rsi;
    }

    template <typename T>
    T SyscallArg2() noexcept {
        return (T)rdx;
    }

    template <typename T>
    T SyscallArg3() noexcept {
        return (T)r8;
    }

    template <typename T>
    T SyscallArg4() noexcept {
        return (T)r9;
    }

    template <typename T>
    T SyscallArg5() noexcept {
        return (T)r10;
    }
};

}
