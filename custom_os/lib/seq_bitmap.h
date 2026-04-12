#pragma once

#include <cstddef>
#include <cstdint>

#include "lib/spinlock.h"
#include "mm/page_alloc.h"

class SeqBitmap {
private:
    int32_t max_n_;
    int32_t start_;
    uint64_t* data_;
    SpinLock lock_;

public:
    SeqBitmap(int32_t max_n, int32_t start, mm::AllocFlags flags) noexcept;

    int32_t Alloc() noexcept;
    void Free(int32_t n);

    ~SeqBitmap() noexcept;
};
