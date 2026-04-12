#pragma once

#include <memory>
#include <string_view>

#include <cstddef>
#include <cstdint>

#include "lib/atomic.h"
#include "lib/shared_ptr.h"
#include "lib/mutex.h"
#include "lib/flags.h"
#include "fs/block_device.h"
#include "fs/page_cache.h"
#include "mm/obj_alloc.h"
#include "mm/membuf.h"

namespace vfs {

constexpr size_t PATH_MAX = 4096;

class FileSystemRoot;
class Inode;
class Dentry;
class File;

enum class InodeType : uint8_t {
    Dir = 0,
    CharDev = 1,
    BlockDev = 2,
    FIFO = 3,
    Symlink = 4,
    Regular = 5,
    Socket = 6,
    Invalid = 7,
};

class Inode : public RefCounted {
public:
    uint32_t id_ = 0;
    FileSystemRoot* owner_ = nullptr;
    PageCache page_cache_;

    InodeType type_ = InodeType::Invalid;
    int mode = 0;
    std::atomic<size_t> size_;
    uint32_t blocks_;
    Mutex mutex_;

    virtual kern::Errno ReadPage(mm::Page&) noexcept = 0;

    virtual kern::Errno WritePage(mm::Page&) noexcept = 0;

    virtual kern::Errno Lookup(Dentry&) noexcept = 0;

    virtual kern::Errno Create(Dentry& dentry, InodeType type, int mode) noexcept = 0;

    virtual kern::Errno Sync() noexcept = 0;

    kern::Result<mm::Page*> LoadPage(size_t idx) noexcept;

    bool IsDir() const {
        return type_ == InodeType::Dir;
    }
};

using InodePtr = IntrusiveSharedPtr<Inode, NoOpRefCountedTracker<Inode>>;

class Dentry;
using DentryPtr = IntrusiveSharedPtr<Dentry, NoOpRefCountedTracker<Dentry>>;

class Dentry : public RefCounted {
private:
    SpinLock lock_;

    InodePtr inode_;
    DentryPtr parent_;

    ListNode children_list_;
    ListHead<Dentry, &Dentry::children_list_> children_head_;

    std::unique_ptr<char[]> name_;
    size_t name_size_ = 0;

public:
    static DentryPtr New(std::string_view name, DentryPtr parent) noexcept;

    static DentryPtr MakeRoot() noexcept;

    std::string_view Name() const noexcept {
        return std::string_view(name_.get(), name_size_);
    }

    bool IsValid() const noexcept {
        return static_cast<bool>(inode_);
    }

    void MakeInvalid() noexcept {
        inode_ = nullptr;
    }

    void SetInode(InodePtr inode) noexcept {
        inode_ = std::move(inode);
    }

    InodePtr Inode() const noexcept {
        return inode_;
    }

    DentryPtr Parent() const noexcept {
        return parent_;
    }

    kern::Result<DentryPtr> WaitLookup() noexcept;

    kern::Result<DentryPtr> Lookup(std::string_view name) noexcept;

    void PrintTree() noexcept;

private:
    DentryPtr LookupLocked(std::string_view name) noexcept;

    void PrintTree(size_t ident) noexcept;
};

class FileSystemRoot {
public:
    BlockDevice* dev_ = nullptr;
    size_t block_size_ = 0;
    DentryPtr root_ = nullptr;
};

class File;
using FilePtr = IntrusiveSharedPtr<File>;

enum class FileFlag {
    Readable = 1 << 0,
    Writeable = 1 << 1,
    Mappable = 1 << 3,
};
using FileFlags = BitFlags<FileFlag>;

class File : public RefCounted {
public:
    SpinLock lock_;
    FileFlags flags_;

    File(FileFlags flags) : flags_(flags) {}

    File(const File& f)
        : flags_(f.flags_)
    {}

    virtual ~File() noexcept {}

    virtual kern::Result<size_t> Read(mm::MemBuf buf) noexcept = 0;

    virtual kern::Result<size_t> Write(mm::MemBuf buf) noexcept = 0;

    virtual kern::Result<size_t> ReadAt(mm::MemBuf, size_t) noexcept {
        return kern::ENOSYS;
    }

    virtual kern::Result<size_t> WriteAt(mm::MemBuf, size_t) noexcept {
        return kern::ENOSYS;
    }

    virtual kern::Result<mm::Page*> LoadPage(size_t) noexcept {
        return kern::ENOSYS;
    }

    virtual FilePtr Clone() noexcept = 0;
};

class Path;
using PathPtr = IntrusiveSharedPtr<Path>;

class Path : public RefCounted {
private:
    std::unique_ptr<char[]> path_ = nullptr;

private:
    static mm::TypedObjectAllocator<char[PATH_MAX]> kAlloc;

public:
    static kern::Result<PathPtr> FromUser(const char* uptr) noexcept;
    static kern::Result<PathPtr> FromKernel(std::string_view kptr) noexcept;

    const char* Cstr() const noexcept {
        return &path_[0];
    }

    char* Cstr() noexcept {
        return &path_[0];
    }
};

enum class LookupFlag {
    Create = 1 << 0,
    Exclusive = 1 << 1,
};
using LookupFlags = BitFlags<LookupFlag>;

void Init();

typedef int64_t (*copy_fn_t)(void* dst, const void* src, size_t sz);

int UserReadAt(File& f, void* buf, size_t offset, size_t sz);
int KernelReadAt(File& f, void* buf, size_t offset, size_t sz);

int Write(File& f, void* buf, size_t sz);
int WriteAt(File& f, void* buf, size_t offset, size_t sz);

int Seek(File& f, size_t off);

kern::Result<FilePtr> Open(PathPtr, FileFlags file_flags, LookupFlags lookup_flags = {});
kern::Errno Close(File& f);

void SetRoot(FileSystemRoot* fs);

} // namespace vfs
