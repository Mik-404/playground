#include "fs/ext2.h"

namespace ext2 {

kern::Errno Ext2Inode::Create(vfs::Dentry& dentry, vfs::InodeType /* type */, int /* mode */) noexcept {
    if (dentry.Name().size() >= MAX_DENTRY_NAME_SIZE) {
        return kern::ENAMETOOLONG;
    }

    if (type_ != vfs::InodeType::Dir) {
        return kern::ENOTDIR;
    }

    return kern::ENOSYS;
}

kern::Result<uint32_t> Ext2FsRoot::ResolveOrAllocBlock(Ext2Inode* inode, size_t idx, bool /* alloc */) noexcept {
    if (idx < NUM_DIRECT_BLOCK_POINTERS) {
        return inode->block_pointers_[idx];
    }

    return kern::EFBIG;
}

}
