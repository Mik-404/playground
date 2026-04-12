#pragma once

#include <cstddef>
#include <cstdint>

#include "mm/kasan.h"

#ifdef CONFIG_KASAN

#define KASAN_INTERCEPT_DECLARE(type, name, ...) type name(__VA_ARGS__); \
    type _Orig##name(__VA_ARGS__)
#define KASAN_NO_INTERCEPT(name) _Orig##name
#define KASAN_INTERCEPT_DEFINE(res, name) NO_KASAN res _Orig##name

#else

#define KASAN_INTERCEPT_DECLARE(type, name, ...) type name(__VA_ARGS__)
#define KASAN_NO_INTERCEPT(name) name
#define KASAN_INTERCEPT_DEFINE(res, name) res name

#endif

extern "C" {

KASAN_INTERCEPT_DECLARE(int, memcmp, const void* str1, const void* str2, size_t count) noexcept;
KASAN_INTERCEPT_DECLARE(void, memset, void* p, int ch, size_t sz) noexcept;
KASAN_INTERCEPT_DECLARE(void, memcpy, void* dst, const void* src, size_t sz) noexcept;
KASAN_INTERCEPT_DECLARE(void, memmove, void* dst, const void* src, size_t sz) noexcept;

size_t strlen(const char* str);

}


namespace std {
    using ::memcpy;
    using ::memset;
    using ::memcmp;
    using ::memmove;
    using ::strlen;
}
