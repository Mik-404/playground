#include "arch/x86/arch/thread.h"
#include "lib/stack_unwind.h"
#include "kernel/sched.h"
#include "kernel/printk.h"
#include "kernel/ksyms.h"
#include "kernel/panic.h"
#include "lib/common.h"
#include "mm/paging.h"
#include "mm/obj_alloc.h"

namespace arch {

void StackIter::Next() noexcept {
    curr_addr_ = frame_->ret;
    frame_ = (StackFrame*)frame_->rbp;
}


__attribute__((noinline)) StackIter StackIter::FromHere() noexcept {
    uint64_t rip = 0;
    uint64_t rbp = 0;
    __asm__ volatile (
        "call .Lnext%=\n"
        ".Lnext%=: pop %0\n"
        "mov %%rbp, %1\n"
        : "=r"(rip), "=g"(rbp)
    );
    StackIter iter((StackFrame*)rbp, rip);
    iter.Next();
    return iter;
}

StackIter StackIter::FromRegs(arch::Registers& regs) noexcept {
    return StackIter((StackFrame*)regs.rbp, regs.rip);
}

StackIter StackIter::FromKstack(sched::Task& task) noexcept {
    arch::OnStackContext* ctx = (arch::OnStackContext*)task.arch_thread.SavedKernelRsp();
    return StackIter((StackFrame*)ctx->rbp, ctx->ret_addr);
}

}
