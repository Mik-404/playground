#pragma once

#include "fs/vfs.h"
#include "kernel/error.h"

namespace vfs {

class InodeFile : public File {
public:
    DentryPtr dentry_;
    std::atomic<size_t> position_ = 0;

    InodeFile(FileFlags flags, DentryPtr dentry)
        : File(flags | FileFlag::Mappable)
        , dentry_(std::move(dentry))
    {}

    InodeFile(const InodeFile& file)
        : File(file)
        , dentry_(file.dentry_)
        , position_(file.position_.load(std::memory_order_relaxed))
    {}

    static kern::Result<FilePtr> FromDentry(FileFlags flags, DentryPtr dentry) noexcept;

    kern::Result<size_t> Read(mm::MemBuf buf) noexcept override;
    kern::Result<size_t> Write(mm::MemBuf buf) noexcept override;
    kern::Result<size_t> ReadAt(mm::MemBuf buf, size_t pos) noexcept override;
    kern::Result<size_t> WriteAt(mm::MemBuf buf, size_t pos) noexcept override;

    FilePtr Clone() noexcept override;
    kern::Result<mm::Page*> LoadPage(size_t index) noexcept override;
};

}
