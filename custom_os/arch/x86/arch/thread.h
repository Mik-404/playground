#pragma once

#include <cstdint>
#include <stdint.h>

#include "arch/x86/arch/regs.h"
#include "arch/x86/x86.h"
#include "kernel/error.h"

namespace arch {

class Thread {
private:
    uint64_t kstack_top_ = 0;

    // Used in syscall handling.
    uint64_t saved_user_rsp_ = 0;

    // For kernel context switching.
    uint64_t saved_kernel_rsp_ = 0;
    
    bool per_cpu_used = false;

    // Area to store SSE/x87 context
    alignas(x86::FXSAVE_AREA_ALIGNMENT) uint8_t fpu_context[x86::FXSAVE_AREA_SIZE_BYTES];

public:
    static kern::Errno AllocateKstack(Thread& th) noexcept;

    static kern::Errno CloneCurrent(Thread* dst, int flags, void* ip, void* sp) noexcept;

    template <typename T>
    T SwitchTo(Thread& next, T arg) noexcept {
        return (T)DoSwitchTo(next, (uint64_t)arg);
    }

    Registers& Regs() noexcept {
        return *(Registers*)((uintptr_t)kstack_top_ - sizeof(Registers));
    }

    const Registers& Regs() const noexcept {
        return *(const Registers*)((uintptr_t)kstack_top_ - sizeof(Registers));
    }

    void* GetFPUArea () noexcept;

    void SetFPUInitialization () noexcept;

    uint64_t SavedKernelRsp() const noexcept {
        return saved_kernel_rsp_;
    }

    ~Thread() noexcept;

private:
    uint64_t DoSwitchTo(Thread& next, uint64_t arg) noexcept;
};

struct OnStackContext {
public:
    uint64_t r15 = 0;
    uint64_t r14 = 0;
    uint64_t r13 = 0;
    uint64_t r12 = 0;
    uint64_t rbp = 0;
    uint64_t rbx = 0;
    uint64_t ret_addr = 0;
};

extern "C" [[noreturn]] void ReturnFromExecve() noexcept;

}
