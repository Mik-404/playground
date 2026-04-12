#include "fs/pipe.h"
#include "kernel/syscall.h"

kern::Errno SysPipe(sched::Task* /* task */, int* /* fds */) noexcept {
    return kern::ENOSYS;
}
REGISTER_SYSCALL(pipe, SysPipe);
