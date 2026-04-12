#include "common.h"

#include <stdbool.h>
#include <sys/wait.h>

TEST(pipe_basic) {
    ASSERT_ERR(pipe((void*)0x0), EFAULT);

    int fds[2];
    int err = pipe(fds);
    ASSERT(err == 0);
    uint8_t buf[4096];
    for (size_t i = 1; i < 1000; i++) {
        for (size_t pos = 0; pos < sizeof(buf); pos++) {
            buf[pos] = (i + pos) & 0xff;
        }
        int res = ASSERT_NO_ERR(write(fds[1], buf, sizeof(buf)));
        ASSERT(res == 4096);
        res = ASSERT_NO_ERR(read(fds[0], buf, sizeof(buf)));
        ASSERT(res == 4096);
        for (size_t pos = 0; pos < sizeof(buf); pos++) {
            ASSERT(buf[pos] == ((i + pos) & 0xff));
        }
    }

    ASSERT_NO_ERR(close(fds[1]));
    for (int i = 0; i < 100; i++) {
        char ch;
        int res = ASSERT_NO_ERR(read(fds[0], &ch, 1));
        ASSERT(res == 0);
    }
    ASSERT_NO_ERR(close(fds[0]));
}

TEST(pipe_write_no_readers) {
    int fds[2];
    ASSERT_NO_ERR(pipe(fds));

    ASSERT_NO_ERR(close(fds[0]));

    uint8_t ch = 'a';

    pid_t pid = ASSERT_NO_ERR(fork());
    if (pid == 0) {
        signal(SIGPIPE, SIG_DFL);
        write(fds[1], &ch, 1);
        exit(0);
    }

    int status;
    ASSERT_NO_ERR(waitpid(pid, &status, 0));
    ASSERT_MSG(WIFSIGNALED(status) && WTERMSIG(status) == SIGPIPE, "child process should be terminated with SIGPIPE, but status is %x", status);

    ASSERT(signal(SIGPIPE, SIG_IGN) != SIG_ERR);
    ASSERT_ERR(write(fds[1], &ch, 1), EPIPE);

    ASSERT_NO_ERR(close(fds[1]));
}

TEST(pipe_drain) {
    const char str[] = "test pipe drain";
    const int sz = sizeof(str) - 1;

    int fds[2];
    ASSERT_NO_ERR(pipe(fds));

    int res = ASSERT_NO_ERR(write(fds[1], str, sz));
    ASSERT(res == sz);

    ASSERT_NO_ERR(close(fds[1]));

    char buf[sz];
    res = ASSERT_NO_ERR(read(fds[0], &buf, sz));
    ASSERT(res == sz);

    res = ASSERT_NO_ERR(read(fds[0], &buf, sz));
    ASSERT(res == 0);
}

TEST(pipe_single_writer_multiple_readers) {
    const size_t N_CHILDREN = 100;

    int fds[2];
    ASSERT_NO_ERR(pipe(fds));

    unsigned char buf[4096];
    for (size_t i = 0; i < N_CHILDREN; i++) {
        pid_t pid = ASSERT_NO_ERR(fork());
        if (pid == 0) {
            close(fds[1]);
            int res = ASSERT_NO_ERR(read(fds[0], buf, 4096));
            ASSERT(res == 0 || res == 4096);
            if (res == 4096) {
                for (size_t i = 0; i < 4096; i++) {
                    ASSERT(buf[i] == (i & 0xff));
                }
            }
            exit(0);
        }
        ASSERT(pid > 1);
    }

    close(fds[0]);

    for (size_t i = 0; i < 4096; i++) {
        buf[i] = i & 0xff;
    }
    for (size_t i = 0; i < N_CHILDREN; i++) {
        int res = ASSERT_NO_ERR(write(fds[1], buf, 4096));
        ASSERT(res == 4096);
    }
    close(fds[1]);

    for (size_t i = 0; i < N_CHILDREN; i++) {
        int status = 0;
        ASSERT_NO_ERR(wait(&status));
        ASSERT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }
}

