#pragma once

#include <memory>

#include <cstddef>
#include <cstdint>

#include "fs/vfs.h"
#include "lib/seq_bitmap.h"
#include "lib/mutex.h"
#include "mm/page_alloc.h"

class FileTable {
public:
    Mutex mutex_;
    std::unique_ptr<vfs::FilePtr[]> files_;
    size_t files_cnt_ = 0;
    size_t max_files_ = 0;

    static kern::Result<std::unique_ptr<FileTable>> Clone(const FileTable* src) noexcept;

    kern::Result<int32_t> AssignFd(vfs::FilePtr f) noexcept;
    kern::Result<vfs::FilePtr> ReleaseFd(int32_t f) noexcept;
    kern::Result<vfs::FilePtr> ResolveFd(int32_t fd) noexcept;

    ~FileTable();

private:
    kern::Errno Grow(size_t minFiles) noexcept;
};

