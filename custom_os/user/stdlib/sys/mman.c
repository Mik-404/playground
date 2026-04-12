#include <sys/mman.h>
#include <sys/syscalls.h>
#include <stdio.h>
#include <stdlib_private/syscalls.h>

void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset) {
    int64_t res = SYSCALL6(SYS_mmap, addr, length, prot, flags, fd, offset);
    if (res < 0) {
        errno = -res;
        return MAP_FAILED;
    }
    errno = 0;
    return (void*)res;
}

int munmap(void* addr, size_t length) {
    int64_t res = SYSCALL2(SYS_munmap, addr, length);
    return SET_ERRNO(res);
}

int mprotect(void* addr, size_t length, int prot) {
    int64_t res = SYSCALL3(SYS_mprotect, addr, length, prot);
    return SET_ERRNO(res);
}
