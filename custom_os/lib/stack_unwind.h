#pragma once

#include <array>
#include <cstdint>
#include <cstddef>

#include "arch/stack_unwind.h"

namespace sched {
struct Task;
}

namespace unwind {

using StackIter = arch::StackIter;

struct UnwindedStack {
    static constexpr size_t MAX_FRAMES = 50;

    std::array<uintptr_t, MAX_FRAMES> stack;
    size_t size = 0;

    static UnwindedStack FromStackIter(StackIter iter, size_t skip = 0) noexcept;
    void Print() const noexcept;
};

void PrintStack(StackIter iter, size_t skip = 0) noexcept;

}
