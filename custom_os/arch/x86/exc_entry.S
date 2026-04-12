#include "arch/x86/exc_asm.h"
#include "arch/x86/regs_asm.h"
#include "arch/x86/gdt_asm.h"
#include "arch/x86/asm_defs.h"

.intel_syntax noprefix

.macro SWAPGS_KERNEL cs_offset
    # If interrupt is triggered in kernel-space, appropriate GS value is already loaded.
    cmp qword ptr [rsp + \cs_offset], GDT_SEGMENT_SELECTOR(KERNEL_CODE_SEG, RPL_RING0)
    je  .Lskip_gs\@
    swapgs

    .Lskip_gs\@:
.endm

.macro EXC_ENTRY handler push_errcode
FUNC(ExcEntry\handler):
    SWAPGS_KERNEL cs_offset=8

    .if \push_errcode
    # Emulate error code on stack.
    pushq -1
    .endif

    PUSH_REGS

    # Stop stack unwinding here.
    xor rbp, rbp

    # Pass pointer to arch::Register which are located on top of stack.
    mov rdi, rsp
    call \handler

    POP_REGS

    SWAPGS_KERNEL cs_offset=8

    # Return from interrupt.
    iretq
FUNC_END(ExcEntry\handler)
.endm

// CPU exceptions.

EXC_ENTRY handler=DeHandler push_errcode=1
EXC_ENTRY handler=DbHandler push_errcode=1
EXC_ENTRY handler=BpHandler push_errcode=1
EXC_ENTRY handler=OfHandler push_errcode=1
EXC_ENTRY handler=BrHandler push_errcode=1
EXC_ENTRY handler=UdHandler push_errcode=1
EXC_ENTRY handler=NmHandler push_errcode=1
EXC_ENTRY handler=DfHandler push_errcode=0
EXC_ENTRY handler=TsHandler push_errcode=1
EXC_ENTRY handler=NpHandler push_errcode=1
EXC_ENTRY handler=SsHandler push_errcode=1
EXC_ENTRY handler=GpHandler push_errcode=0
EXC_ENTRY handler=PfHandler push_errcode=0
EXC_ENTRY handler=MfHandler push_errcode=1
EXC_ENTRY handler=AcHandler push_errcode=0
EXC_ENTRY handler=McHandler push_errcode=1
EXC_ENTRY handler=XmHandler push_errcode=1
EXC_ENTRY handler=VeHandler push_errcode=1
EXC_ENTRY handler=CpHandler push_errcode=0

// External interrupts.
EXC_ENTRY handler=LapicSpuriousHandler push_errcode=1
EXC_ENTRY handler=TimerHandler push_errcode=1

// IPIs sent by kernel.
EXC_ENTRY handler=PanicBroadcastHandler push_errcode=1
EXC_ENTRY handler=SchedWakeCpuHandler push_errcode=1

// Generic external IRQ handler.
EXC_ENTRY handler=ExternalIrqHandler push_errcode=0

.section .text
.global _external_irq_entries_start
.global _external_irq_entries_end
.align X86_EXTERNAL_IRQ_ENTRY_SIZE
_external_irq_entries_start:
    .Lvec = X86_EXTERNAL_IRQ_START
    .rept X86_NUM_EXTERNAL_IRQS
        # External interrupts has no error code.
        pushq .Lvec
        jmp ExcEntryExternalIrqHandler

        .Lvec = .Lvec + 1
        .align X86_EXTERNAL_IRQ_ENTRY_SIZE
    .endr
_external_irq_entries_end:
