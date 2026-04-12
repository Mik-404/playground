#include "common.h"

#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>


static void do_read_file() {
    ASSERT_ERR(open("/non-existing-file", O_RDONLY), ENOENT);

    ASSERT_ERR(open("/bin/run_tests/test.txt", O_RDONLY), ENOTDIR);

    int fd = ASSERT_NO_ERR(open("/etc/testdata/test.txt", O_RDONLY));
    ASSERT(fd == 3);

    char buf[4096];
    int res = ASSERT_NO_ERR(read(fd, buf, sizeof(buf)));
    const char expected[] = "this is a text from an existing file!\n";
    ASSERT(res == sizeof(expected) - 1);
    ASSERT(strncmp(buf, expected, sizeof(expected) - 1) == 0);

    for (int i = 0; i < 1000; i++) {
        res = ASSERT_NO_ERR(read(fd, buf, sizeof(buf)));
        ASSERT(res == 0);
    }

    ASSERT_NO_ERR(close(fd));

    for (int i = 3; i < 200; i++) {
        ASSERT_ERR(close(i), EBADF);
    }

    for (int i = 0; i < 200; i++) {
        ASSERT_ERR(read(fd, buf, sizeof(buf)), EBADF);
    }

    ASSERT_ERR(read(-1, NULL, 1000), EBADF);
    ASSERT_ERR(read(0xffffffff, NULL, 1000), EBADF);
}

TEST(read_file) {
    do_read_file();
}

TEST(read_file_parallel) {
    const int N_CHILDREN = 100;

    for (int i = 0; i < N_CHILDREN; i++) {
        pid_t pid = ASSERT_NO_ERR(fork());
        if (pid == 0) {
            do_read_file();
            exit(0);
        }
        ASSERT(pid > 1);
    }
    for (int i = 0; i < N_CHILDREN; i++) {
        int status = 0;
        pid_t pid = ASSERT_NO_ERR(wait(&status));
        ASSERT(pid > 1);
        ASSERT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }
}

TEST(read_from_parent_file) {
    const int N_CHILDREN = 2;
    int fd = ASSERT_NO_ERR(open("/etc/gentestdata/big_letters.txt", O_RDONLY));

    for (int i = 0; i < N_CHILDREN; i++) {
        pid_t pid = ASSERT_NO_ERR(fork());
        if (pid == 0) {
            char buf[5000];
            for (int j = 0; j < 26; j++) {
                ssize_t count = ASSERT_NO_ERR(read(fd, buf, 5000));
                ASSERT(count == 5000);
                for (int k = 0; k < 5000; k++) {
                    ASSERT_MSG(buf[k] == 64 + j, "expected '%c' got '%c' at index %lu %d %d", 64 + j, buf[k], k, j, getpid());
                }
            }
            exit(0);
        }
        ASSERT(pid > 1);
    }
    close(fd);
    for (int i = 0; i < N_CHILDREN; i++) {
        int status = 0;
        pid_t pid = ASSERT_NO_ERR(wait(&status));
        ASSERT(pid > 1);
        ASSERT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }
}
