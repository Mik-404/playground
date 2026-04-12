#pragma once

#include <cstdint>

#include <type_traits>

#include "lib/atomic.h"
#include "lib/locking.h"
#include "lib/stack_unwind.h"

template <bool CacheLineAlign = false>
class AlignedSpinLock {
private:
    static constexpr uint64_t Free = 0;
    static constexpr uint64_t Locked = 1;

private:
    alignas(CacheLineAlign ? CACHE_LINE_SIZE_BYTES : alignof(std::atomic<uint64_t>)) std::atomic<uint64_t> lock_ = Free;

public:
    AlignedSpinLock() : lock_(Free) {}

    AlignedSpinLock(AlignedSpinLock&) = delete;
    AlignedSpinLock(AlignedSpinLock&&) = delete;

    void RawLock() noexcept {
        for (;;) {
            if (RawTryLock()) {
                break;
            }
        }
    }

    bool RawTryLock() noexcept {
        uint64_t expected = Free;
        if (lock_.compare_exchange_weak(expected, Locked, std::memory_order_acquire, std::memory_order_acquire)) {
            return true;
        }
        return false;
    }

    void RawUnlock() noexcept {
        lock_.store(Free, std::memory_order_release);
    }
};

using SpinLock = AlignedSpinLock<false>;
