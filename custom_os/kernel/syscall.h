#pragma once

#include <cstdint>
#include <type_traits>

#include "mm/paging.h"
#include "kernel/sched.h"
#include "lib/function_traits.h"

constexpr uint64_t SYS_read = 0;
constexpr uint64_t SYS_write = 1;
constexpr uint64_t SYS_open = 2;
constexpr uint64_t SYS_close = 3;
constexpr uint64_t SYS_fork = 4;
constexpr uint64_t SYS_exit = 5;
constexpr uint64_t SYS_wait4 = 6;
constexpr uint64_t SYS_mmap = 7;
constexpr uint64_t SYS_execve = 8;
constexpr uint64_t SYS_getpid = 9;
constexpr uint64_t SYS_getppid = 10;
constexpr uint64_t SYS_kill = 11;
constexpr uint64_t SYS_signal = 12;
constexpr uint64_t SYS_sigreturn = 13;
constexpr uint64_t SYS_pipe = 14;
constexpr uint64_t SYS_munmap = 15;
constexpr uint64_t SYS_mprotect = 16;
constexpr uint64_t SYS_sched_yield = 17;
constexpr uint64_t SYS_sync = 18;
constexpr uint64_t SYS_sleep = 19;
constexpr uint64_t SYS_gettimeofday = 20;

constexpr uint64_t SYS_max = 21;

template <typename T>
struct IsKernResult;

template <typename>
struct IsKernResult : std::false_type {};

template <typename T>
struct IsKernResult<kern::Result<T>> : std::true_type {};

template <typename T>
inline constexpr int64_t SyscallResultWrapper(T res) noexcept {
    if constexpr (std::is_same_v<T, int64_t>) {
        return res;
    } else if constexpr (std::is_same_v<T, kern::Errno>) {
        return -res.Code();
    } else if constexpr (IsKernResult<T>::value) {
        if (!res.Ok()) {
            return -res.Err().Code();
        }
        return (int64_t)*res;
    } else {
        static_assert(false, "invalid return type for syscall");
    }
    return -kern::ENOSYS.Code();
}

template <typename Fn>
inline int64_t SyscallWrapper(Fn fn) noexcept {
    sched::Task* task = sched::Current();
    BUG_ON(!task);

    auto& regs = task->arch_thread.Regs();

    if constexpr (FunctionTraits<Fn>::ArgCount == 1) {
        return SyscallResultWrapper(fn(task));
    } else if constexpr (FunctionTraits<Fn>::ArgCount == 2) {
        return SyscallResultWrapper(fn(
            task,
            regs.SyscallArg0<typename FunctionTraits<Fn>::template NthArg<1>>()
        ));
    } else if constexpr (FunctionTraits<Fn>::ArgCount == 3) {
        return SyscallResultWrapper(fn(
            task,
            regs.SyscallArg0<typename FunctionTraits<Fn>::template NthArg<1>>(),
            regs.SyscallArg1<typename FunctionTraits<Fn>::template NthArg<2>>()
        ));
    } else if constexpr (FunctionTraits<Fn>::ArgCount == 4) {
        return SyscallResultWrapper(fn(
            task,
            regs.SyscallArg0<typename FunctionTraits<Fn>::template NthArg<1>>(),
            regs.SyscallArg1<typename FunctionTraits<Fn>::template NthArg<2>>(),
            regs.SyscallArg2<typename FunctionTraits<Fn>::template NthArg<3>>()
        ));
    } else if constexpr (FunctionTraits<Fn>::ArgCount == 5) {
        return SyscallResultWrapper(fn(
            task,
            regs.SyscallArg0<typename FunctionTraits<Fn>::template NthArg<1>>(),
            regs.SyscallArg1<typename FunctionTraits<Fn>::template NthArg<2>>(),
            regs.SyscallArg2<typename FunctionTraits<Fn>::template NthArg<3>>(),
            regs.SyscallArg3<typename FunctionTraits<Fn>::template NthArg<4>>()
        ));
    } else if constexpr (FunctionTraits<Fn>::ArgCount == 6) {
        return SyscallResultWrapper(fn(
            task,
            regs.SyscallArg0<typename FunctionTraits<Fn>::template NthArg<1>>(),
            regs.SyscallArg1<typename FunctionTraits<Fn>::template NthArg<2>>(),
            regs.SyscallArg2<typename FunctionTraits<Fn>::template NthArg<3>>(),
            regs.SyscallArg3<typename FunctionTraits<Fn>::template NthArg<4>>(),
            regs.SyscallArg4<typename FunctionTraits<Fn>::template NthArg<5>>()
        ));
    } else if constexpr (FunctionTraits<Fn>::ArgCount == 7) {
        return SyscallResultWrapper(fn(
            task,
            regs.SyscallArg0<typename FunctionTraits<Fn>::template NthArg<1>>(),
            regs.SyscallArg1<typename FunctionTraits<Fn>::template NthArg<2>>(),
            regs.SyscallArg2<typename FunctionTraits<Fn>::template NthArg<3>>(),
            regs.SyscallArg3<typename FunctionTraits<Fn>::template NthArg<4>>(),
            regs.SyscallArg4<typename FunctionTraits<Fn>::template NthArg<5>>(),
            regs.SyscallArg5<typename FunctionTraits<Fn>::template NthArg<6>>()
        ));
    } else {
        static_assert(false, "syscall function should take up to 6 arguments");
    }
}

struct SyscallDef {
    uint64_t n;
    int64_t (*entry)();
};

#define REGISTER_SYSCALL(name, fn) \
    extern "C" int64_t _Sys_##name() noexcept { \
        return SyscallWrapper(fn); \
    } \
    [[maybe_unused]] __attribute__((section("syscalls_table"))) SyscallDef _sys_def_##name = { .n = SYS_##name, .entry = _Sys_##name };
