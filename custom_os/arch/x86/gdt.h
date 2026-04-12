#pragma once

#include <cstdint>

#include "gdt_asm.h"

namespace x86 {

struct GdtDescriptor {
    uint32_t dw0;
    uint32_t dw1;
};

#define GDT_DESCRIPTOR(gdt, idx, base, limit, flags) gdt->descs[idx] = (x86::GdtDescriptor){                           \
    .dw0 = ((((uint32_t)(base)) & 0xFFFF) << 16) | ((uint32_t)(limit) & 0xFFFF),                                \
    .dw1 = ((uint32_t)(((base) >> 16) & 0xFF)) | (flags) | GDT_PRESENT | (((uint32_t)((limit) >> 16) & 0xF) << 16) | (((uint32_t)((base) >> 24) & 0xFF) << 24),  \
}

#define GDT_DESCRIPTOR_64(gdt, idx, flags) GDT_DESCRIPTOR(gdt, idx, 0, 0xFFFFF, flags)

#define TSS_DESCRIPTOR_64(gdt, idx, base, limit, flags) \
    GDT_DESCRIPTOR(gdt, idx, base, limit, flags); \
    gdt->descs[idx + 1] = (x86::GdtDescriptor){.dw0 = ((uint32_t)(base >> 32)) & 0xFFFFFFFF, .dw1 = 0}

#define GDT_MAX_DESCRIPTORS 16

struct Gdt {
    GdtDescriptor descs[GDT_MAX_DESCRIPTORS];
};

struct GdtPointer {
    uint16_t size;
    uint64_t base;
} __attribute__((packed));

}
