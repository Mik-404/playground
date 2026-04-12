#pragma once

#define MULTIBOOT_HEADER_MAGIC    0xe85250D6
#define MULTIBOOT_HEADER_ARCH_32  0

#define MULTIBOOT_MAGIC 0x36d76289

#define MULTIBOOT_TAG_END           0
#define MULTIBOOT_TAG_CLI           1
#define MULTIBOOT_TAG_MMAP          6
#define MULTIBOOT_TAG_ACPI_OLD_RSDP 14

#define MULTIBOOT_MMAP_TYPE_RAM  1
#define MULTIBOOT_MMAP_TYPE_ACPI 3


#ifndef __ASSEMBLER__

#include "lib/common.h"

namespace multiboot {

struct Tag {
    uint32_t type;
    uint32_t size;
    char data[0];
};

struct BootInfo {
    uint32_t total_size;
    uint32_t reserved;
    Tag tags[0];
};

struct MemoryMapEntry {
    uint64_t base_addr;
    uint64_t length;
    uint32_t type;
    uint32_t reserved;
};

struct MemoryMapTag {
    Tag base;
    uint32_t entry_size;
    uint32_t entry_version;
    MemoryMapEntry entries[0];
};

void Init(uint32_t magic, void* info);

class MemoryMapIter {
private:
    const MemoryMapEntry* curr_ = nullptr;
    const MemoryMapTag* mmap_tag_ = nullptr;

public:
    MemoryMapIter();

    const MemoryMapEntry* Next() noexcept;
};

void Init(uint32_t magic, void* info);

const Tag* LookupTag(uint32_t tag_code);

void BootInfoRelocate();

}

#endif
