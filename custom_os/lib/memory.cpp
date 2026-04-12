#include "lib/memory.h"
#include "mm/kasan.h"

KASAN_INTERCEPT_DEFINE(int, memcmp)(const void* str1, const void* str2, size_t count) noexcept {
    const unsigned char *s1 = (const unsigned char*)str1;
    const unsigned char *s2 = (const unsigned char*)str2;

    while (count-- > 0) {
        if (*s1++ != *s2++) {
            return s1[-1] < s2[-1] ? -1 : 1;
        }
    }
    return 0;
}

KASAN_INTERCEPT_DEFINE(void, memset)(void* p, int ch, size_t sz) noexcept {
    uint8_t* ptr = (uint8_t*)p;
    while (sz > 0) {
        *ptr = ch;
        ptr++;
        sz--;
    }
}

KASAN_INTERCEPT_DEFINE(void, memcpy)(void* dst, const void* src, size_t sz) noexcept {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    while (sz > 0) {
        *d = *s;
        d++;
        s++;
        sz--;
    }
}

KASAN_INTERCEPT_DEFINE(void, memmove)(void* dst, const void* src, size_t sz) noexcept {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;

    if (dst == src) {
        return;
    }

    // TODO: check this.
    if (d > s && (uintptr_t)d - (uintptr_t)s < sz) {
        for (size_t i = sz; i > 0; i--) {
            d[i] = s[i];
        }
    } else if (d < s && (uintptr_t)s - (uintptr_t)d < sz) {
        for (size_t i = 0; i < sz; i++) {
            d[i] = s[i];
        }
    } else {
        memcpy(dst, src, sz);
    }
}

size_t strlen(const char* str) {
	size_t len = 0;
	while (str[len]) {
		len++;
    }
	return len;
}
