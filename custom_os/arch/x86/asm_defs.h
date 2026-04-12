#pragma once

#define FUNC(name) \
    .section .text; \
    .global name; \
    .type name, @function; \
name

#define FUNC_END(name) .size name, . - name

#define RIP_FIXUP(addr, next) \
    .pushsection .rip_fixups,"a"; \
    .quad addr; \
    .quad next; \
    .popsection
