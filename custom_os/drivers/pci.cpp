#include "pci.h"
#include "kernel/panic.h"
#include "arch/x86/x86.h"

constexpr size_t PCI_ADDRESS_PORT = 0xcf8;
constexpr size_t PCI_DATA_PORT = 0xcfc;

#define PCI_ADDRESS(bus, dev, func, offset) (((uint32_t)(bus) << 16) | ((uint32_t)(dev) << 11) | ((uint32_t)(func) << 8) | ((uint32_t)(offset) & 0xfc) | (1 << 31))

namespace pci {

uint32_t Addr(Device* dev, uint8_t offset) noexcept {
    return (uint32_t(dev->bus_) << 16) | (uint32_t(dev->dev_) << 11) | (uint32_t(dev->func_) << 8) | (uint32_t(offset) & 0xfc) | (1 << 31);
}

uint32_t Device::ReadConf(uint8_t offset) noexcept {
    x86::Outl(PCI_ADDRESS_PORT, Addr(this, offset));
    return x86::Inl(PCI_DATA_PORT);
}

void Device::WriteConf(uint8_t offset, uint32_t value) noexcept {
    x86::Outl(PCI_ADDRESS_PORT, Addr(this, offset));
    x86::Outl(PCI_DATA_PORT, value);
}

bool Device::MatchClass(uint8_t expected_cls, uint8_t expected_subcls) noexcept {
    uint32_t id = ReadConf(0x00);

    if (id == 0xffffffff) {
        return false;
    }

    uint32_t clsAndSubcls = ReadConf(0x08);
    uint8_t cls = (clsAndSubcls >> 24) & 0xff;
    uint8_t subclass = (clsAndSubcls >> 16) & 0xff;
    return cls == expected_cls && subclass == expected_subcls;
}

bool Device::Init() noexcept {
    uint32_t id = ReadConf(0x00);

    if (id == 0xffffffff) {
        return false;
    }

    uint32_t status = ReadConf(0x04);
    status >>= 16;
    if (status & (1 << 4)) {
        uint32_t capPtr = ReadConf(0x34) & 0xff;
        for (;;) {
            uint32_t capHdr = ReadConf(capPtr);
            uint32_t capType = capHdr & 0xff;
            switch (capType) {
            case 0x05:
                flags_ |= PCI_FLAG_MSI_CAPABLE;
                break;
            }

            capPtr = (capHdr >> 8) & 0xff;
            if (capPtr == 0) {
                break;
            }
        }
    }

    return true;
}

bool FindDevice(uint8_t cls, uint8_t subclass, Device* pciDev) noexcept {
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint16_t dev = 0; dev < 32; dev++) {
            pciDev->bus_ = bus;
            pciDev->dev_ = dev;
            pciDev->func_ = 0;

            if (pciDev->MatchClass(cls, subclass)) {
                pciDev->Init();
                return true;
            }

            uint32_t reg = pciDev->ReadConf(0x0c);
            uint8_t headerType = (reg >> 16) & 0xff;
            if (headerType & 0x80) {
                // Multifunction device.
                for (uint16_t func = 1; func < 8; func++) {
                    pciDev->func_ = func;
                    if (pciDev->MatchClass(cls, subclass)) {
                        pciDev->Init();
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

uint32_t Device::ReadBar4() noexcept {
    return ReadConf(0x20);
}

void Device::EnableBusMastering() noexcept {
    WriteConf(0x4, ReadConf(0x4) | (1 << 2));
}

}
