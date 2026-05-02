.intel_syntax noprefix

#include "arch/x86/asm_defs.h"
#include "arch/x86/gdt_asm.h"
#include "arch/x86/per_cpu_asm.h"
#include "arch/x86/regs_asm.h"

.extern DoSyscall

# This is an entry point for syscall instruction.
# On enter, following holds:
#   rax contains syscall number;
#   rdi, rsi, rdx, r8, r9, r10 contain syscall arguments (in order);
#   rcx contains userspace rip;
#   r11 contains userspace rflags;
#   rsp contains *userspace* stack (it may be corrupted or not mapped);
#   interrupts are disabled (IF set in IA32_FMASK).
FUNC(SyscallEntry):
    # Load per-cpu section base in gs.
    swapgs

    # We cannot use user-controlled rsp here:
    # No stack switch will be performed if exception or interrupt occurs here since we are already in ring0.
    # So, invalid rsp leads us to the double fault.

    # Save current task pointer in temporary variable.
    mov PER_CPU(saved_user_rsp), rsp

    # rsp = task_current->kstack_top
    mov rsp, PER_CPU(task_current)
    mov rsp, [rsp]

    # We have a reliable stack now, enable interrupts.
    sti

    # Push fake interrupt frame.
    pushq GDT_SEGMENT_SELECTOR(USER_DATA_SEG, RPL_RING3)
    push qword ptr PER_CPU(saved_user_rsp)
    push r11
    pushq GDT_SEGMENT_SELECTOR(USER_CODE_SEG, RPL_RING3)
    push rcx
    pushq -1

    PUSH_REGS

    # Stop stack unwinding here.
    xor rbp, rbp

    # Prepare arguments for DoSyscall(sysno, arch_regs).
    mov rdi, rax
    mov rsi, rsp
    call DoSyscall

    # rax holds return value from syscall handler.
    POP_REGS pop_rax=0

    # Restore user stack pointer.
    mov rsp, qword ptr PER_CPU(saved_user_rsp)

    # Restore user-controller gs.
    swapgs

    sysretq
FUNC_END(SyscallEntry)

# Resets stack to initial state: registers + iretq frame. And then jumps to the user space.
FUNC(ReturnFromExecve):
    # Interrupts are NOT disabled, so direct use of rsp will lead to overriding saved registers. Use r10 instead.
    # r10 = &PER_CPU(task_current)
    mov r10, qword ptr PER_CPU(task_current)

    # r10 = thread->kstack_top_
    mov r10, [r10]

    # Registers + iretq frame.
    sub r10, 21 * 8

    # Finally, reset stack and jumpt to the user space.
    mov rsp, r10

    POP_REGS
    iretq
FUNC_END(ReturnFromExecve)


FUNC(GoToSignalHandler):
    mov rsp, rdi
    POP_REGS

    swapgs

    iretq
FUNC_END(GoToSignalHandler)