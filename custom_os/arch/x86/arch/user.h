#pragma once

#include <stddef.h>
#include <stdint.h>

#include "mm/paging.h"

namespace arch {

extern "C" size_t CopyUserGeneric(void* dst, const void* src, size_t n);

inline size_t CopyToUser(void* uptr, const void* kptr, size_t n) noexcept {
    if (((uintptr_t)uptr) >= USERSPACE_ADDRESS_MAX - n) {
        return false;
    }
    return CopyUserGeneric(uptr, kptr, n);
}

inline size_t CopyFromUser(void* kptr, const void* uptr, size_t n) noexcept {
    if (((uintptr_t)uptr) >= USERSPACE_ADDRESS_MAX - n) {
        return false;
    }
    return CopyUserGeneric(kptr, uptr, n);
}

}
