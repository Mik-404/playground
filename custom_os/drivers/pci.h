#pragma once

#include <cstdint>
#include <cstddef>

#include "kernel/error.h"

namespace pci {
class Device {
public:
    uint8_t bus_ = 0;
    uint8_t dev_ = 0;
    uint8_t func_ = 0;
    uint32_t flags_ = 0;

public:
    void SetIrq(int irq) noexcept;
    void EnableBusMastering() noexcept;
    uint32_t ReadBar4() noexcept;
    bool MatchClass(uint8_t expected_cls, uint8_t expected_subcls) noexcept;
    bool Init() noexcept;
    uint32_t ReadConf(uint8_t offset) noexcept;
    void WriteConf(uint8_t offset, uint32_t value) noexcept;
};

constexpr size_t PCI_FLAG_MSI_CAPABLE = (1 << 0);
constexpr size_t PCI_CLASS_MASS_STORAGE_CONTROLLER = 0x01;
constexpr size_t PCI_SUBCLASS_IDE = 0x01;
constexpr size_t PCI_SUBCLASS_ATA = 0x05;
constexpr size_t PCI_SUBCLASS_SATA = 0x06;

bool FindDevice(uint8_t cls, uint8_t subclass, Device* dev) noexcept;

struct PrdtEntry {
    uint32_t buf_addr;
    uint16_t byte_count;
    uint16_t mark;
};

constexpr uint16_t PRDT_MARK_END = 1 << 15;

}
