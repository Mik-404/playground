#include "file_table.h"
#include "kernel/panic.h"
#include "mm/kmalloc.h"
#include "mm/paging.h"
#include "mm/new.h"
#include "kernel/sched.h"

constexpr size_t kStartFiles = 10;

static mm::TypedObjectAllocator<FileTable> file_table_alloc;

FileTable::~FileTable() {
    for (size_t i = 0; i < max_files_; i++) {
        files_[i].Reset();
    }
}

kern::Errno FileTable::Grow(size_t minFiles) noexcept {
    if (max_files_ >= minFiles) {
        return kern::ENOERR;
    }

    size_t new_max_files = max_files_ > 0 ? max_files_ : kStartFiles;
    while (new_max_files < minFiles) {
        new_max_files = 3 * new_max_files / 2;
    }
    std::unique_ptr<vfs::FilePtr[]> newFiles(new vfs::FilePtr[new_max_files]);
    if (!newFiles) {
        return kern::ENOMEM;
    }
    if (files_) {
        for (size_t i = 0; i < max_files_; i++) {
            newFiles[i] = std::move(files_[i]);
        }
    }
    files_ = std::move(newFiles);
    max_files_ = new_max_files;

    return kern::ENOERR;
}

kern::Result<int32_t> FileTable::AssignFd(vfs::FilePtr f) noexcept {
    BUG_ON(!f);

    RawScopeLocker locker(mutex_);

    auto err = Grow(files_cnt_ + 1);
    if (!err.Ok()) {
        return err;
    }

    int32_t fd = -1;
    for (size_t i = 0; i < max_files_; i++) {
        if (!files_[i]) {
            fd = i;
            break;
        }
    }
    BUG_ON(fd == -1);
    files_[fd] = f;
    files_cnt_++;
    return fd;
}

kern::Result<vfs::FilePtr> FileTable::ReleaseFd(int32_t fd) noexcept {
    RawScopeLocker locker(mutex_);

    if (fd < 0 || (size_t)fd >= max_files_) {
        return kern::EBADF;
    }

    if (!files_[fd]) {
        return kern::EBADF;
    }

    files_cnt_--;
    kern::Result<vfs::FilePtr> res = std::move(files_[fd]);
    return res;
}

kern::Result<vfs::FilePtr> FileTable::ResolveFd(int32_t fd) noexcept {
    RawScopeLocker locker(mutex_);

    if (fd < 0 || (size_t)fd >= max_files_) {
        return kern::EBADF;
    }

    vfs::FilePtr f = files_[fd];
    if (!f) {
        return kern::EBADF;
    }
    return f;
}

kern::Result<std::unique_ptr<FileTable>> FileTable::Clone(const FileTable* src) noexcept {
    std::unique_ptr<FileTable> ft(new (file_table_alloc) FileTable());
    if (!ft) {
        return kern::ENOMEM;
    }

    auto err = ft->Grow(src ? src->max_files_ : kStartFiles);
    if (!err.Ok()) {
        return err;
    }

    if (!src) {
        return ft;
    }

    for (size_t i = 0; i < src->max_files_; i++) {
        if (src->files_[i]) {
            ft->files_[i] = src->files_[i]->Clone();
        }
    }

    return ft;
}
