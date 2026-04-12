#pragma once

#include <cstddef>
#include <cstdint>

#include "kernel/panic.h"
#include "mm/kasan.h"

namespace acpi {

struct Rsdp {
    char signature[8];
    uint8_t checksum;
    char oemid[6];
    uint8_t revision;
    uint32_t RsdtAddr;
} __attribute__ ((packed));

struct SdtHeader {
    char Signature[4];
    uint32_t Length;
    uint8_t Revision;
    uint8_t Checksum;
    char Oemid[6];
    char OemTableid[8];
    uint32_t OemRevision;
    uint32_t CreatorId;
    uint32_t CreatorRevision;
} __attribute__((packed));

struct Sdt {
    SdtHeader Header;

    union {
        uint32_t Entries[0];
        char Data[0];
    };
} __attribute__((packed));

#define ACPI_ADDRESS_SPACE_MEMORY 0
#define ACPI_ADDRESS_SPACE_IO     1

struct Address {
    uint8_t AddressSpaceId;
    uint8_t RegisterBitWidth;
    uint8_t RegisterBitOffset;
    uint8_t Reserved0;
    uint64_t Base;
} __attribute__((packed));

struct MadtEntryLapic {
    uint8_t AcpiProcessorUid;
    uint8_t ApicId;
    uint32_t Flags;
} __attribute__((packed));

struct MadtEntryIoapic {
    uint8_t IoapicId;
    uint8_t Reserved;
    uint32_t IoapicAddr;
    uint32_t Gsib;
} __attribute__((packed));

struct MadtEntry {
    uint8_t Type;
    uint8_t Length;
    uint8_t Data[0];
} __attribute__((packed));

struct MadtHeader {
    SdtHeader Acpi;
    uint32_t LapicAddr;
    uint32_t Flags;
    MadtEntry FirstEntry;
} __attribute__((packed));

struct HpetHeader {
    SdtHeader header;
    uint32_t event_timer_block_id;
    Address base_address;
    uint8_t hpet_number;
    uint16_t minimum_tick;
    uint8_t page_protection;
} __attribute__((packed));

#define MADT_TYPE_LAPIC          0
#define MADT_TYPE_IOAPIC         1
#define MADT_TYPE_ISO            2
#define MADT_TYPE_NMI            3
#define MADT_TYPE_LAPIC_OVERRIDE 4

void Init();

Sdt* GetRsdt();
Rsdp* GetRsdp();

NO_KASAN const Sdt* LookupSdt(Sdt* root, const char* signature);

template <typename T>
NO_KASAN const T* LookupRsdt(const char* signature) {
    const Sdt* sdt = LookupSdt(GetRsdt(), signature);
    if (sdt && sdt->Header.Length < sizeof(T)) {
        panic("type size %lu is greater than table length %lu", sizeof(T), sdt->Header.Length);
    }
    return (T*)sdt;
}

}
