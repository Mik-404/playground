#pragma once

// HellOS using following IDT layout on x86:
// 0..31   Processor exceptions
// 32      Timer interrupt
// 33      Spurious LAPIC interrupt
// 34      Panic broadcast IPI
// 35      Scheduler wake up CPU IPI
// 36..256 External interrupts

#define X86_EXCEPTION_DE  0
#define X86_EXCEPTION_DB  1
#define X86_EXCEPTION_BP  3
#define X86_EXCEPTION_OF  4
#define X86_EXCEPTION_BR  5
#define X86_EXCEPTION_UD  6
#define X86_EXCEPTION_NM  7
#define X86_EXCEPTION_DF  8
#define X86_EXCEPTION_TS 10
#define X86_EXCEPTION_NP 11
#define X86_EXCEPTION_SS 12
#define X86_EXCEPTION_GP 13
#define X86_EXCEPTION_PF 14
#define X86_EXCEPTION_MF 16
#define X86_EXCEPTION_AC 17
#define X86_EXCEPTION_MC 18
#define X86_EXCEPTION_XM 19
#define X86_EXCEPTION_VE 20
#define X86_EXCEPTION_CP 21

#define X86_TIMER_IRQ    32
#define X86_SPURIOUS_IRQ 33
#define X86_PANIC_BROADCAST_IRQ 34
#define X86_SCHED_BROADCAST_IRQ 35

#define X86_EXTERNAL_IRQ_START 36
#define X86_MAX_IRQ            256
#define X86_NUM_EXTERNAL_IRQS  (X86_MAX_IRQ - X86_EXTERNAL_IRQ_START)

#define X86_EXTERNAL_IRQ_ENTRY_SIZE 16

#define X86_PF_ERRCODE_P    (1 << 0)
#define X86_PF_ERRCODE_W    (1 << 1)
#define X86_PF_ERRCODE_U    (1 << 2)
#define X86_PF_ERRCODE_RSVD (1 << 3)
#define X86_PF_ERRCODE_I    (1 << 4)
#define X86_PF_ERRCODE_PK   (1 << 5)