TEST(pipe_multiple_writers_single_reader) {
    const size_t N_CHILDREN = 200;
    const size_t N_TRIES = 100;

    int fds[2];
    ASSERT_NO_ERR(pipe(fds));

    pid_t pids[N_CHILDREN];

    // Start read children. Each child writes its own pid in pipe N_TRIES times.
    for (size_t i = 0; i < N_CHILDREN; i++) {
        pids[i] = fork();
        if (pids[i] == 0) {
            pid_t pid = getpid();
            close(fds[0]);
            for (size_t try = 0; try < N_TRIES; try++) {
                int res = ASSERT_NO_ERR(write(fds[1], &pid, sizeof(pid_t)));
                ASSERT_MSG(res == sizeof(pid_t), "pipe write is not atomic, res = %d", res);
            }
            exit(0);
        }
        ASSERT(pids[i] > 1);
    }

    // Cleanup write-side of pipe. We don't need this in parent.
    ASSERT_NO_ERR(close(fds[1]));

    int counts[N_CHILDREN];
    for (size_t i = 0; i < N_CHILDREN; i++) {
        counts[i] = 0;
    }

    // Start read from children.
    for (size_t i = 0; i < N_CHILDREN * N_TRIES; i++) {
        pid_t pid = 0;
        int res = ASSERT_NO_ERR(read(fds[0], &pid, sizeof(pid_t)));
        ASSERT_MSG(res == sizeof(pid_t), "read incorrect: res is %d, i = %d", res, i);
        bool found = false;
        for (size_t j = 0; j < N_CHILDREN; j++) {
            if (pids[j] == pid) {
                counts[j]++;
                found = true;
                break;
            }
        }
        ASSERT_MSG(found, "no child with pid %d", pid);
    }

    printf("reads done\n");

    // All children wrote N_CHILDREN * N_TRIES samples, so all of them must exit at this point.
    for (size_t i = 0; i < N_CHILDREN; i++) {
        int status = 0;
        ASSERT_NO_ERR(wait(&status));
        ASSERT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }

    // Ensure we've receive correct number of copies of each pid.
    for (size_t i = 0; i < N_CHILDREN; i++) {
        ASSERT(counts[i] == N_TRIES);
    }

    // Check no more data left in pipe.
    char ch;
    int res = ASSERT_NO_ERR(read(fds[0], &ch, 1));
    ASSERT(res == 0);

    // Cleanup read-side of pipe.
    ASSERT_NO_ERR(close(fds[0]));
}

TEST(pipe_multiple_writers_multiple_reader) {
    const size_t N_CHILDREN = 100;
    const size_t N_TRIES = 100;
    const size_t BUF_SIZE = 4096;

    int fds[2];
    int err = pipe(fds);
    ASSERT(err >= 0);

    // Start read children.
    for (size_t i = 0; i < N_CHILDREN; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            close(fds[1]);
            for (size_t try = 0; try < N_TRIES; try++) {
                unsigned char buf[BUF_SIZE];

                int res = ASSERT_NO_ERR(read(fds[0], buf, BUF_SIZE));
                // Writes less or equal than page size must be atomic.
                ASSERT_MSG(res == 0 || res == BUF_SIZE, "read %d bytes (non-atomic read), should read 0 or BUF_SIZE bytes (%d)", BUF_SIZE);
                if (res == BUF_SIZE) {
                    for (size_t i = 0; i < BUF_SIZE; i++) {
                        ASSERT(buf[i] == (i & 0xff));
                    }
                }
            }
            exit(0);
        }
        ASSERT(pid > 1);
    }

    // Start write children.
    for (size_t i = 0; i < N_CHILDREN; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            close(fds[0]);
            unsigned char buf[BUF_SIZE];
            for (size_t i = 0; i < BUF_SIZE; i++) {
                buf[i] = i & 0xff;
            }
            for (size_t try = 0; try < N_TRIES; try++) {
                int res = ASSERT_NO_ERR(write(fds[1], buf, BUF_SIZE));
                ASSERT_MSG(res == BUF_SIZE, "wrote %d bytes (non-atomic write), should be exactly BUF_SIZE bytes (%d)", res, BUF_SIZE);
            }
            exit(0);
        }
        ASSERT(pid > 1);
    }

    close(fds[1]);

    for (size_t i = 0; i < 2 * N_CHILDREN; i++) {
        int status = 0;
        ASSERT_NO_ERR(wait(&status));
        ASSERT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }

    printf("all children have finished\n");

    char ch;
    int res = ASSERT_NO_ERR(read(fds[0], &ch, 1));
    ASSERT_MSG(res == 0, "no EOF received on pipe");

    ASSERT_NO_ERR(close(fds[0]));
}
