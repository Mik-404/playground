#pragma once

#include <stddef.h>
#include <stdint.h>
#include <errno.h>

#define SYSCALL0(n) \
    ({ \
        int64_t res; \
        __asm__ volatile ( \
            "syscall\n" \
            : "=a"(res) \
            : "a"(n) \
            : "rdi", "rsi", "rdx", "rcx", "r8", "r9", "r10", "r11", "r12", "memory" \
        ); \
        res; \
    })

#define SYSCALL1(n, arg0) \
    ({ \
        int64_t res; \
        __asm__ volatile ( \
            "syscall\n" \
            : "=a"(res) \
            : "a"(n), "D"(arg0) \
            : "rsi", "rdx", "rcx", "r8", "r9", "r10", "r11", "r12", "memory" \
        ); \
        res; \
    })

#define SYSCALL2(n, arg0, arg1) \
    ({  \
        int64_t res; \
        __asm__ volatile ( \
            "syscall\n" \
            : "=a"(res) \
            : "a"(n), "D"(arg0), "S"(arg1) \
            : "rdx", "rcx", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15", "rbx", "memory" \
        ); \
        res; \
    })

#define SYSCALL3(n, arg0, arg1, arg2) \
    ({ \
        int64_t res; \
        __asm__ volatile ( \
            "syscall\n" \
            : "=a"(res) \
            : "a"(n), "D"(arg0), "S"(arg1), "d"(arg2)  \
            : "rcx", "r8", "r9", "r10", "r11", "r12", "memory" \
        ); \
        res; \
    })

#define SYSCALL4(n, arg0, arg1, arg2, arg3) \
    ({ \
        int64_t res; \
        __asm__ volatile ( \
            "mov %[a3], %%r8\n" \
            "syscall\n" \
            : "=a"(res) \
            : "a"(n), "D"(arg0), "S"(arg1), "d"(arg2), [a3] "g"(arg3)  \
            : "rcx", "r8", "r9", "r10", "r11", "r12", "memory" \
        ); \
        res; \
    })

#define SYSCALL5(n, arg0, arg1, arg2, arg3, arg4) \
    ({ \
        int64_t res; \
        __asm__ volatile ( \
            "mov %[a3], %%r8\n" \
            "mov %[a4], %%r9\n" \
            "syscall\n" \
            : "=a"(res) \
            : "a"(n), "D"(arg0), "S"(arg1), "d"(arg2), [a3] "g"(arg3), [a4] "g"((uint64_t)arg4)  \
            : "rcx", "r8", "r9", "r10", "r11", "r12", "memory" \
        ); \
        res; \
    })

#define SYSCALL6(n, arg0, arg1, arg2, arg3, arg4, arg5) \
    ({ \
        int64_t res; \
        __asm__ volatile ( \
            "mov %[a3], %%r8\n" \
            "mov %[a4], %%r9\n" \
            "mov %[a5], %%r10\n" \
            "syscall\n" \
            : "=a"(res) \
            : "a"(n), "D"(arg0), "S"(arg1), "d"(arg2), [a3] "g"((uint64_t)arg3), [a4] "g"((uint64_t)arg4), [a5] "g"((uint64_t)arg5)  \
            : "rcx", "r11", "r8", "r9", "r10", "r12", "memory" \
        ); \
        res; \
    })

#define SET_ERRNO(res) ({ \
    if ((res) < 0) { \
        errno = -res; \
        res = -1; \
    } else {\
        errno = 0; \
    } \
    res; \
})
