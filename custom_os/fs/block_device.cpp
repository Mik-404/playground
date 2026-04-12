#include <cstdint>

#include "fs/block_device.h"
#include "mm/new.h"
#include "mm/obj_alloc.h"
#include "kernel/panic.h"

static ListHead<BlockDevice, &BlockDevice::dev_list_> all_block_devices;

kern::Errno BlockDevice::Register(BlockDevice& dev, std::string_view name) noexcept {
    char* dev_name = new char[name.size() + 1];
    if (!dev_name) {
        return kern::ENOMEM;
    }
    dev_name[name.size()] = '\0';
    memcpy(dev_name, name.data(), name.size());
    dev.name_ = dev_name;

    all_block_devices.InsertLast(dev);
    return kern::ENOERR;
}

BlockDevice* BlockDevice::ByName(std::string_view name) noexcept {
    for (BlockDevice& bdev : all_block_devices) {
        if (name.compare(bdev.name_) == 0) {
            return &bdev;
        }
    }
    return nullptr;
}

void BlockDevice::EndRequest(RequestPtr req) noexcept {
    req->OnEnd();
}
