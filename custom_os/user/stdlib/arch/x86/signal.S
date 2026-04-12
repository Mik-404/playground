#include <sys/syscalls.h>

.intel_syntax noprefix
.extern handlers

.section .text
    .global signal_trampoline
    .type signal_trampoline, @function
signal_trampoline:
    lea r8, [handlers + rdi * 8]
    call [r8]

    mov rax, SYS_sigreturn
    syscall
