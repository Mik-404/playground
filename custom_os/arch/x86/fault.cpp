#include "arch/x86/exc_asm.h"
#include "kernel/irq.h"
#include "kernel/ksyms.h"
#include "kernel/panic.h"
#include "kernel/sched.h"
#include "kernel/signal.h"
#include "lib/stack_unwind.h"
#include "linker.h"
#include "mm/vmem.h"

extern "C" {

struct RipFixup {
    uint64_t from;
    uint64_t to;
};

static bool TryRipFixup(arch::Registers& regs) {
    RipFixup* fixups = (RipFixup*)&_rip_fixups_start;
    size_t fixup_count = (RipFixup*)&_rip_fixups_end - fixups;
    for (size_t i = 0; i < fixup_count; i++) {
        if (regs.rip == fixups[i].from) {
            regs.rip = fixups[i].to;
            return true;
        }
    }
    return false;
}

[[noreturn]] static void ReportBadKernelPageFault(arch::Registers& regs, uint64_t fault_addr) {
    kern::PanicStart();
    printk("bad memory access by kernel on address %p\n", fault_addr);
    unwind::PrintStack(unwind::StackIter::FromRegs(regs));
    kern::PanicFinish();
}

static void DoPageFault(arch::Registers& regs) noexcept {
    uint64_t fault_addr = x86::ReadCr2();
    sched::Task* task = sched::Current();

    if (regs.IsKernel() && KERNEL_KASAN_SHADOW_MEMORY_START <= fault_addr && fault_addr < KERNEL_KASAN_SHADOW_MEMORY_START + KERNEL_KASAN_SHADOW_MEMORY_SIZE) {
        // Page fault was caused by KASAN routines on unmapped shadow area.
        ReportBadKernelPageFault(regs, fault_addr);
    }

    // CR2 could be modified by page fault in an another interrupt.
    kern::IrqEnable();

    if (regs.errcode & X86_PF_ERRCODE_RSVD) {
        // Reserved bits were set in page table, something is totally wrong.
        ReportBadKernelPageFault(regs, fault_addr);
    }

    // Is a fault on a kernel address?
    if (fault_addr >= USERSPACE_ADDRESS_MAX) {
        if (regs.IsKernel()) {
            // Page fault was caused by kernel code. This is a bug.
            ReportBadKernelPageFault(regs, fault_addr);
        }
        // User code trying to access kernel space.
        kern::SignalSend(task, kern::Signal::SIGSEGV);
        return;
    }

    // Convert x86 page fault flags into mm::PageFaultFlags.
    mm::PageFaultFlags flags;
    if (!(regs.errcode & X86_PF_ERRCODE_P)) {
        flags |= mm::PageFaultFlag::NoPage;
    }
    if (regs.errcode & X86_PF_ERRCODE_W) {
        flags |= mm::PageFaultFlag::Write;
    }
    if (regs.errcode & X86_PF_ERRCODE_I) {
        flags |= mm::PageFaultFlag::Exec;
    }

    auto status = task->vmem->HandlePageFault(fault_addr, flags);
    if (!status.Ok() || *status == mm::FaultStatus::Ok) {
        // If everything is OK, just leave.
        // If an error occured, HandlePagefault already sent a signal to the process.
        return;
    }

    if (regs.IsKernel()) {
        // Kernel has faulted in a userspace page. Check is it inside allowed code region.
        if (TryRipFixup(regs)) {
            return;
        }
        ReportBadKernelPageFault(regs, fault_addr);
    }

    kern::SignalSend(task, kern::Signal::SIGSEGV);
}

void PfHandler(arch::Registers* regs) {
    DoPageFault(*regs);
    if (regs->IsUser()) {
        kern::SignalDeliver();
    }
}

[[noreturn]] void ReportBadException(arch::Registers* regs, std::string_view name, bool error_code = false) noexcept {
    kern::PanicStart();
    printk("unexpected CPU expection #%s", name.data());
    if (error_code) {
        printk("(%lx)", regs->errcode);
    }
    printk(" in kernel\n");
    unwind::PrintStack(unwind::StackIter::FromRegs(*regs));
    kern::PanicFinish();
}

void GenericUnexpectedExcHandler(arch::Registers* regs, std::string_view name, kern::Signal sig, bool error_code = false) noexcept {
    if (regs->IsKernel()) {
        ReportBadException(regs, name, error_code);
    }

    auto* task = sched::Current();

    kern::IrqEnable();
    kern::SignalSend(task, sig);
    kern::SignalDeliver();
}

// General protection exception.
void GpHandler(arch::Registers* regs) noexcept {
    GenericUnexpectedExcHandler(regs, "GP", kern::Signal::SIGSEGV, true);
}

// Divide error exception.
void DeHandler(arch::Registers* regs) {
    GenericUnexpectedExcHandler(regs, "DE", kern::Signal::SIGFPE);
}

// Debug exception.
void DbHandler(arch::Registers* regs) {
    GenericUnexpectedExcHandler(regs, "DB", kern::Signal::SIGTRAP);
}

// Breakpoint exception.
void BpHandler(arch::Registers* regs) {
    GenericUnexpectedExcHandler(regs, "BP", kern::Signal::SIGFPE);
}

// Overflow exception.
void OfHandler(arch::Registers* regs) {
    GenericUnexpectedExcHandler(regs, "OF", kern::Signal::SIGFPE);
}

// Bound range exceeded exception.
void BrHandler(arch::Registers* regs) {
    GenericUnexpectedExcHandler(regs, "BR", kern::Signal::SIGKILL);
}

// Invalid opcode exception.
void UdHandler(arch::Registers* regs) {
    GenericUnexpectedExcHandler(regs, "UD", kern::Signal::SIGFPE, false);
}

// Device not available exception.
void NmHandler(arch::Registers* regs) {
    if (regs->IsKernel()) {
        ReportBadException(regs, "NM");
    }


   kern::SignalSend(sched::Current(), kern::Signal::SIGILL);
    kern::IrqEnable();
    kern::SignalDeliver();
}

// Double fault exception.
void DfHandler(arch::Registers* regs) {
    ReportBadException(regs, "DF");
}

// Invalid TSS exception.
void TsHandler(arch::Registers* regs) {
    ReportBadException(regs, "TS", true);
}

// Segment not present exception.
void NpHandler(arch::Registers* regs) {
    ReportBadException(regs, "NP", true);
}

// Stack fault exception.
void SsHandler(arch::Registers* regs) {
    ReportBadException(regs, "SS", true);
}

// x87 FPU error exception.
void MfHandler(arch::Registers* regs) {
    GenericUnexpectedExcHandler(regs, "MF", kern::Signal::SIGFPE);
}

// Alignment check exception.
void AcHandler(arch::Registers* regs) {
    ReportBadException(regs, "AC");
}

// Machine check exception.
void McHandler(arch::Registers* regs) {
    ReportBadException(regs, "MC");
}

// SIMD exception.
void XmHandler(arch::Registers* regs) {
    ReportBadException(regs, "XM");
}

// Virtualization exception.
void VeHandler(arch::Registers* regs) {
    ReportBadException(regs, "VE");
}

// Control protection exception.
void CpHandler(arch::Registers* regs) {
    ReportBadException(regs, "CP");
}

}
