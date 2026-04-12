
.intel_syntax noprefix
.extern _stdlib_start

.section .text
    .global _start
    .type _start, @function
_start:
    call _stdlib_start
