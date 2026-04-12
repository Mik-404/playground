#include <sched.h>
#include <sys/syscalls.h>
#include <stdlib_private/syscalls.h>

void sched_yield() {
    SYSCALL0(SYS_sched_yield);
}
