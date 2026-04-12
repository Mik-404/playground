#include "kernel/kernel_thread.h"
#include "kernel/clone.h"


namespace kern {

Errno CreatePid1(ThreadEntry entry, void* arg) noexcept {
    return DoClone(reinterpret_cast<void*>(entry), arg, CLONE_KERNEL_THREAD | CLONE_PID1).ToErrno();
}

Result<sched::TaskPtr> CreateKthread(ThreadEntry entry, void* arg) noexcept {
    return DoClone(reinterpret_cast<void*>(entry), arg, CLONE_KERNEL_THREAD);
}

}
