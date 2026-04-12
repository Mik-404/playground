#pragma once

#include "lib/atomic.h"
#include <cstdint>

#include "kernel/irq.h"

namespace sync {

class SeqLock {
private:
    std::atomic<uint64_t> seq_;

private:
    uint64_t ReadStart() noexcept;
    bool ReadRetry(uint64_t seq) noexcept;
    void WriteStart() noexcept;
    void WriteEnd() noexcept;

public:
    template <typename Fn>
    void Read(Fn fn) noexcept {
        uint64_t seq = 0;
        do {
            seq = ReadStart();
            fn();
        } while (ReadRetry(seq));
    }

    template <typename Fn>
    void Write(Fn fn) noexcept {
        WriteStart();
        fn();
        WriteEnd();
    }
};

}
