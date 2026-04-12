#pragma once

#include <cstdint>
#include <cstddef>

#include "arch/x86/arch/regs.h"

namespace sched {
struct Task;
}

namespace arch {

class StackIter {
private:
    struct StackFrame {
        uint64_t rbp;
        uint64_t ret;
    };

    StackFrame* frame_ = nullptr;
    uintptr_t curr_addr_ = 0;

    StackIter(StackFrame* frame, uintptr_t addr) noexcept : frame_(frame), curr_addr_(addr) {}

public:
    static StackIter FromHere() noexcept;
    static StackIter FromRegs(Registers& regs) noexcept;
    static StackIter FromKstack(sched::Task& task) noexcept;

    bool HasNext() const noexcept {
        return frame_ != nullptr;
    }

    uintptr_t Get() const noexcept {
        return curr_addr_;
    }

    void Next() noexcept;

    template <typename Fn>
    void Visit(Fn fn) noexcept {
        while (HasNext()) {
            fn(Get());
            Next();
        }
    }
};

}
