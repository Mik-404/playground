#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <array>

#include "defs.h"
#include "fs/vfs.h"
#include "fs/buffer.h"
#include "kernel/error.h"
#include "lib/mutex.h"

namespace ext2 {

constexpr size_t NUM_DIRECT_BLOCK_POINTERS = 12;
constexpr size_t SINGLE_INDRECT_BLOCK = 12;
constexpr size_t DOUBLE_INDRECT_BLOCK = 13;
constexpr size_t TRIPLE_INDIRECT_BLOCK = 14;

constexpr size_t MAX_DENTRY_NAME_SIZE = 255;

struct OnDiskExtendedSuperBlock {
    uint32_t s_first_ino;
    uint16_t s_inode_size;
    uint16_t s_block_group_nr;
    uint32_t s_feature_compat;
    uint32_t s_feature_incompat;
    uint32_t s_feature_ro_compat;
    uint8_t  s_uuid[16];
    uint8_t  s_volume_name[16];
    uint8_t  s_last_mounted[64];
    uint32_t s_algo_bitmap;
    uint8_t  s_prealloc_blocks;
    uint8_t  s_prealloc_dir_blocks;
    uint16_t unused0;
    uint8_t  s_journal_uuid[16];
    uint32_t s_journal_inum;
    uint32_t s_journal_dev;
    uint32_t s_last_orphan;
    uint8_t  unused1[788];
} __attribute__((packed));

struct OnDiskSuperBlock {
    uint32_t s_inodes_count;
    uint32_t s_blocks_count;
    uint32_t s_r_blocks_count;
    uint32_t s_free_blocks_count;
    uint32_t s_free_inodes_count;
    uint32_t s_first_data_block;
    uint32_t s_log_block_size;
    uint32_t s_log_frag_size;
    uint32_t s_blocks_per_group;
    uint32_t s_frags_per_group;
    uint32_t s_inodes_per_group;
    uint32_t s_mtime;
    uint32_t s_wtime;
    uint16_t s_mnt_count;
    uint16_t s_max_mnt_count;
    uint16_t s_magic;
    uint16_t s_state;
    uint16_t s_errors;
    uint16_t s_minor_rev_level;
    uint32_t s_lastcheck;
    uint32_t s_checkinterval;
    uint32_t s_creator_os;
    uint32_t s_rev_level;
    uint16_t s_def_resuid;
    uint16_t s_def_resgid;
    OnDiskExtendedSuperBlock extended;

    inline uint32_t BlockSize() noexcept {
        return 1024 << s_log_block_size;
    }
} __attribute__((packed));

struct OnDiskInode {
    uint16_t mode;
    uint16_t uid;
    uint32_t size_lo;
    uint32_t atime;
    uint32_t ctime;
    uint32_t mtime;
    uint32_t dtime;
    uint16_t gid;
    uint16_t links;
    uint32_t i_blocks;
    uint32_t flags;
    uint32_t os_specific1;
    uint32_t i_block[15];
    uint32_t generation_number;
    uint32_t acl;
    union {
        uint32_t size_hi;
        uint32_t dir_acl;
    };
    uint32_t fragment_block_address;
    uint8_t os_specific2[12];

    inline size_t Size() const noexcept {
        uint64_t res = size_hi;
        res = res << 32;
        res |= size_lo;
        return res;
    }

    inline void SetSize(size_t size) noexcept {
        size_lo = size & 0xffffffff;
        size_hi = size >> 32;
    }
};

struct OnDiskBlockGroupDesc {
    uint32_t bg_block_bitmap;
    uint32_t bg_inode_bitmap;
    uint32_t bg_inode_table;
    uint16_t bg_free_blocks_count;
    uint16_t bg_free_inodes_count;
    uint16_t bg_used_dirs_count;
    uint8_t  unused[14];
} __attribute__((packed));

struct OnDiskDirEntryHead {
    uint32_t inode;
    uint16_t rec_len;
    uint8_t name_len;
    uint8_t file_type;
    char name[0];

