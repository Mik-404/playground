#include "kernel/printk.h"
#include "kernel/sched.h"
#include "kernel/signal.h"
#include "kernel/syscall.h"
#include "kernel/panic.h"
#include "kernel/time.h"
#include "lib/common.h"
#include "lib/defer.h"


extern SyscallDef __start_syscalls_table;
extern SyscallDef __stop_syscalls_table;

extern "C" int64_t DoSyscall(uint64_t sysno) {
    sched::Task* task = sched::Current();
    BUG_ON_NULL(task);

    if (sysno >= SYS_max) {
        return -kern::ENOSYS.Code();
    }
    SyscallDef def = (&__start_syscalls_table)[sysno];
    BUG_ON(def.n != sysno);
    int64_t res = def.entry();
    kern::SignalDeliver();
    return res;
}
