#pragma once

#include <boost/intrusive/set.hpp>

#include "fs/block_device.h"
#include "kernel/wait.h"
#include "mm/page_alloc.h"

namespace fs {

class BufferPool;

class Buffer;
using BufferPtr = IntrusiveSharedPtr<Buffer, NoOpRefCountedTracker<Buffer>>;

class Buffer : public RefCounted {
public:
    static constexpr uint32_t Locked = 1 << 0;
    static constexpr uint32_t Dirty = 1 << 1;
    static constexpr uint32_t UpToDate = 1 << 2;
    static constexpr uint32_t InPool = 1 << 3;

private:
    Buffer(size_t size)
        : size(size)
    {}

public:
    kern::WaitQueue wq;

    std::atomic<uint32_t> flags = 0;

    uint64_t index = 0;
    size_t size = 0;

    mm::Page* page = nullptr;

    BufferPool* pool = nullptr;
    boost::intrusive::set_member_hook<> buffer_pool_node;

    ListNode dirty_list;

public:
    static BufferPtr New(size_t size) noexcept;
    static kern::Result<BufferPtr> ReadNoCache(BlockDevice* dev, size_t index, size_t size) noexcept;

    bool TryLock() noexcept {
        return !(flags.fetch_or(Locked, std::memory_order_acquire) & Locked);
    }

    void RawLock() noexcept {
        wq.WaitCond([&]() {
            return TryLock();
        });
    }

    void RawUnlock() noexcept {
        flags.fetch_and(~Locked, std::memory_order_release);
        wq.WakeAll();
    }

    void MarkDirty() noexcept;

    const void* Data() const noexcept {
        return (char*)page->Virt() + 1;
    }

    void* Data() noexcept {
        return (char*)page->Virt() + 1;
    }

    void WaitUpToDate() noexcept;
};

struct BufferKey {
    using type = size_t;

    size_t operator()(const Buffer& buf) const noexcept {
        return buf.index;
    }
};

class BufferPool {
public:
    SpinLock lock_;
    BlockDevice* dev_ = nullptr;
    size_t block_size_ = 0;

private:
    boost::intrusive::set<
        Buffer,
        boost::intrusive::member_hook<Buffer, boost::intrusive::set_member_hook<>, &Buffer::buffer_pool_node>,
        boost::intrusive::key_of_value<BufferKey>
    > buffers_;

private:
    BufferPtr FindBufferLocked(size_t index) noexcept;

public:
    BufferPool(BlockDevice* dev, size_t block_size)
        : dev_(dev)
        , block_size_(block_size)
    {}

    ~BufferPool();

    kern::Result<BufferPtr> ReadBuffer(size_t index) noexcept;
};

}
