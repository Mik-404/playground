#pragma once

#include <memory>
#include <cstdint>
#include <cstddef>

#include "kernel/error.h"

namespace mm {

class MemBuf {
public:
    bool is_kernel_ = false;
    uint8_t* data_ = nullptr;
    size_t size_ = 0;

public:
    MemBuf() = default;

    MemBuf(void* data, size_t sz, bool is_kernel = false)
        : is_kernel_(is_kernel)
        , data_((uint8_t*)data)
        , size_(sz)
    {}

    kern::Errno CopyTo(void* buf, size_t sz) noexcept;
    kern::Errno CopyFrom(const void* buf, size_t sz) noexcept;

    size_t Size() const noexcept {
        return size_;
    }

    void Skip(size_t sz) noexcept {
        sz = std::min(sz, size_);
        data_ += sz;
        size_ -= sz;
    }
};

}
