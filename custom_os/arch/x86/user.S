.intel_syntax noprefix

#include "arch/x86/asm_defs.h"

# CopyUserGeneric performs copy to/from user space addressed.
# Arguments are:
# * rdi – dst
# * rsi – src
# * rdx – count
FUNC(CopyUserGeneric):
    mov rcx, rdx

1:
    rep movs byte ptr [rdi], byte ptr [rsi]

.Lfixup:
    mov rax, rdx
    sub rax, rcx
    ret

RIP_FIXUP(1b, .Lfixup)
FUNC_END(CopyUserGeneric)
