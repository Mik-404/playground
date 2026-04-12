#pragma once

#include "lib/atomic.h"
#include <cstdint>

#include "lib/spinlock.h"
#include "lib/stack_unwind.h"
#include "kernel/wait.h"

namespace sched {
struct Task;
}

class Mutex {
private:
    SpinLock lock_;
    kern::WaitQueue wq_;
    sched::Task* owner_ = nullptr;

public:
    void RawLock() noexcept;
    void RawUnlock() noexcept;

    void AssertHeld();
};
