ENTRY(_early_entry_point)

#include "defs.h"

#define DEFINE_HH_SECTION(name) name : AT(_phys_start_hh + ADDR(name) - KERNEL_IMAGE_START)

SECTIONS
{
    // TODO: fix multiboot header.
    . = SIZEOF_HEADERS + 200;

    .multiboot : ALIGN(4) {
        KEEP(*(.multiboot))
    }

    . = 2M;

    _phys_start_early = .;
    .early.text : {
        *(.early.text)
    }

    .early.rodata : {
        *(.early.rodata)
    }

    .early.data : {
        *(.early.data)
    }

    .early.bss : {
        *(.early.bss)
    }

    _phys_start_cpu_startup = ALIGN(PAGE_SIZE);
    .cpu_startup 0x8000 : AT(_phys_start_cpu_startup) {
        *(.cpu_startup)
    }
    _phys_end_cpu_startup = _phys_start_cpu_startup + . - 0x8000;
    . = _phys_end_cpu_startup;

    _phys_start_per_cpu = ALIGN(PAGE_SIZE);
    .per_cpu 0x0 : AT(_phys_start_per_cpu) {
        *(.per_cpu)
    }
    _phys_end_per_cpu = _phys_start_per_cpu + .;
    . = _phys_end_per_cpu;
    _phys_end_early = .;

    . = ALIGN(2M);
    _phys_start_hh = .;
    . = KERNEL_IMAGE_START;

    DEFINE_HH_SECTION(.text) {
        *(.text)
    }

    DEFINE_HH_SECTION(.rodata) {
        *(.rodata)
    }

    . = ALIGN(8);
    PROVIDE(_ctors_start = .);
    DEFINE_HH_SECTION(.ctors) {
        *(.ctors)
    }
    PROVIDE(_ctors_end = .);

    . = ALIGN(PAGE_SIZE);
    PROVIDE(_rip_fixups_start = .);
    DEFINE_HH_SECTION(.rip_fixups) {
        *(.rip_fixups)
    }
    PROVIDE(_rip_fixups_end = .);

    DEFINE_HH_SECTION(.ksyms) {
        *(.ksyms)
    }

    PROVIDE(_ksymstrs_start = .);
    PROVIDE(_ksymdefs_start = .);
    PROVIDE(_ksymdefs_end = .);

    DEFINE_HH_SECTION(.data) {
        *(.data)
    }

    DEFINE_HH_SECTION(.bss) {
        *(COMMON)
        *(.bss)
    }
    . = ALIGN(2M);
    _phys_end_hh = _phys_start_hh + . - KERNEL_IMAGE_START;

    /DISCARD/ : {
        *(.eh_frame)
        *(.note.gnu.*)
        *(.dynamic)
        *(.ctors)
    }
}
