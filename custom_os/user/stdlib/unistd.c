#include <unistd.h>
#include <stdlib_private/syscalls.h>
#include <sys/syscalls.h>

int open(const char* path, int flags, ...) {
    int res = SYSCALL2(SYS_open, path, flags);
    return SET_ERRNO(res);
}

ssize_t write(int fd, const void* buf, size_t count) {
    ssize_t res = SYSCALL3(SYS_write, fd, buf, count);
    return SET_ERRNO(res);
}

ssize_t read(int fd, void* buf, size_t count) {
    ssize_t res = SYSCALL3(SYS_read, fd, buf, count);
    return SET_ERRNO(res);
}

int close(int fd) {
    int res = SYSCALL1(SYS_close, fd);
    return SET_ERRNO(res);
}

pid_t fork() {
    pid_t res = SYSCALL0(SYS_fork);
    return SET_ERRNO(res);
}

int execve(const char* pathname, char* const argv[], char* const envp[]) {
    (void)argv;
    (void)envp;
    int res = SYSCALL1(SYS_execve, pathname);
    return SET_ERRNO(res);
}

void _exit(int status) {
    SYSCALL1(SYS_exit, status);
}

pid_t getpid() {
    return SYSCALL0(SYS_getpid);
}

pid_t getppid() {
    return SYSCALL0(SYS_getppid);
}

int pipe(int pipefd[2]) {
    int res = SYSCALL1(SYS_pipe, pipefd);
    return SET_ERRNO(res);
}

void sync() {
    SYSCALL0(SYS_sync);
}

unsigned int sleep(unsigned int seconds) {
    return SYSCALL1(SYS_sleep, seconds);
}
