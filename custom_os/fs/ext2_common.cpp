#include <memory>

#include "drivers/ata.h"
#include "fs/block_device.h"
#include "fs/ext2.h"
#include "fs/vfs.h"
#include "kernel/panic.h"
#include "lib/common.h"
#include "mm/page_alloc.h"
#include "mm/kmalloc.h"
#include "mm/new.h"

namespace ext2 {

namespace {

mm::TypedObjectAllocator<Ext2Inode> ext2_inode_alloc;

vfs::InodeType InodeTypeFromMode(uint16_t mode) noexcept {
    switch (mode & EXT2_S_IFMASK) {
    case EXT2_S_IFREG:
        return vfs::InodeType::Regular;
    case EXT2_S_IFDIR:
        return vfs::InodeType::Dir;
    case EXT2_S_IFBLK:
        return vfs::InodeType::BlockDev;
    case EXT2_S_IFCHR:
        return vfs::InodeType::CharDev;
    case EXT2_S_IFIFO:
        return vfs::InodeType::FIFO;
    case EXT2_S_IFLNK:
        return vfs::InodeType::Symlink;
    case EXT2_S_IFSOCK:
        return vfs::InodeType::Socket;
    default:
        return vfs::InodeType::Invalid;
    }
}

uint16_t ModeFromInodeType(vfs::InodeType type) noexcept {
    switch (type) {
    case vfs::InodeType::Regular:
        return EXT2_S_IFREG;
    case vfs::InodeType::Dir:
        return EXT2_S_IFDIR;
    case vfs::InodeType::BlockDev:
        return EXT2_S_IFBLK;
    case vfs::InodeType::CharDev:
        return EXT2_S_IFCHR;
    case vfs::InodeType::FIFO:
        return EXT2_S_IFIFO;
    case vfs::InodeType::Symlink:
        return EXT2_S_IFLNK;
    case vfs::InodeType::Socket:
        return EXT2_S_IFSOCK;
    default:
        BUG_ON_REACH();
    }
}

}

Ext2Inode::Ext2Inode(Ext2FsRoot* fs) noexcept {
    blocks_ = 0;
    size_ = 0;
    owner_ = fs;
    for (size_t i = 0; i < block_pointers_.size(); i++) {
        block_pointers_[i] = 0;
    }
}

Ext2Inode::Ext2Inode(Ext2FsRoot* fs, uint32_t id, OnDiskInode* on_disk_inode) noexcept {
    owner_ = fs;
    id_ = id;
    type_ = InodeTypeFromMode(on_disk_inode->mode);
    blocks_ = on_disk_inode->i_blocks;
    size_ = on_disk_inode->Size();
    for (size_t i = 0; i < block_pointers_.size(); i++) {
        block_pointers_[i] = on_disk_inode->i_block[i];
    }
}

kern::Errno Ext2Inode::Sync() noexcept {
    auto fs = reinterpret_cast<Ext2FsRoot*>(owner_);

    size_t offset = 0;
    auto inode_buf = fs->ReadInodeBlock(id_, offset);
    if (!inode_buf.Ok()) {
        return inode_buf.Err();
    }

    OnDiskInode* on_disk_inode = (OnDiskInode*)((uintptr_t)inode_buf->Data() + offset);

    on_disk_inode->mode = ModeFromInodeType(type_);
    on_disk_inode->SetSize(size_);
    for (size_t i = 0; i < block_pointers_.size(); i++) {
        on_disk_inode->i_block[i] = block_pointers_[i];
    }
    on_disk_inode->i_blocks = blocks_;

    inode_buf->MarkDirty();

    return kern::ENOERR;
}

constexpr size_t SUPER_BLOCK_OFFSET = 1024;

OnDiskSuperBlock* Ext2FsRoot::SuperBlock() const {
    uint8_t* data = (uint8_t*)sb_block_buf_->Data();
    return (OnDiskSuperBlock*)(data + SUPER_BLOCK_OFFSET % block_size_);
}

OnDiskBlockGroupDesc* Ext2FsRoot::BlockGroupDesc(size_t group) const {
    OnDiskBlockGroupDesc* descrs = (OnDiskBlockGroupDesc*)bgd_block_buf_->Data();
    return &descrs[group];
}

kern::Result<fs::BufferPtr> Ext2FsRoot::ReadBlock(uint64_t block) {
    return buf_pool_->ReadBuffer(block);
}

kern::Result<fs::BufferPtr> Ext2FsRoot::ReadInodeBlock(uint32_t inode_id, size_t& offset) noexcept {
    OnDiskSuperBlock* sb = SuperBlock();

    uint32_t group = (inode_id - 1) / sb->s_inodes_per_group;
    uint32_t inode_index = (inode_id - 1) % sb->s_inodes_per_group;

    uint32_t inode_size = 128;
    if (sb->s_rev_level >= 1) {
        inode_size = sb->extended.s_inode_size;
    }

    uint32_t block = BlockGroupDesc(group)->bg_inode_table + (inode_index * inode_size) / block_size_;
    kern::Result<fs::BufferPtr> inode_buf = ReadBlock(block);
    if (!inode_buf.Ok()) {
        return inode_buf.Err();
    }

    printk("[ext2] read inode %u in block %lu offset %lu\n", inode_id, block, offset);

    offset = (inode_index * inode_size) % block_size_;
    return *inode_buf;
}

kern::Result<vfs::InodePtr> Ext2FsRoot::ReadInode(uint32_t inode_id) {
    size_t offset = 0;
    auto inode_buf = ReadInodeBlock(inode_id, offset);
    if (!inode_buf.Ok()) {
        return inode_buf.Err();
    }

    OnDiskInode* on_disk_inode = (OnDiskInode*)((uintptr_t)inode_buf->Data() + offset);

    vfs::InodePtr inode(new (ext2_inode_alloc) Ext2Inode(this, inode_id, on_disk_inode));
    if (!inode) {
        return kern::ENOMEM;
    }
    return inode;
}

Ext2FsRoot::~Ext2FsRoot() {
}

namespace {

uint32_t ext2_max_blocks(Ext2FsRoot* fs, Ext2Inode* info) {
    return info->blocks_ / (2 << fs->SuperBlock()->s_log_block_size);
}

}

kern::Errno Ext2Inode::Lookup(vfs::Dentry& dentry) noexcept {
    auto fs = reinterpret_cast<Ext2FsRoot*>(owner_);

    size_t max_pages = DIV_ROUNDUP(size_, PAGE_SIZE);
    for (size_t page_idx = 0; page_idx < max_pages; page_idx++) {
        kern::Result<mm::Page*> page = LoadPage(page_idx);
        if (!page.Ok()) {
            return page.Err();
        }


        OnDiskDirEntryHead* head = (OnDiskDirEntryHead*)page->Virt();
        uint8_t* end = AdvancePointer((uint8_t*)head, PAGE_SIZE);
        for (; (uint8_t*)head < end; head = AdvancePointer(head, head->rec_len)) {
            if (head->inode == 0) {
                continue;
            }

            if (dentry.Name().compare(head->Name()) != 0) {
                continue;
            }

            auto inode = fs->ReadInode(head->inode);
            if (!inode.Ok()) {
                return inode.Err();
            }
            dentry.SetInode(std::move(*inode));
            return kern::ENOERR;
        }
    }

    dentry.MakeInvalid();
    return kern::ENOERR;
}

kern::Errno Ext2Inode::ReadPage(mm::Page& page) noexcept {
    auto fs = reinterpret_cast<Ext2FsRoot*>(owner_);

    memset(page.Virt(), '\0', PAGE_SIZE);

    size_t size = size_.load(std::memory_order_relaxed);
    size_t offset = page.pc_index * PAGE_SIZE;
    if (offset >= size) {
        return kern::ENOERR;
    }

    size_t block = page.pc_index;
    kern::Result<uint32_t> block_id = fs->ResolveOrAllocBlock(this, block, false);
    if (!block_id.Ok()) {
        return block_id.Err();
    }

    // Page containing block is not yet allocated on disk.
    if (*block_id == 0) {
        return kern::ENOERR;
    }

    auto block_buf = fs->ReadBlock(*block_id);
    if (!block_buf.Ok()) {
        return block_buf.Err();
    }

    size_t sz = std::min<size_t>(PAGE_SIZE, size - offset);
    memcpy(page.Virt(), block_buf->Data(), sz);

    printk("[ext2] read page, inode=%lu page_idx=%lu\n", id_, page.pc_index, sz);

    return kern::ENOERR;
}

kern::Errno Ext2Inode::WritePage(mm::Page& page) noexcept {
    auto fs = reinterpret_cast<Ext2FsRoot*>(owner_);

    size_t size = size_.load(std::memory_order_relaxed);
    size_t offset = page.pc_index * PAGE_SIZE;
    if (offset >= size) {
        // TODO: resize lock
        return kern::ENOERR;
    }

    printk("[ext2] write page, inode=%lu page_idx=%lu\n", id_, page.pc_index);

    size_t block = page.pc_index;
    auto block_id = fs->ResolveOrAllocBlock(this, block, true);
    if (!block_id.Ok()) {
        return block_id.Err();
    }

    auto block_buf = fs->ReadBlock(*block_id);
    if (!block_buf.Ok()) {
        return block_buf.Err();
    }

    {
        PreemptSafeScopeLocker locker(**block_buf);
        size_t sz = std::min<size_t>(PAGE_SIZE, size - offset);
        memcpy(block_buf->Data(), (void*)page.Virt(), sz);
        block_buf->MarkDirty();
    }

    return kern::ENOERR;
}

kern::Result<vfs::FileSystemRoot*> Mount(BlockDevice* dev) noexcept {
    std::unique_ptr<Ext2FsRoot> fs(new Ext2FsRoot());
    if (!fs) {
        return kern::ENOMEM;
    }

    fs->dev_ = dev;

    auto sb_buf = fs::Buffer::ReadNoCache(dev, 1, 1024);
    if (!sb_buf.Ok()) {
        return sb_buf.Err();
    }

    auto sb = (OnDiskSuperBlock*)sb_buf->Data();

    if (sb->s_magic != EXT2_SIGNATURE) {
        printk("[ext2] magic missmatch: 0x%x != 0x%x\n", (uint32_t)sb->s_magic, EXT2_SIGNATURE);
        return kern::EINVAL;
    }

    size_t block_size = sb->BlockSize();
    if (block_size != PAGE_SIZE) {
        printk("[ext2] unsupported block size %lu\n", block_size);
        return kern::EINVAL;
    }

    fs->block_size_ = block_size;
    fs->buf_pool_ = std::unique_ptr<fs::BufferPool>(new fs::BufferPool(dev, block_size));
    if (!fs->buf_pool_) {
        return kern::ENOMEM;
    }
    sb_buf = fs->buf_pool_->ReadBuffer(SUPER_BLOCK_OFFSET / block_size);
    if (!sb_buf.Ok()) {
        printk("[ext2] cannot re-read superblock: %e\n", sb_buf.Err().Code());
        return sb_buf.Err();
    }

    fs->sb_block_buf_ = std::move(*sb_buf);

    sb = fs->SuperBlock();

    printk("[ext2] superblock: rev=%d.%d, block_size=%d, mnt_count=%lu\n", sb->s_rev_level, sb->s_minor_rev_level, block_size, sb->s_mnt_count);
    sb->s_mnt_count++;
    fs->sb_block_buf_->MarkDirty();

    // Block group descriptor block is the block following superblock.
    size_t bgdBlock = 1024 / block_size + 1;
    auto bgdblock_buf = fs->ReadBlock(bgdBlock);
    if (!bgdblock_buf.Ok()) {
        return bgdblock_buf.Err();
    }
    fs->bgd_block_buf_ = *bgdblock_buf;

    // Root inode ID is always 2.
    auto rootInode = fs->ReadInode(2);
    if (!rootInode.Ok()) {
        printk("[ext2] cannot read root inode: %e\n", rootInode.Err().Code());
        return rootInode.Err();
    }

    if (!rootInode->IsDir()) {
        printk("[ext2] root inode is not a directory\n");
        return kern::EINVAL;
    }

    fs->root_ = vfs::Dentry::MakeRoot();
    fs->root_->SetInode(std::move(*rootInode));

    return fs.release();
}

void Init() {
}

}
