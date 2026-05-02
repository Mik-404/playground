#include "arch/x86/arch/signal.h"
#include "arch/regs.h"
#include "arch/x86/arch/thread.h"
#include "arch/x86/gdt.h"
#include "arch/x86/lapic.h"
#include "arch/x86/msr.h"
#include "arch/x86/arch/user.h"
#include "arch/x86/x86.h"
#include "drivers/acpi.h"
#include "gdt_asm.h"
#include "kernel/clone.h"
#include "kernel/error.h"
#include "kernel/irq.h"
#include "kernel/ksyms.h"
#include "kernel/panic.h"
#include "kernel/printk.h"
#include "kernel/sched.h"
#include "kernel/signal.h"
#include "kernel/syscall.h"
#include "lib/common.h"
#include "mm/page_alloc.h"
#include "mm/paging.h"
#include "mm/vmem.h"

#include <cstdint>

namespace arch {

extern "C" void GoToSignalHandler(Registers* regs);

kern::Errno SignalDeliver([[maybe_unused]]kern::Signal sig, void* handler) noexcept {
    Registers& regs = sched::Current()->arch_thread.Regs();
    if (!regs.IsUser()) {
        return kern::EINVAL;
    }
    uint64_t& rsp = regs.Sp();
    if ((uintptr_t)rsp < sizeof(Registers) && ALIGN_DOWN(rsp - sizeof(Registers), 16) < 8) {
        return kern::EINVAL;
    }
    uint64_t new_rsp = ALIGN_DOWN(rsp - sizeof(Registers), 16);
    if (!mm::CopyToUser((void*)new_rsp, &regs, sizeof(Registers))) {
        return kern::EINVAL;
    }

    rsp = new_rsp;
    regs.rip = reinterpret_cast<uint64_t>(handler);
    regs.rdi = (int)sig;
    GoToSignalHandler(&regs);
    return kern::ENOSYS;
}

kern::Errno SysSigreturn([[maybe_unused]]sched::Task* curr) noexcept {
    Registers& regs = sched::Current()->arch_thread.Regs();
    uint64_t& rsp = regs.Sp();

    if (!mm::CopyFromUser(&regs, (void*)rsp, sizeof(Registers))) {
        sched::TaskExit((uint32_t)sched::TASK_EXIT_KILLED | (uint32_t)kern::Signal::SIGSEGV);
    }
    if (regs.cs != GDT_SEGMENT_SELECTOR(USER_CODE_SEG, RPL_RING3) ||
            regs.ss != GDT_SEGMENT_SELECTOR(USER_DATA_SEG, RPL_RING3)) {
        sched::TaskExit((uint32_t)sched::TASK_EXIT_KILLED | (uint32_t)kern::Signal::SIGSEGV);
    }

    regs.rflags = (regs.rflags & x86::RFLAGS_ALLOWED_MASK) | x86::RFLAGS_FORCED_MASK;

    if (regs.rip >= USERSPACE_ADDRESS_MAX) {
        sched::TaskExit((uint32_t)sched::TASK_EXIT_KILLED | (uint32_t)kern::Signal::SIGSEGV);
    }

    GoToSignalHandler(&regs);
    panic("Unexpected return from user space!");

}
REGISTER_SYSCALL(sigreturn, SysSigreturn);

}
