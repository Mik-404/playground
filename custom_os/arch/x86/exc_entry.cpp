#include "arch/x86/exc_asm.h"
#include "arch/x86/gdt.h"
#include "arch/x86/lapic.h"
#include "arch/x86/per_cpu.h"
#include "lib/stack_unwind.h"
#include "arch/x86/x86.h"
#include "kernel/irq.h"
#include "kernel/ksyms.h"
#include "kernel/panic.h"
#include "linker.h"
#include "mm/vmem.h"
#include "exc_entry.h"

namespace x86 {

struct IdtEntry {
    uint32_t dw0;
    uint32_t dw1;
    uint32_t dw2;
    uint32_t dw3;
};

static IdtEntry idt[X86_MAX_IRQ];

void IdtLoad() noexcept {
    x86::DtPtr idtptr = { .limit = sizeof(idt), .base = (uint64_t)&idt };
    x86::Lidt(idtptr);
}

static void IdtSet(uint8_t vec, uint64_t addr) {
    idt[vec].dw0 = (addr & 0xffff) | (GDT_SEGMENT_SELECTOR(KERNEL_CODE_SEG, RPL_RING0) << 16);
    idt[vec].dw1 = (addr & 0xffff0000) | (1 << 15) | (0xe << 8);
    idt[vec].dw2 = addr >> 32;
    idt[vec].dw3 = 0;
}

extern "C" {

void ExcEntryDeHandler();
void ExcEntryDbHandler();
void ExcEntryBpHandler();
void ExcEntryOfHandler();
void ExcEntryBrHandler();
void ExcEntryUdHandler();
void ExcEntryNmHandler();
void ExcEntryDfHandler();
void ExcEntryTsHandler();
void ExcEntryNpHandler();
void ExcEntrySsHandler();
void ExcEntryGpHandler();
void ExcEntryPfHandler();
void ExcEntryMfHandler();
void ExcEntryAcHandler();
void ExcEntryMcHandler();
void ExcEntryXmHandler();
void ExcEntryVeHandler();
void ExcEntryCpHandler();

void ExcEntryTimerHandler();
void ExcEntryLapicSpuriousHandler();
void ExcEntryPanicBroadcastHandler();
void ExcEntrySchedWakeCpuHandler();

typedef struct { char data[X86_EXTERNAL_IRQ_ENTRY_SIZE]; } ExternalIrqEntry;
extern ExternalIrqEntry _external_irq_entries_start[];
extern ExternalIrqEntry _external_irq_entries_end;

} // extern "C"

void IdtInit() noexcept {
    memset(idt, '\0', sizeof(idt));

    // Register known CPU exceptions.
    IdtSet(X86_EXCEPTION_DE, (uint64_t)&ExcEntryDeHandler);
    IdtSet(X86_EXCEPTION_DB, (uint64_t)&ExcEntryDbHandler);
    IdtSet(X86_EXCEPTION_BP, (uint64_t)&ExcEntryBpHandler);
    IdtSet(X86_EXCEPTION_OF, (uint64_t)&ExcEntryOfHandler);
    IdtSet(X86_EXCEPTION_BR, (uint64_t)&ExcEntryBrHandler);
    IdtSet(X86_EXCEPTION_UD, (uint64_t)&ExcEntryUdHandler);
    IdtSet(X86_EXCEPTION_NM, (uint64_t)&ExcEntryNmHandler);
    IdtSet(X86_EXCEPTION_DF, (uint64_t)&ExcEntryDfHandler);
    IdtSet(X86_EXCEPTION_TS, (uint64_t)&ExcEntryTsHandler);
    IdtSet(X86_EXCEPTION_NP, (uint64_t)&ExcEntryNpHandler);
    IdtSet(X86_EXCEPTION_SS, (uint64_t)&ExcEntrySsHandler);
    IdtSet(X86_EXCEPTION_GP, (uint64_t)&ExcEntryGpHandler);
    IdtSet(X86_EXCEPTION_PF, (uint64_t)&ExcEntryPfHandler);
    IdtSet(X86_EXCEPTION_MF, (uint64_t)&ExcEntryMfHandler);
    IdtSet(X86_EXCEPTION_AC, (uint64_t)&ExcEntryAcHandler);
    IdtSet(X86_EXCEPTION_MC, (uint64_t)&ExcEntryMcHandler);
    IdtSet(X86_EXCEPTION_XM, (uint64_t)&ExcEntryXmHandler);
    IdtSet(X86_EXCEPTION_VE, (uint64_t)&ExcEntryVeHandler);
    IdtSet(X86_EXCEPTION_CP, (uint64_t)&ExcEntryCpHandler);

    IdtSet(X86_TIMER_IRQ, (uint64_t)&ExcEntryTimerHandler);
    IdtSet(X86_SPURIOUS_IRQ, (uint64_t)&ExcEntryLapicSpuriousHandler);
    IdtSet(X86_PANIC_BROADCAST_IRQ, (uint64_t)&ExcEntryPanicBroadcastHandler);
    IdtSet(X86_SCHED_BROADCAST_IRQ, (uint64_t)&ExcEntrySchedWakeCpuHandler);

    // Register external interrupts.
    size_t num_ext_irqs = &_external_irq_entries_end - _external_irq_entries_start;
    if (num_ext_irqs > X86_NUM_EXTERNAL_IRQS) {
        panic("too many external IRQs registered (%d, %d max)", num_ext_irqs, X86_MAX_IRQ - X86_EXTERNAL_IRQ_START);
    }

    for (size_t i = 0; i < X86_NUM_EXTERNAL_IRQS; i++) {
        uint8_t vec = X86_EXTERNAL_IRQ_START + i;
        uint64_t addr = (uint64_t)&_external_irq_entries_start[i];
        if (idt[vec].dw0 != 0) {
            panic("duplicate entry for IRQ%d\n", vec);
        }
        IdtSet(vec, addr);
    }
}

extern "C" {

void NmiHandler(arch::Registers* regs) {
    panic("NMI interrupt at rip=%p", regs->rip);
}

void ExternalIrqHandler(arch::Registers* regs) noexcept {
    lapic::Eoi();
    kern::IrqGenericEntry(regs->errcode);
}

void PanicBroadcastHandler() noexcept {
    kern::IrqDisable();
    x86::HltForever();
}

void SchedWakeCpuHandler() noexcept {
    kern::IrqEnable();
    sched::Preempt();
}


void TimerHandler(arch::Registers* regs) noexcept {
    lapic::Eoi();
    if (PER_CPU_GET(cpu_id) == 0) {
        time::PeriodicTick();
    }
    kern::IrqEnable();
    sched::TimerTick(regs->IsUser());
}

} // extern "C"

} // namespace x86
