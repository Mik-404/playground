#include "mm/membuf.h"
#include "mm/vmem.h"

namespace mm {

kern::Errno MemBuf::CopyTo(void* buf, size_t sz) noexcept {
    size_t to_copy = std::min(size_, sz);
    if (!is_kernel_) {
        if (!CopyFromUser(buf, data_, to_copy)) {
            return kern::EFAULT;
        }
    } else {
        memcpy(buf, data_, to_copy);
    }
    return kern::ENOERR;
}

kern::Errno MemBuf::CopyFrom(const void* buf, size_t sz) noexcept {
    size_t to_copy = std::min(size_, sz);
    if (!is_kernel_) {
        if (!CopyToUser(data_, buf, to_copy)) {
            return kern::EFAULT;
        }
    } else {
        memcpy(data_, buf, to_copy);
    }
    return kern::ENOERR;
}

}
