.intel_syntax noprefix

#include "arch/x86/gdt_asm.h"
#include "arch/x86/regs_asm.h"
#include "arch/x86/asm_defs.h"

# ContextSwitch saves calee-saved register, performs stack switch and restores registers from previous context.
FUNC(ContextSwitch):
    # rdi holds pointer to current context
    # rsi holds pointer to next context
    # rdx holds a user-provided argument

    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15

    # Stack switch.
    mov qword ptr [rdi], rsp
    mov rsp, qword ptr [rsi]

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx

    # Return passed argument in rax.
    mov rax, rdx

    ret
FUNC_END(ContextSwitch)

.extern SchedFinishContextSwitch
.extern SchedReturnFromKthread

# InitUserThread is a first code called in a userspace process. It finishes context switch and jumps to the userspace.
FUNC(InitUserThread):
    # ContextSwitch passes argument from previous context in rax.
    mov r12, rax
    mov rdi, r12
    call SchedFinishContextSwitch

    POP_REGS
    iretq
FUNC_END(InitUserThread)

# InitKernelThread is a first code called right after kernel thread was launched. It finishes context switch and jumps to the entry point.
FUNC(InitKernelThread):
    mov r12, rax
    mov rdi, r12
    call SchedFinishContextSwitch

    # Arguments passed to InitKernelThread through OnStackContext:
    # * r14 contains kernel thread entry point
    # * r15 contains kernel thread argument
    # Those registers are preserved by SchedFinishContextSwitch by calling conventions.
    mov rax, r14
    mov rdi, r15

    # We cannot pop arch::Registers from stack, because they can be used to perform execve from kernel thread.
    # So just zero all unused registers.
    xor r15, r15
    xor r14, r14
    xor r13, r13
    xor r12, r12
    xor r11, r11
    xor r10, r10
    xor r9, r9
    xor r8, r8
    xor rsi, rsi
    xor rdx, rdx
    xor rcx, rcx
    xor rbx, rbx

    # Align the stack.
    and rsp, 0xfffffffffffffff0

    # Stop stack unwinding here.
    xor rbp, rbp

    # Push entry point address on stack.
    push rax
    push rax

    # Call entry point.
    call rax

    # Call cleanup function.
    pop rdi
    call SchedReturnFromKthread
FUNC_END(InitKernelThread)

# ReturnWithIter performs iretq with given arch::Registers.
# rdi contains pointer to arch::Registers.
FUNC(ReturnWithIret):
    mov rsp, rdi
    POP_REGS
    iretq
FUNC_END(ReturnWithIret)
