#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

#include "common.h"

static void check_mapping(unsigned char* addr) {
    for (size_t i = 0; i < 26; i++) {
        for (size_t j = 0; j < 5000; j++) {
            ASSERT(addr[i * 5000 + j] == 64 + i);
        }
    }
}

TEST_FORK(mmap_einval) {
    // Cannot map first page.
    ASSERT_MMAP_FAILED(mmap((void*)0x0, 4096, PROT_READ, MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0), EINVAL);

    // Invalid file descriptor.
    ASSERT_MMAP_FAILED(mmap((void*)0x10000000, 4096, PROT_READ, MAP_FIXED | MAP_PRIVATE, -1, 0), EBADF);

    // File is opened only for reading, but mapping is for read/write.
    int fd = open("/bin/run_tests", O_RDONLY);
    ASSERT_MMAP_FAILED(mmap((void*)0x10000000, 4096, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_PRIVATE, fd, 0), EINVAL);
    close(fd);
}

TEST_FORK(mmap_file_read_only) {
    int fd = ASSERT_NO_ERR(open("/etc/gentestdata/big_letters.txt", O_RDONLY));
    unsigned char* addr = mmap((void*)0x100000000, 5000 * 26, PROT_READ, MAP_FIXED | MAP_PRIVATE, fd, 0);
    ASSERT_MSG_ERRNO(addr != MAP_FAILED, "mmap failed");
    check_mapping(addr);
}

TEST(mmap_lazy) {
    pid_t pid = ASSERT_NO_ERR(fork());
    // Fork child, create a large mmap region and consume as lot pages as we can. Child should be killed by SIGKILL.
    if (pid == 0) {
        volatile char* addr = mmap((void*)0x100000000, 4 * (1ull << 40), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_FIXED, -1, 0);
        ASSERT_MSG_ERRNO(addr != MAP_FAILED, "mmap failed");

        for (size_t page = 0;; page++) {
            addr[page * 4096] = 'a';
            ASSERT(addr[page * 4096] == 'a');
        }
        exit(0);
    }
    ASSERT(pid > 1);

    int status = 0;
    ASSERT_NO_ERR(waitpid(-1, &status, 0));
    ASSERT_SIGNALED(status, SIGKILL);

    // Check that a few pages are available in a big mapping.
    pid = ASSERT_NO_ERR(fork());
    if (pid == 0) {
        volatile char* addr = mmap((void*)0x100000000, 4 * (1ull << 40), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_FIXED, -1, 0);
        ASSERT_MSG_ERRNO(addr != MAP_FAILED, "mmap failed");

        for (size_t page = 0; page < 1000; page++) {
            addr[page * 4096] = 'a';
        }
        for (size_t page = 0; page < 1000; page++) {
            ASSERT(addr[page * 4096] == 'a');
        }
        exit(0);
    }

    status = 0;
    pid_t res = ASSERT_NO_ERR(waitpid(pid, &status, 0));
    ASSERT(res == pid);
    ASSERT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

TEST_FORK(many_mmaps) {
    void* const BASE_ADDR = (void*)0x100000000;
    const size_t N_MMAPS = 10000;

    for (size_t i = 0; i < N_MMAPS; i++) {
        volatile char* addr = mmap(BASE_ADDR + 2 * i * 4096, 4096, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_FIXED, -1, 0);
        ASSERT_MSG_ERRNO(addr != MAP_FAILED, "mmap failed");
    }

    volatile char* buf = BASE_ADDR;
    for (size_t i = N_MMAPS - 1; i > 0; i--) {
        buf[2 * i * 4096] = 'a';
        ASSERT(buf[2 * i * 4096] == 'a');
    }
}
