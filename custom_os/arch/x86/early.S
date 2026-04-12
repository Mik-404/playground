#include "arch/x86/asm_defs.h"
#include "arch/x86/gdt_asm.h"
#include "mm/paging.h"

.intel_syntax noprefix

# Section .early.bss contains zero-filled data. It doesn't appear in ELF file, but filled during image load.
.section .early.bss,"wa",@nobits
    .align PAGE_SIZE
    .zero 2*PAGE_SIZE
.Lbootstrap_stack_top:

.global early_p4
.type early_p4, @object
    .align PAGE_SIZE
early_p4:
    .zero PAGE_SIZE

.align PAGE_SIZE
.Learly_p3:
    .zero PAGE_SIZE

.global early_data
early_data:
_multiboot_info_addr:
    .quad 0
_multiboot_magic:
    .long 0


# Section .early.data contains some global variables.
.section .early.data,"wa"
# Setup GDT for 64-bit mode.
.Lgdt64:
    # 0: null segment.
    .long 0
    .long 0

    # 1: 64-bit kernel code segment.
    .long 0xFFFF
    # (0xF << 16) fills base 31:24.
    .long GDT_GRANULARITY | GDT_LONG | GDT_PRESENT | GDT_SYSTEM | (1<<11) | GDT_READ | (0xF << 16)

.Lgdt64_ptr:
    .word . - .Lgdt64
    .long .Lgdt64

.macro CLEAR start size
    mov esi, \start
    mov al, 0
    mov ecx, \size
    rep stosb
.endm

# 64-bit mode enabling process is described in ISDM, Volume 3A, Section 9.8.5.
.macro ENTER_LONG_MODE
.code32
    # Write PML4 physical address into CR3.
    lea eax, early_p4
    mov cr3, eax

    # Set PAE bit in CR4.
    mov eax, cr4
    or eax, 1<<5
    mov cr4, eax

    # Set IA32_EFER.LME bit (Long Mode Enable).
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1<<8
    wrmsr

    # Enable paging by setting PG bit in CR0.
    mov eax, cr0
    or eax, 1<<31
    mov cr0, eax

    # Load GDT with 64-bit code segment.
    lgdt .Lgdt64_ptr

    # Load prepared code segment with index 1 with RPL=0 from GDT.
    ljmp GDT_SEGMENT_SELECTOR(1, RPL_RING0):.Lret\@
.Lret\@:
    # We we are now running 64-bit mode. Yay!
.code64
    # Load prepared data segments: segment selector is 0b10000 = 0x10.
    mov ax, 0
    mov ds, ax
    mov es, ax
    mov gs, ax
    mov fs, ax
    # Loading null segment in SS allowed only in 64-bit mode.
    mov ss, ax
.endm


.section .early.text
# _early_entry_point is an entry point of our kernel, execution after bootloader starts here.
.global _early_entry_point
.type _early_entry_point, @function
_early_entry_point:
# Tell assembler, that we are running 32-bit protected mode.
.code32
    # Our kernel just loaded, current processor state is described in "I386 machine state" section of Multiboot2 specification.
    mov _multiboot_info_addr, ebx
    mov _multiboot_magic, eax

    # To enable long mode, we need to setup basic paging.
    # Here we setup identity mapping for the first four gigabytes.

    CLEAR early_p4 PAGE_SIZE
    CLEAR .Learly_p3 PAGE_SIZE

    # P4[0] = P3_addr | FLAG_PRESENT
    lea edi, early_p4
    lea eax, .Learly_p3
    or eax, PTE_PRESENT
    mov dword ptr [edi], eax

    # P3[0] = 0 | PTE_PAGE_SIZE | PTE_PRESENT
    lea edi, .Learly_p3
    mov dword ptr [edi], PTE_PAGE_SIZE | PTE_PRESENT

    ENTER_LONG_MODE

    # Setup booststrap stack and call early initialization.
    lea rsp, .Lbootstrap_stack_top
    call EarlyStart

    # Jump to the higher-half.
    lea rdi, early_data
    lea rax, KernelTrampoline
    jmp rax


# Code below runs in higher addresses, so it is placed in sections without .early prefix.
.section .bss,"wa",@nobits
    .align PAGE_SIZE
    .zero 2*PAGE_SIZE
.Lmain_stack_top:

.extern X86Init
.extern KernelMain
.extern KernelMainReturn

FUNC(KernelTrampoline):
    # Reset to the main stack.
    lea rsp, .Lmain_stack_top

    # Stop stack unwinding here.
    xor rbp, rbp

    # Now we are ready to start C++ part of higher-half kernel.
    lea rax, X86Init
    call rax

    # If KernelMain occasionally returns, KernelMainReturn will be called.
    lea rax, KernelMainReturn
    push rax

    lea rax, KernelMain
    jmp rax
FUNC_END(KernelTrampoline)

# Here comes CPU startup code.
.section .cpu_startup,"wa"
.code16
    cli
    cld
    ljmp 0:.Lstartup

# Setup GDT for entering protected mode.
.Lgdt32:
    # 0: null segment.
    .long 0
    .long 0

    # 1: 32-bit kernel code segment.
    .long 0xFFFF
    .long GDT_GRANULARITY | GDT_PRESENT | GDT_SYSTEM | GDT_CODE_SEG | GDT_SIZE | GDT_READ | (0xF << 16)

    # 2: 32-bit kernel data segment.
    .long 0xFFFF
    .long GDT_GRANULARITY | GDT_PRESENT | GDT_SYSTEM | GDT_READ | (0xF << 16)

.Lgdt32_ptr:
    .word . - .Lgdt32
    .long .Lgdt32

.Lstartup:
    lgdt [.Lgdt32_ptr]

    # Switch to the protected mode.
    mov eax, cr0
    or  eax, 1
    mov cr0, eax

    # Load code segment.
    ljmp GDT_SEGMENT_SELECTOR(1, RPL_RING0):.Lprotected_mode

.code32
.Lprotected_mode:
    mov ax, GDT_SEGMENT_SELECTOR(2, RPL_RING0)
    mov ds, ax
    mov es, ax
    mov ss, ax

    ENTER_LONG_MODE

.extern tmp_bootstrap_stack
.extern InitApCpu
    mov rsp, tmp_bootstrap_stack
    lea rax, InitApCpu
    call rax
