#include <atomic>
#include <memory>

#include "fs/buffer.h"
#include "kernel/clone.h"
#include "kernel/panic.h"
#include "kernel/syscall.h"
#include "kernel/time.h"
#include "kernel/wait.h"
#include "lib/locking.h"
#include "lib/spinlock.h"

#include "mm/kmalloc.h"
#include "mm/new.h"

namespace fs {

namespace {
mm::TypedObjectAllocator<Buffer> buffer_alloc;

}

BufferPool::~BufferPool() {
    for (auto it = buffers_.begin(); it != buffers_.end();) {
        auto next = ++it;
        it->Unref();
        it = next;
    }
}

BufferPtr Buffer::New(size_t size) noexcept {
    BufferPtr buf(new (buffer_alloc) Buffer(size));
    if (!buf) {
        return nullptr;
    }

    buf->page = mm::AllocPage(DIV_ROUNDUP(size, PAGE_SIZE));
    if (buf->page == nullptr) {
        return nullptr;
    }

    buf->flags = 0;
    buf->size = size;

    return buf;
}

void Buffer::WaitUpToDate() noexcept {
    wq.WaitCond([&]() {
        return flags.load(std::memory_order_acquire) & Buffer::UpToDate;
    });
}

static kern::Errno ReadAsync(BufferPtr buf, BlockDevice* dev) {
    auto end_read = [buf](const BlockDevice::BaseRequest&) mutable {
        buf->flags.fetch_or(Buffer::UpToDate, std::memory_order_release);
        buf->wq.WakeAll();
    };

    BlockDevice::RequestPtr req(new BlockDevice::Request(std::move(end_read)));
    if (!req) {
        return kern::ENOMEM;
    }

    req->sector = buf->index * (buf->size / dev->SectorSize());
    req->buf.data = buf->Data();
    req->buf.size = buf->size;

    auto err = dev->Submit(std::move(req));
    if (!err.Ok()) {
        return err;
    }

    return kern::ENOERR;
}

kern::Result<BufferPtr> Buffer::ReadNoCache(BlockDevice* dev, size_t index, size_t size) noexcept {
    auto buf = Buffer::New(size);
    if (!buf) {
        return kern::ENOMEM;
    }
    buf->index = index;
    if (auto err = ReadAsync(buf, dev); !err.Ok()) {
        return err;
    }

    buf->WaitUpToDate();

    return buf;
}

BufferPtr BufferPool::FindBufferLocked(size_t index) noexcept {
    auto it = buffers_.find(index);
    if (it == buffers_.end()) {
        return nullptr;
    }
    return BufferPtr(&*it);
}

kern::Result<BufferPtr> BufferPool::ReadBuffer(size_t index) noexcept {
    BufferPtr buf = WithIrqSafeLocked(lock_, [&]() {
        return FindBufferLocked(index);
    });

    if (buf) {
        buf->WaitUpToDate();
        return buf;
    }

    auto new_buf = Buffer::New(block_size_);
    if (!new_buf) {
        return kern::ENOMEM;
    }
    new_buf->pool = this;
    new_buf->flags |= Buffer::InPool;
    new_buf->index = index;

    WithIrqSafeLocked(lock_, [&]() {
        buf = FindBufferLocked(index);
        if (!buf) {
            new_buf->Ref();
            buffers_.insert(*new_buf.Get());
        } else {
            buf->Ref();
        }
    });

    if (buf) {
        // Someone was ahead of us and already inserted buffer into pool with same index.
        buf->WaitUpToDate();
        return buf;
    }

    if (auto err = ReadAsync(new_buf, dev_); !err.Ok()) {
        new_buf->Unref();
        return err;
    }

    new_buf->WaitUpToDate();
    return new_buf;
}

}
