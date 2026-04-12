#pragma once

#include <cstddef>
#include <cstdint>

#include "lib/common.h"

#define PER_CPU_GET(varname) ({ \
    __typeof__(__per_cpu_##varname) val; \
    __asm__ volatile ("mov %%gs:__per_cpu_" #varname ", %0" : "=r"(val) ); \
    val; \
})

#define PER_CPU_SET(varname, x) { \
    __typeof__(__per_cpu_##varname) val = x; \
    __asm__ volatile ("mov %0, %%gs:__per_cpu_" #varname : : "r"(val) ); \
}

#define PER_CPU_ADD(varname, x) { \
    __typeof__(__per_cpu_##varname) val = x; \
    __asm__ volatile ("add %0, %%gs:__per_cpu_" #varname : : "r"(val) ); \
}

#define PER_CPU_STRUCT_GET(varname, member) ({ \
    __typeof__(__per_cpu_##varname.member) val; \
    __asm__ volatile ( \
        "mov %%gs:__per_cpu_" #varname " + %c1, %0" \
        : "=r"(val) \
        : "K"(OFFSETOF(__typeof__(__per_cpu_##varname), member)) \
    ); \
    val; \
})

#define PER_CPU_STRUCT_SET(varname, member, x) { \
    __asm__ volatile ( \
        "mov %1, %%gs:__per_cpu_" #varname " + %c0" \
        : \
        : "K"(OFFSETOF(__typeof__(__per_cpu_##varname), member)), "r"(x) \
    ); \
}
