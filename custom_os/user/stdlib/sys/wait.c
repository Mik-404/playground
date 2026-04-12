#include <sys/wait.h>
#include <sys/syscalls.h>
#include <stdlib_private/syscalls.h>


pid_t wait4(pid_t pid, int* status, int options, struct rusage* rusage) {
    int64_t res = SYSCALL4(SYS_wait4, pid, status, options, rusage);
    return SET_ERRNO(res);
}

pid_t waitpid(pid_t pid, int* status, int options) {
    return wait4(pid, status, options, NULL);
}

pid_t wait(int* wstatus) {
    return waitpid(-1, wstatus, 0);
}