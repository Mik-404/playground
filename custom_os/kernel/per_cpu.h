#pragma once

// TODO: remove it from here
#include "arch/x86/per_cpu.h"
#include "defs.h"

extern uintptr_t per_cpu_sections[];

#define PER_CPU_DEFINE(type, varname) \
    PER_CPU_DECLARE(type, varname); \
    __attribute__((section(".per_cpu"), aligned(CACHE_LINE_SIZE_BYTES))) type __per_cpu_##varname

#define PER_CPU_DECLARE(type, varname) extern "C" type __per_cpu_##varname

PER_CPU_DECLARE(size_t, cpu_id);

#define PER_CPU_PTR(varname) ({ \
    auto ptr = &__per_cpu_##varname; \
    ptr = (decltype(ptr))(per_cpu_sections[PER_CPU_GET(cpu_id)] + (uintptr_t)&__per_cpu_##varname); \
    ptr; \
})

#define PER_CPU_PTR_FOR(cpu, varname) ({ \
    auto ptr = &__per_cpu_##varname; \
    ptr = (decltype(ptr))(per_cpu_sections[cpu] + (uintptr_t)&__per_cpu_##varname); \
    ptr; \
})

namespace kern {

void InitPerCpu();

}
