#include "lib/seqlock.h"

namespace sync {

uint64_t SeqLock::ReadStart() noexcept {
    uint64_t seq = 0;
    do {
        seq = seq_.load(std::memory_order_acquire);
    } while (seq % 2 != 0);
    return seq;
}

bool SeqLock::ReadRetry(uint64_t seq) noexcept {
    // Ensure no reads/writes are reordered after read critical section (after seqlock_read_retry).
    std::atomic_thread_fence(std::memory_order_release);
    return (seq % 2 != 0) || seq_.load(std::memory_order_relaxed) != seq;
}

void SeqLock::WriteStart() noexcept {
    seq_.fetch_add(1, std::memory_order_relaxed);
    // Ensure no reads/writes are reordered before the start of write critical section.
    std::atomic_thread_fence(std::memory_order_acquire);
}

void SeqLock::WriteEnd() noexcept {
    seq_.fetch_add(1, std::memory_order_release);
}

}
