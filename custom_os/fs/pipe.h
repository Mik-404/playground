#pragma once

#include <memory>

#include "defs.h"
#include "fs/vfs.h"
#include "kernel/error.h"
#include "lib/locking.h"
#include "lib/mutex.h"


#include "mm/obj_alloc.h"
#include "mm/page_alloc.h"

#include "kernel/panic.h"
#include "kernel/sched.h"
#include "kernel/signal.h"
#include "kernel/syscall.h"
#include "lib/shared_ptr.h"
#include "mm/new.h"


class CircularBuffer {
    constexpr static size_t Capacity = 4 * KB;
public:

    CircularBuffer () noexcept;

    ~CircularBuffer () noexcept;

    bool full () const noexcept;

    bool empty () const noexcept;

    size_t size() const noexcept;

    size_t capacity() const noexcept;

    int push_back (void* buf, size_t sz) noexcept;

    int pop_front(void* buf, size_t sz) noexcept;


private:
    char* arr_;
    mm::Page* page_;
    size_t sz_;
    size_t start_;
};


namespace vfs {

class PipeWriter;
class PipeReader;

struct PipeState : public RefCounted {
    kern::WaitQueue reader_wq_;
    kern::WaitQueue writer_wq_;
    Mutex mutex_;
    CircularBuffer buffer;
    bool reader_alive_ = true;
    bool writer_alive_ = true;

    static void operator delete(void* ptr) noexcept;
};

class PipeWriter : public File {
    IntrusiveSharedPtr<PipeState> state_;

public:
    PipeWriter(FileFlags flags, IntrusiveSharedPtr<PipeState> state) noexcept
        : File(flags),
          state_(state)
    {}

    ~PipeWriter() noexcept {
        RawScopeLocker locker (state_->mutex_);
        state_->writer_alive_ = false;
        state_->reader_wq_.WakeAll();
    }

    kern::Result<size_t> Read(mm::MemBuf buf) noexcept override;
    kern::Result<size_t> Write(mm::MemBuf buf) noexcept override;

    FilePtr Clone() noexcept override;

    static void operator delete(void* ptr) noexcept;
};

class PipeReader : public File {
    IntrusiveSharedPtr<PipeState> state_;

public:
    PipeReader(FileFlags flags, IntrusiveSharedPtr<PipeState> state) noexcept
        : File(flags),
          state_(state)
    {}

    ~PipeReader() noexcept {
        RawScopeLocker locker (state_->mutex_);
        state_->reader_alive_ = false;
        state_->writer_wq_.WakeAll();
    }
    
    kern::Result<size_t> Read(mm::MemBuf buf) noexcept override;
    kern::Result<size_t> Write(mm::MemBuf buf) noexcept override;

    FilePtr Clone() noexcept override;

    static void operator delete(void* ptr) noexcept;
};

}
