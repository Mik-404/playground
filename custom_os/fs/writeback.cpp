#include "fs/page_cache.h"
#include "kernel/kernel_thread.h"
#include "kernel/syscall.h"

namespace fs {

using namespace time::literals;

SpinLock dirty_buffers_lock;
ListHead<Buffer, &Buffer::dirty_list> dirty_buffers;
kern::WaitQueue sync_wq;

void DoWriteback(BufferPtr buf) noexcept {
    auto end_request = [buf](const BlockDevice::BaseRequest&) mutable {
        buf->flags.fetch_and(~(Buffer::Dirty | Buffer::Locked), std::memory_order_release);
        buf->wq.WakeAll();
        buf->Unref();
    };

    BlockDevice::RequestPtr req(new BlockDevice::Request(end_request));
    if (!req) {
        return;
    }

    req->sector = buf->index * (buf->size / buf->pool->dev_->SectorSize());
    req->buf.data = buf->Data();
    req->buf.size = buf->size;
    req->flags = BlockDevice::RequestFlag::Write;

    buf->RawLock();
    auto err = buf->pool->dev_->Submit(std::move(req));
    if (!err.Ok()) {
        buf->RawUnlock();
        WithIrqSafeLocked(dirty_buffers_lock, [&]() {
            dirty_buffers.InsertLast(*buf);
        });
        printk("[writeback] failed to write buffer: %e", err.Code());
    }
}

void BuffersWritebackThread(void*) noexcept {
    for (;;) {
        for (;;) {
            IrqSafeScopeLocker locker(dirty_buffers_lock);
            if (dirty_buffers.Empty()) {
                sync_wq.WakeAll();
                break;
            }
            BufferPtr buf(&dirty_buffers.First());
            buf->dirty_list.Remove();
            locker.Unlock();

            DoWriteback(buf);
        }
        time::SleepUntil(time::NowMonotonic().Add(30_ms));
    }
}

void Buffer::MarkDirty() noexcept {
    uint32_t old_flags = flags.fetch_or(Buffer::Dirty, std::memory_order_relaxed);

    // Cannot mark not-yet-read buffer as dirty.
    BUG_ON(!(old_flags & Buffer::UpToDate));

    if (old_flags & Buffer::Dirty) {
        return;
    }

    Ref();
    IrqSafeScopeLocker locker(dirty_buffers_lock);
    dirty_buffers.InsertLast(*this);
}

void BuffersWritebackStart() noexcept {
    auto err = kern::CreateKthread(BuffersWritebackThread, nullptr);
    if (!err.Ok()) {
        panic("cannot create buffers writeback thread: %e", err.Err().Code());
    }
}

kern::Errno SysSync(sched::Task*) noexcept {
    IrqSafeScopeLocker locker(dirty_buffers_lock);
    sync_wq.WaitCondLocked(locker, [&]() {
        return dirty_buffers.Empty();
    });
    return kern::ENOERR;
}
REGISTER_SYSCALL(sync, SysSync);


}