    std::string_view Name() const noexcept {
        return std::string_view(&name[0], name_len);
    }
} __attribute__((packed));

static_assert(sizeof(OnDiskSuperBlock) == 1024);
static_assert(sizeof(OnDiskInode) >= 128);
static_assert(sizeof(OnDiskBlockGroupDesc) == 32);

#define EXT2_SIGNATURE 0xef53

#define EXT2_S_IFSOCK 0xc000
#define EXT2_S_IFLNK  0xa000
#define EXT2_S_IFREG  0x8000
#define EXT2_S_IFBLK  0x6000
#define EXT2_S_IFDIR  0x4000
#define EXT2_S_IFCHR  0x2000
#define EXT2_S_IFIFO  0x1000
#define EXT2_S_IFMASK 0xf000

#define EXT2_S_ISUID  0x0800
#define EXT2_S_ISGID  0x0400
#define EXT2_S_ISVTX  0x0200

#define EXT2_S_IRUSR  0x0100
#define EXT2_S_IWUSR  0x0080
#define EXT2_S_IXUSR  0x0040
#define EXT2_S_IRGRP  0x0020
#define EXT2_S_IWGRP  0x0010
#define EXT2_S_IXGRP  0x0008
#define EXT2_S_IROTH  0x0004
#define EXT2_S_IWOTH  0x0002
#define EXT2_S_IXOTH  0x0001

#define EXT2_FT_UNKNOWN   0
#define EXT2_FT_REG_FILE  1
#define EXT2_FT_DIR       2
#define EXT2_FT_CHRDEV    3
#define EXT2_FT_BLKDEV    4
#define EXT2_FT_FIFO      5
#define EXT2_FT_SOCK      6
#define EXT2_FT_SYMLINK   7

class Ext2FsRoot;

class Ext2Inode : public vfs::Inode {
public:
    std::array<uint32_t, 15> block_pointers_;

    kern::Errno ReadPage(mm::Page&) noexcept override;

    kern::Errno WritePage(mm::Page&) noexcept override;

    kern::Errno Lookup(vfs::Dentry&) noexcept override;

    kern::Errno Create(vfs::Dentry& dentry, vfs::InodeType type, int mode) noexcept override;

    Ext2Inode(Ext2FsRoot* fs) noexcept;

    Ext2Inode(Ext2FsRoot* fs, uint32_t id, OnDiskInode* rawInode) noexcept;

    kern::Errno Sync() noexcept override;

    virtual ~Ext2Inode() {}
};

class Ext2FsRoot : public vfs::FileSystemRoot {
public:
    Mutex sb_mutex_;
    fs::BufferPtr sb_block_buf_;

    fs::BufferPtr bgd_block_buf_;
    std::unique_ptr<fs::BufferPool> buf_pool_;

    OnDiskSuperBlock* SuperBlock() const;

    OnDiskBlockGroupDesc* BlockGroupDesc(size_t group) const;

    size_t TotalGroups() const noexcept {
        auto sb = SuperBlock();
        return DIV_ROUNDUP(sb->s_blocks_count, sb->s_blocks_per_group);
    }

    ~Ext2FsRoot();

    kern::Result<fs::BufferPtr> ReadBlock(uint64_t block);
    kern::Result<vfs::InodePtr> ReadInode(uint32_t inode_id);

    kern::Result<fs::BufferPtr> ReadInodeBlock(uint32_t inode_id, size_t& offset) noexcept;
    kern::Result<uint32_t> ResolveOrAllocBlock(Ext2Inode* inode, size_t idx, bool alloc = false) noexcept;
    kern::Result<uint32_t> AllocInode(bool is_dir) noexcept;

private:
    kern::Result<uint32_t> BitmapAlloc(uint32_t bitmap_start, uint32_t bitmap_end) noexcept;
    kern::Result<uint32_t> AllocBlockInGroup(size_t group) noexcept;
    kern::Result<uint32_t> AllocBlock() noexcept;
    kern::Result<uint32_t> ResolveOrAllocPointerInBlock(Ext2Inode* inode, uint32_t block_with_ptrs_id, size_t idx, bool alloc) noexcept;
    kern::Result<uint32_t> EnsureBlockPointerInInode(Ext2Inode* inode, size_t idx, bool alloc) noexcept;
    kern::Result<uint32_t> AllocInodeInGroup(size_t group, bool is_dir) noexcept;
};

kern::Result<vfs::FileSystemRoot*> Mount(BlockDevice* dev) noexcept;

void Init();

}
