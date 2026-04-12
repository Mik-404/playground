#include <expected>
#include <memory>

#include "fs/file_table.h"
#include "fs/inode.h"
#include "fs/vfs.h"
#include "kernel/error.h"
#include "kernel/panic.h"
#include "kernel/printk.h"
#include "kernel/sched.h"
#include "kernel/syscall.h"
#include "lib/common.h"
#include "mm/kmalloc.h"
#include "mm/new.h"
#include "mm/page_alloc.h"
#include "uapi/fcntl.h"

namespace vfs {

static FileSystemRoot* root_fs = nullptr;
static mm::TypedObjectAllocator<Dentry> dentry_alloc;

mm::TypedObjectAllocator<char[PATH_MAX]> Path::kAlloc;

kern::Errno Close(File&) {
    return kern::ENOERR;
}

DentryPtr Dentry::LookupLocked(std::string_view name) noexcept {
    for (Dentry& dentry : children_head_) {
        if (std::string_view(dentry.Name()).compare(name) == 0) {
            return DentryPtr(&dentry);
        }
    }
    return nullptr;
}

DentryPtr Dentry::New(std::string_view name, DentryPtr parent) noexcept {
    DentryPtr new_dentry(new (dentry_alloc) Dentry());
    if (!new_dentry) {
        return nullptr;
    }

    std::unique_ptr<char[]> new_name(new char[name.size() + 1]);
    if (!new_name) {
        return nullptr;
    }
    memcpy(new_name.get(), name.data(), name.size());
    new_name[name.size()] = '\0';
    new_dentry->name_ = std::move(new_name);
    new_dentry->name_size_ = name.size();
    new_dentry->parent_ = std::move(parent);
    return new_dentry;
}

kern::Result<DentryPtr> Dentry::Lookup(std::string_view name) noexcept {
    if (name == ".") {
        return DentryPtr(this);
    } else if (name == "..") {
        return parent_;
    }

    // First try: search for dentry.
    DentryPtr cached_dentry = WithPreemptSafeLocked(lock_, [&]() {
        return LookupLocked(name);
    });

    if (cached_dentry) {
        // Yay, dentry is found.
        return cached_dentry;
    }

    RawScopeLocker locker(inode_->mutex_);

    // Did someone inserted dentry while we were waiting on inode mutex?
    cached_dentry = WithPreemptSafeLocked(lock_, [&]() {
        return LookupLocked(name);
    });

    if (cached_dentry) {
        return cached_dentry;
    }

    // Slow path: we must query the file system to perform the lookup.

    auto new_dentry = Dentry::New(name, DentryPtr(this));
    if (!new_dentry) {
        return kern::ENOMEM;
    }

    if (auto err = inode_->Lookup(*new_dentry); !err.Ok()) {
        return err;
    }

    // Insert new dentry into children list.
    WithPreemptSafeLocked(lock_, [&]() {
        new_dentry->Ref();
        children_head_.InsertLast(*new_dentry);
    });

    return new_dentry;
}

void Dentry::PrintTree(size_t ident) noexcept {
    for (Dentry& dentry : children_head_) {
        for (size_t i = 0; i < ident * 4; i++) {
            printk(" ");
        }
        printk("%s (%lu)", dentry.name_.get(), dentry.RefCount());
        if (!dentry.IsValid()) {
            printk(" (invalid)\n");
            continue;
        }
        printk(" -> %lu (%lu)\n", dentry.inode_->id_, dentry.inode_->RefCount());
        dentry.PrintTree(ident + 1);
    }
}

void Dentry::PrintTree() noexcept {
    printk("===== dentry cache =====\n");
    printk("/ -> %lu\n", inode_->id_);
    PrintTree(1);
    printk("===== dentry cache =====\n");
}

kern::Result<DentryPtr> DoLookup(Path& path) {
    // TODO: sync getting root dentry with umount?
    DentryPtr base_dir = root_fs->root_;

    DentryPtr dentry;
    const char* name = path.Cstr();
    for (;;) {
        while (*name != '\0' && *name == '/') {
            name++;
        }

        const char* next = name;
        while (*next != '\0' && *next != '/') {
            next++;
        }

        if (!base_dir->Inode()->IsDir()) {
            return kern::ENOTDIR;
        }

        auto res = base_dir->Lookup(std::string_view(name, next - name));
        if (!res.Ok()) {
            return res.Err();
        }

        if (*next == '\0') {
            return *res;
        }

        if (!res->IsValid()) {
            return kern::ENOENT;
        }

        base_dir = std::move(*res);
        name = next;
    }
}

kern::Result<DentryPtr> LookupOrCreate(Path& path, InodeType type, LookupFlags flags, int mode) noexcept {
    auto dentry = DoLookup(path);
    if (!dentry.Ok()) {
        return dentry;
    }
    if (dentry->IsValid()) {
        if (flags.HasAll(LookupFlag::Exclusive | LookupFlag::Create)) {
            return kern::EEXIST;
        }
        return dentry;
    } else {
        if (!flags.Has(LookupFlag::Create)) {
            return kern::ENOENT;
        }

        auto parent_inode = dentry->Parent()->Inode();
        RawScopeLocker locker(parent_inode->mutex_);

        if (dentry->IsValid()) {
            // Someone already created child inode while we were waiting on parent inode lock.
            return dentry;
        }

        auto err = parent_inode->Create(**dentry, type, mode);
        if (!err.Ok()) {
            return err;
        }
        return dentry;
    }

    return dentry;
}

void SetRoot(FileSystemRoot* fs) {
    root_fs = fs;
}

DentryPtr Dentry::MakeRoot() noexcept  {
    DentryPtr d(new (dentry_alloc) Dentry());
    if (!d) {
        return nullptr;
    }
    d->parent_ = d;
    return d;
}

kern::Result<FilePtr> Open(PathPtr path, FileFlags file_flags, LookupFlags lookup_flags) {
    kern::Result<DentryPtr> dentry = LookupOrCreate(*path, InodeType::Regular, lookup_flags, 0777);
    if (!dentry.Ok()) {
        return dentry.Err();
    }

    return InodeFile::FromDentry(file_flags, std::move(*dentry));
}

kern::Result<PathPtr> Path::FromUser(const char* userPath) noexcept {
    PathPtr path(new Path());
    if (!path) {
        return kern::ENOMEM;
    }
    path->path_ = std::unique_ptr<char[]>(new (Path::kAlloc) char[PATH_MAX]);
    if (!path->path_) {
        return kern::ENOMEM;
    }
    auto copied = mm::CopyStringFromUser(path->Cstr(), userPath, PATH_MAX);
    if (!copied.Ok()) {
        return copied.Err();
    }
    if (*copied == PATH_MAX) {
        return kern::ENAMETOOLONG;
    }
    path->path_[*copied] = '\0';
    return path;
}

kern::Result<PathPtr> Path::FromKernel(std::string_view kernPath) noexcept {
    if (kernPath.size() + 1 > PATH_MAX) {
        return kern::ENAMETOOLONG;
    }

    PathPtr path(new Path());
    if (!path) {
        return kern::ENOMEM;
    }
    path->path_ = std::unique_ptr<char[]>(new (Path::kAlloc) char[PATH_MAX]);
    if (!path->path_) {
        return kern::ENOMEM;
    }
    memcpy(path->Cstr(), kernPath.data(), kernPath.size());
    path->path_[kernPath.size()] = '\0';
    return path;
}

kern::Result<int32_t> SysOpen(sched::Task* curr, char* pathStr, int flags) noexcept {
    auto path = Path::FromUser(pathStr);
    if (!path.Ok()) {
        return path.Err();
    }

    LookupFlags lookup_flags;
    FileFlags open_flags;
    if (flags & O_RDONLY) {
        open_flags |= FileFlag::Readable;
    }
    if (flags & O_WRONLY) {
        open_flags |= FileFlag::Writeable;
    }
    if (flags & O_CREAT) {
        lookup_flags |= LookupFlag::Create;
    }

    kern::Result<FilePtr> f = Open(std::move(*path), open_flags, lookup_flags);
    if (!f.Ok()) {
        return f.Err();
    }

    auto fd = curr->file_table->AssignFd(std::move(*f));
    if (!fd.Ok()) {
        return fd.Err();
    }

    return *fd;
}
REGISTER_SYSCALL(open, SysOpen);

kern::Result<size_t> SysRead(sched::Task* curr, int32_t fd, char* buf, size_t sz) noexcept {
    auto f = curr->file_table->ResolveFd(fd);
    if (!f.Ok()) {
        return f.Err();
    }
    return f->Read(mm::MemBuf(buf, sz));
}
REGISTER_SYSCALL(read, SysRead);

kern::Result<size_t> SysWrite(sched::Task* curr, int32_t fd, char* buf, size_t sz) noexcept {
    auto f = curr->file_table->ResolveFd(fd);
    if (!f.Ok()) {
        return f.Err();
    }
    return f->Write(mm::MemBuf(buf, sz));
}
REGISTER_SYSCALL(write, SysWrite);

kern::Errno SysClose(sched::Task* curr, int32_t fd) noexcept {
    auto f = curr->file_table->ReleaseFd(fd);
    if (!f.Ok()) {
        return f.Err();
    }
    return Close(*f.Val());
}
REGISTER_SYSCALL(close, SysClose);

void Init() {
}

} // namespace vfs

