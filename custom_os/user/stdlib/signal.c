#include <signal.h>
#include <unistd.h>
#include <stdlib_private/syscalls.h>
#include <sys/syscalls.h>

int raise(int sig) {
    return kill(getpid(), sig);
}

int kill(pid_t pid, int sig) {
    int res = SYSCALL2(SYS_kill, pid, sig);
    return SET_ERRNO(res);
}

sighandler_t handlers[SIG_MAX] = {};

void signal_trampoline();

sighandler_t signal(int sig, sighandler_t handler) {
    sighandler_t new_handler = signal_trampoline;
    if (handler == SIG_DFL || handler == SIG_IGN) {
        new_handler = handler;
    }

    int res = SYSCALL2(SYS_signal, sig, (uint64_t)new_handler);
    if (res < 0) {
        errno = -res;
        return SIG_ERR;
    }
    errno = 0;

    sighandler_t prev = handlers[sig];
    handlers[sig] = handler;

    return prev;
}
