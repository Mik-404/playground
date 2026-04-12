#include "fs/inode.h"
#include "mm/vmem.h"
#include "mm/new.h"
#include "lib/murmur.h"
#include "kernel/sched.h"

namespace vfs {

kern::Result<mm::Page*> Inode::LoadPage(size_t idx) noexcept {
    mm::Page* page = page_cache_.GetPage(idx);
    if (!page) {
        return kern::ENOMEM;
    }
    PageCache::LockPage(*page);
    auto ret = ReadPage(*page);
    if (!ret.Ok()) {
        PageCache::UnlockPage(*page);
        page->Unref();
        return ret;
    }
    PageCache::UnlockPage(*page);
    page->Unref();
    return page;
}

kern::Result<size_t> InodeFile::ReadAt(mm::MemBuf buf, size_t offset) noexcept {
    if (!flags_.Has(FileFlag::Readable)) {
        return kern::EBADF;
    }

    Inode& inode = *dentry_->Inode();
    size_t size = inode.size_.load(std::memory_order_relaxed);
    if (offset >= size) {
        return 0;
    }

    size_t max_size_to_copy = buf.Size();
    if (offset + buf.Size() >= size) {
        max_size_to_copy = size - offset;
    }

    size_t start_page = offset / PAGE_SIZE;
    size_t end_page = DIV_ROUNDUP(offset + max_size_to_copy, PAGE_SIZE);
    size_t read = 0;
    size_t offset_in_page = offset % PAGE_SIZE;

    for (size_t idx = start_page; idx < end_page; idx++) {
        auto page = inode.LoadPage(idx);
        if (!page.Ok()) {
            return page.Err();
        }

        size_t to_copy = MIN(max_size_to_copy - read, PAGE_SIZE - offset_in_page);
        auto err = buf.CopyFrom((const void*)((char*)page->Virt() + offset_in_page), to_copy);
        if (!err.Ok()) {
            return err;
        }
        read += to_copy;
        buf.Skip(to_copy);
        offset_in_page = 0;
    }

    return read;
}

kern::Result<size_t> InodeFile::WriteAt(mm::MemBuf buf, size_t offset) noexcept {
    if (!flags_.Has(FileFlag::Writeable)) {
        return kern::EBADF;
    }

    Inode& inode = *dentry_->Inode();

    {
        RawScopeLocker locker(inode.mutex_);
        size_t size = inode.size_.load(std::memory_order_relaxed);
        if (size < offset + buf.size_) {
            inode.size_.store(offset + buf.size_, std::memory_order_relaxed);
            if (auto err = inode.Sync(); !err.Ok()) {
                return err;
            }
        }
    }


    size_t start_page = offset / PAGE_SIZE;
    size_t end_page = DIV_ROUNDUP(offset + buf.Size(), PAGE_SIZE);
    size_t written = 0;
    size_t offset_in_page = offset % PAGE_SIZE;

    for (size_t idx = start_page; idx < end_page; idx++) {
        auto page = inode.LoadPage(idx);
        if (!page.Ok()) {
            return page.Err();
        }

        size_t to_copy = MIN(buf.Size(), PAGE_SIZE - offset_in_page);
        auto err = buf.CopyTo((void*)((char*)page->Virt() + offset_in_page), to_copy);
        if (!err.Ok()) {
            return err;
        }

        if (auto err = inode.WritePage(**page); !err.Ok()) {
            return err;
        }

        written += to_copy;
        buf.Skip(to_copy);
        offset_in_page = 0;
    }

    return written;
}

kern::Result<size_t> InodeFile::Read(mm::MemBuf buf) noexcept {
    kern::Result<size_t> res = ReadAt(buf, position_);
    if (res.Ok()) {
        position_.fetch_add(*res, std::memory_order_relaxed);
    }
    return res;
}

kern::Result<size_t> InodeFile::Write(mm::MemBuf buf) noexcept {
    auto res = WriteAt(buf, position_);
    if (res.Ok()) {
        position_.fetch_add(*res, std::memory_order_relaxed);
    }
    return res;
}

namespace {

mm::TypedObjectAllocator<InodeFile> InodeFileAlloc;

}

kern::Result<FilePtr> InodeFile::FromDentry(FileFlags flags, DentryPtr dentry) noexcept {
    InodeFile* inodeFile = new (InodeFileAlloc) InodeFile(flags, std::move(dentry));
    if (!inodeFile) {
        return kern::ENOMEM;
    }

    return FilePtr(inodeFile);
}

FilePtr InodeFile::Clone() noexcept {
    return FilePtr(new (InodeFileAlloc) InodeFile(*this));
}

kern::Result<mm::Page*> InodeFile::LoadPage(size_t index) noexcept {
    return dentry_->Inode()->LoadPage(index);
}

}
