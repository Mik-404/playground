#pragma once

#include <cstddef>
#include <cstdint>

#include "lib/memory.h"

#define ALIGN_UP(X, R) (((uint64_t)(X) + (R) - 1) / (R) * (R))
#define ALIGN_DOWN(X, R) (((uint64_t)(X) / (R)) * (R))
#define DIV_ROUNDUP(X, R) (((uint64_t)(X) + (R) - 1) / (R))

#define OFFSETOF(type, member) &(((type*)0)->member)

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))


template <typename T>
inline T* AdvancePointer(T* ptr, size_t size) noexcept {
    return (T*)((uint8_t*)ptr + size);
}
