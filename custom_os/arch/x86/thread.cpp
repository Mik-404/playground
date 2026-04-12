#include "arch/x86/arch/signal.h"
#include "arch/x86/arch/thread.h"
#include "arch/x86/gdt.h"
#include "arch/x86/lapic.h"
#include "arch/x86/msr.h"
#include "arch/x86/x86.h"
#include "drivers/acpi.h"
#include "kernel/clone.h"
#include "kernel/irq.h"
#include "kernel/ksyms.h"
#include "kernel/panic.h"
#include "kernel/sched.h"
#include "kernel/syscall.h"
#include "lib/common.h"
#include "mm/page_alloc.h"
#include "mm/paging.h"

PER_CPU_DECLARE(x86::Tss, cpu_tss);

constexpr size_t KERNEL_STACK_SIZE = 2 * PAGE_SIZE;

extern "C" {

// ContextSwitch performs a context-switch. See thread.S for details.
uint64_t ContextSwitch(void* currRsp, void* nextRsp, uint64_t arg) noexcept;

void InitUserThread() noexcept;
void InitKernelThread() noexcept;

}

namespace arch {

Thread::~Thread() {
    mm::FreePageSimple((void*)(kstack_top_ - KERNEL_STACK_SIZE));
}

kern::Errno Thread::AllocateKstack(Thread& th) noexcept {
    uint8_t* kstack = (uint8_t*)mm::AllocPageSimple(KERNEL_STACK_SIZE / PAGE_SIZE);
    if (!kstack) {
        return kern::ENOMEM;
    }
    memset(kstack, '\0', KERNEL_STACK_SIZE);
    th.kstack_top_ = (uintptr_t)(kstack + KERNEL_STACK_SIZE);
    return kern::ENOERR;
}

kern::Errno Thread::CloneCurrent(Thread* dst, int flags, void* ip, void* sp) noexcept {
    if (auto err = AllocateKstack(*dst); !err.Ok()) {
        return err;
    }

    uintptr_t kstack_top = dst->kstack_top_;
    kstack_top -= sizeof(Registers);

    Registers* regs = (Registers*)kstack_top;
    memset(regs, '\0', sizeof(Registers));

    kstack_top -= sizeof(OnStackContext);
    OnStackContext* onstack_ctx = (OnStackContext*)kstack_top;
    memset(onstack_ctx, '\0', sizeof(OnStackContext));

    dst->saved_kernel_rsp_ = kstack_top;

    if (flags & CLONE_KERNEL_THREAD) {
        onstack_ctx->r14 = (uint64_t)ip;
        onstack_ctx->r15 = (uint64_t)sp;
        onstack_ctx->ret_addr = (uint64_t)InitKernelThread;
        return kern::ENOERR;
    }

    Thread& curr = sched::Current()->arch_thread;
    // Clone registers.
    *regs = curr.Regs();
    onstack_ctx->ret_addr = (uint64_t)InitUserThread;
    regs->rsp = (uint64_t)sp;
    regs->rip = (uint64_t)ip;

    regs->ss = GDT_SEGMENT_SELECTOR(USER_DATA_SEG, RPL_RING3);
    regs->cs = GDT_SEGMENT_SELECTOR(USER_CODE_SEG, RPL_RING3);
    regs->rflags = x86::RFLAGS_IF;

    return kern::ENOERR;
}

PER_CPU_DEFINE(uint64_t, saved_user_rsp);

uint64_t Thread::DoSwitchTo(Thread& next, uint64_t arg) noexcept {
    PER_CPU_STRUCT_SET(cpu_tss, rsp0, (uint64_t)next.kstack_top_);

    saved_user_rsp_ = PER_CPU_GET(saved_user_rsp);
    PER_CPU_SET(saved_user_rsp, next.saved_user_rsp_);

    return ContextSwitch(&saved_kernel_rsp_, &next.saved_kernel_rsp_, arg);
}

void DoIdle() noexcept {
    x86::Hlt();
}

void InitIdle(sched::Task& th) noexcept {
    if (auto err = Thread::AllocateKstack(th.arch_thread); !err.Ok()) {
        panic("cannot init idle task: %e", err.Code());
    }
}

}
