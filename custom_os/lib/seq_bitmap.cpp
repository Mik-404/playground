#include "lib/seq_bitmap.h"
#include "mm/page_alloc.h"
#include "mm/paging.h"
#include "lib/common.h"
#include "kernel/panic.h"

constexpr size_t kNumsPerChunk = 64;

SeqBitmap::SeqBitmap(int32_t max_n, int32_t start, mm::AllocFlags flags) noexcept
    : max_n_(max_n)
    , start_(start)
{
    size_t chunks_total = DIV_ROUNDUP(max_n_, kNumsPerChunk);
    size_t pgcnt = DIV_ROUNDUP(chunks_total, PAGE_SIZE);
    data_ = (uint64_t*)mm::AllocPageSimple(pgcnt, flags);
    if (!data_) {
        panic("no space for seq bitmap");
    }
}

SeqBitmap::~SeqBitmap() noexcept {
    mm::FreePageSimple(data_);
}

int32_t SeqBitmap::Alloc() noexcept {
    BUG_ON_NULL(data_);

    IrqSafeScopeLocker locker(lock_);

    uint64_t* curr_chunk = data_;
    while (*curr_chunk == (uint64_t)0xffffffffffffffffull) {
        if (curr_chunk == data_ + DIV_ROUNDUP(max_n_, kNumsPerChunk)) {
            return -1;
        }
        curr_chunk++;
    }

    for (uint64_t i = 0; i < kNumsPerChunk; i++) {
        uint64_t mask = 1;
        mask <<= i;
        if (!(*curr_chunk & mask)) {
            *curr_chunk |= 1ull << i;
            return start_ + (curr_chunk - data_) * kNumsPerChunk + i;
        }
    }

    BUG_ON_REACH();

}

void SeqBitmap::Free(int32_t n) {
    BUG_ON(n >= max_n_);
    BUG_ON_NULL(data_);

    IrqSafeScopeLocker locker(lock_);
    data_[n / kNumsPerChunk] |= 1ull << (n % kNumsPerChunk);
}
