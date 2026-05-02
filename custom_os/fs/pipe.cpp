#include "fs/pipe.h"
#include "defs.h"
#include "kernel/error.h"
#include "mm/page_alloc.h"
#include "mm/vmem.h"
#include <cstring>

static mm::TypedObjectAllocator<vfs::PipeState> state_alloc;
static mm::TypedObjectAllocator<vfs::PipeReader> reader_alloc;
static mm::TypedObjectAllocator<vfs::PipeWriter> writer_alloc;

CircularBuffer::CircularBuffer () noexcept : sz_(0), start_(0) {
    page_ = mm::AllocPage(mm::PageAllocOrderBySize(Capacity), mm::AllocFlags{});
    arr_ = (char*)page_->Virt();
}

CircularBuffer::~CircularBuffer () noexcept {
    mm::FreePage(page_);
}

bool CircularBuffer::full () const noexcept {
    return sz_ == capacity();
} 

bool CircularBuffer::empty () const noexcept {
    return sz_ == 0;
} 

size_t CircularBuffer::size() const noexcept {
    return sz_;
}

size_t CircularBuffer::capacity() const noexcept {
    return Capacity;
}

int CircularBuffer::push_back (void* buf, size_t sz) noexcept {
    sz = std::min(Capacity - sz_, sz);
    size_t first_free_pos = (start_ + sz_) % Capacity;
    memcpy(arr_ + first_free_pos, buf
                            , std::min(Capacity - first_free_pos, sz));
    if (Capacity - first_free_pos < sz) {
        memcpy(arr_, ((char*)buf + (Capacity - first_free_pos))
                                , sz - (Capacity - first_free_pos));
    }
    sz_ += sz;
    return sz;
}

int CircularBuffer::pop_front(void* buf, size_t sz) noexcept {
    sz = std::min(sz_, sz);
    memcpy(buf, arr_ + start_
                        , std::min(Capacity - start_, sz));
    if (Capacity - start_ < sz) {
        memcpy(((char*)buf + (Capacity - start_)), arr_
                            , sz - (Capacity - start_));
    }
    sz_ -= sz;
    start_ = (start_ + sz) % Capacity;
    return sz;
}


namespace vfs{

void PipeState::operator delete(void* ptr) noexcept {
    state_alloc.Free((PipeState*)ptr);
}

void PipeReader::operator delete(void* ptr) noexcept {
    reader_alloc.Free((PipeReader*)ptr);
}

void PipeWriter::operator delete(void* ptr) noexcept {
    writer_alloc.Free((PipeWriter*)ptr);
}


kern::Result<size_t> PipeWriter::Read([[maybe_unused]]mm::MemBuf buf) noexcept {
    panic("shouldn't be here");
    return kern::ENOSYS;
}

kern::Result<size_t> PipeWriter::Write(mm::MemBuf buf) noexcept {
    if (!state_->reader_alive_) {
        kern::SignalSend(sched::Current(), kern::Signal::SIGPIPE);
        return kern::EPIPE;
    }
    size_t written = 0;
    mm::Page* buffer_page = mm::AllocPage(0);
    char* kern_buffer = (char*)buffer_page->Virt();
    size_t buf_offset = 0;
    kern::Result<size_t> result;

    while (written < buf.Size()) {
        size_t new_kern_buf_size = std::min(PAGE_SIZE - buf_offset, buf.Size() - (written + buf_offset));
        if (!mm::CopyFromUser(kern_buffer + buf_offset, buf.data_ + written + buf_offset, new_kern_buf_size)) {
            result = (written != 0) ? kern::Result<size_t> (written) : kern::Result<size_t> (kern::EFAULT);
            break;
        }
        RawScopeLocker locker (state_->mutex_);
        state_->writer_wq_.WaitCondLocked(locker,[&]() {
            if (!state_->buffer.full() || !state_->reader_alive_) {
                return true;
            }
            return false;
        });
        if (!state_->reader_alive_) {
            kern::SignalSend(sched::Current(), kern::Signal::SIGPIPE);
            result = (written != 0) ? kern::Result<size_t> (written) : kern::Result<size_t> (kern::EPIPE);
            break;
        }
        int local_written = state_->buffer.push_back(kern_buffer, new_kern_buf_size + buf_offset);
        written += local_written;
        state_->reader_wq_.WakeAll();
        locker.Unlock();
        if (new_kern_buf_size + buf_offset - local_written)
            memmove(kern_buffer, kern_buffer + local_written, new_kern_buf_size + buf_offset - local_written);
        buf_offset = new_kern_buf_size + buf_offset - local_written;
    }
    mm::FreePage(buffer_page);
    if (!result.Ok()) {
        if (written) return written;
        return result.Err();
    }
    return written;
}

FilePtr PipeWriter::Clone() noexcept {
    return FilePtr(this);
}


kern::Result<size_t> PipeReader::Read(mm::MemBuf buf) noexcept {
    mm::Page* buffer_page = mm::AllocPage(0);
    void* kern_buffer = buffer_page->Virt();
    
    RawScopeLocker locker (state_->mutex_);
    state_->reader_wq_.WaitCondLocked(locker,[&]() {
        if (!state_->buffer.empty() || !state_->writer_alive_) {
            return true;
        }
        return false;
    });
    if (!state_->writer_alive_ && state_->buffer.empty()) {
        locker.Unlock();
        mm::FreePage(buffer_page);
        return 0;
    }
    size_t readed = state_->buffer.pop_front(kern_buffer, std::min((size_t)PAGE_SIZE, buf.Size()));
    state_->writer_wq_.WakeAll();
    locker.Unlock();
    auto result_copy_to_user = buf.CopyFrom(kern_buffer, readed);
    mm::FreePage(buffer_page);
    if (!result_copy_to_user.Ok()) {
        return kern::EFAULT;
    }
    return readed;
}

kern::Result<size_t> PipeReader::Write([[maybe_unused]]mm::MemBuf buf) noexcept {
    panic("shouldn't be here");
    return kern::ENOSYS;
}

FilePtr PipeReader::Clone() noexcept {
    return FilePtr(this);
}

};

kern::Errno SysPipe(sched::Task* task, int* fds) noexcept {
    IntrusiveSharedPtr<vfs::PipeState> state (new (state_alloc) vfs::PipeState());
    vfs::FilePtr reader = vfs::FilePtr(new (reader_alloc) vfs::PipeReader(vfs::FileFlag::Readable, state));
    vfs::FilePtr writer = vfs::FilePtr(new (writer_alloc) vfs::PipeWriter(vfs::FileFlag::Writeable,state));
    auto fd0 = task->file_table->AssignFd(reader);
    if (!fd0.Ok()) {
        return fd0.Err();
    }
    auto fd1 = task->file_table->AssignFd(writer);
    if (!fd1.Ok()) {
        if (!task->file_table->ReleaseFd(fd0.Val()).Ok()) {
            panic("cannot release reader fd in pipe!");
        }
        return fd1.Err();
    }
    int kern_fds[2] = {fd0.Val(), fd1.Val()};
    if (!mm::CopyToUser(fds, kern_fds, sizeof(kern_fds))){
        if (!task->file_table->ReleaseFd(fd0.Val()).Ok()) {
            panic("cannot release reader fd in pipe!");
        }
        if (!task->file_table->ReleaseFd(fd1.Val()).Ok()) {
            panic("cannot release writer fd in pipe!");
        }
        return kern::EFAULT;
    }
    return kern::ENOERR;
}
REGISTER_SYSCALL(pipe, SysPipe);
