#include "common.h"

#include <sched.h>
#include <stdatomic.h>
#include <sys/syscalls.h>
#include <sys/wait.h>
#include <unistd.h>


static _Atomic int test_var = 0;

// Test that pages of current executable file are not sharede accross children.
TEST(fork_unshares_exe_pages) {
    atomic_store(&test_var, 0);
    for (int i = 0; i < 100; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            atomic_store(&test_var, i);
            for (int j = 0; j < 1000; j++) {
                ASSERT(atomic_load(&test_var) == i);
                sched_yield();
            }
            exit(0);
        }
        ASSERT_FORK_OK(pid);
    }
    for (int i = 0; i < 100; i++) {
        int status = 0;
        pid_t pid = ASSERT_NO_ERR(wait(&status));
        ASSERT(pid > 1);
        ASSERT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
        ASSERT(atomic_load(&test_var) == 0);
    }
    ASSERT(atomic_load(&test_var) == 0);
}

static void bad_parent(size_t orphans, int depth) {
    if (depth > 0) {
        for (size_t i = 0; i < orphans; i++) {
            pid_t pid = ASSERT_NO_ERR(fork());
            if (pid == 0) {
                bad_parent(orphans, depth - 1);
            }
            ASSERT(pid > 1);
        }
    }
    exit(123);
}

// Test multi-level fork reparenting.
TEST(multi_level_fork) {
    ASSERT(getpid() == 1);

    const size_t N_CHILDREN = 10;
    const size_t N_LEVELS = 3;

    pid_t pid = ASSERT_NO_ERR(fork());
    if (pid == 0) {
        bad_parent(N_CHILDREN, N_LEVELS);
    }
    ASSERT_MSG(pid > 1, "pid %d", pid);

    size_t total_orphans = 0;
    size_t on_level = 1;
    for (size_t l = 0; l <= N_LEVELS; l++) {
        total_orphans += on_level;
        on_level *= N_CHILDREN;
    }

    size_t awaited = 0;
    while (awaited < total_orphans) {
        int status = 0;
        ASSERT_NO_ERR(waitpid(-1, &status, 0));
        ASSERT(WIFEXITED(status) && WEXITSTATUS(status) == 123);
        awaited++;
    }

    for (int i = 0; i < 100; i++) {
        int status = 0;
        ASSERT_ERR(waitpid(-1, &status, 0), ECHILD);
    }
}

// Test basic fork semantics.
TEST(fork_basic) {
    const size_t N_CHILDREN = 1;

    pid_t pids[N_CHILDREN];
    for (size_t i = 0; i < N_CHILDREN; i++) {
        pids[i] = ASSERT_NO_ERR(fork());
        if (pids[i] == 0) {
            volatile char mem[4096 * 3];
            for (size_t i = 0; i < 4096 * 3; i++) {
                mem[i] = i;
            }
            exit(mem[i]);
        }
    }
    for (size_t i = 0; i < N_CHILDREN; i++) {
        int status = 0;
        int pid = ASSERT_NO_ERR(waitpid(pids[i], &status, 0));
        ASSERT(pid == pids[i]);
        ASSERT(WIFEXITED(status));
        ASSERT(WEXITSTATUS(status) == i);
    }
    for (int i = 0; i < 100; i++) {
        int status = 0;
        ASSERT(wait(&status) == -1 && errno == ECHILD);
    }
}

// Test waitpid(-1) waits for any child.
TEST(wait_any) {
    const size_t N_CHILDREN = 256;

    pid_t pids[N_CHILDREN];
    int statuses[N_CHILDREN];

    for (size_t i = 0; i < N_CHILDREN; i++) {
        pids[i] = ASSERT_NO_ERR(fork());
        if (pids[i] == 0) {
            exit(i);
        }
    }
    for (size_t i = 0; i < N_CHILDREN; i++) {
        int status = 0;
        pid_t pid = ASSERT_NO_ERR(waitpid(-1, &status, 0));
        for (size_t j = 0; j < N_CHILDREN; j++) {
            if (pids[j] == pid) {
                statuses[j] = status;
            }
        }
    }

    for (size_t i = 0; i < N_CHILDREN; i++) {
        ASSERT(WIFEXITED(statuses[i]));
        ASSERT(WEXITSTATUS(statuses[i]) == i);
    }
}

TEST(wait_signal) {
    pid_t pid = ASSERT_NO_ERR(fork());
    if (pid == 0) {
        for (;;) {
            // Prevent optimizing out this loop.
            __asm__ volatile ("");
        }
        exit(0);
    }
    ASSERT(pid > 1);
    ASSERT_NO_ERR(kill(pid, SIGKILL));
    int status = 0;
    pid_t wpid = ASSERT_NO_ERR(waitpid(pid, &status, 0));
    ASSERT(wpid == pid);
    ASSERT(!WIFEXITED(status));
    ASSERT(WTERMSIG(status) == SIGKILL);
}

#if TARGET_ARCH == x86
TEST(fork_x86_regs_copied) {
    int64_t pid;
    uint64_t rbx;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;

    __asm__ volatile (
        "mov $0xdeadbeef, %%rbx\n"
        "mov $0xdeadbeef, %%r12\n"
        "mov $0xdeadbeef, %%r13\n"
        "mov $0xdeadbeef, %%r14\n"
        "mov $0xdeadbeef, %%r15\n"
        "mov $" S2(SYS_fork) ", %%rax\n"
        "syscall\n"
        "mov %%rax, %[pid]\n"
        "mov %%rbx, %[rbx]\n"
        "mov %%r12, %[r12]\n"
        "mov %%r13, %[r13]\n"
        "mov %%r14, %[r14]\n"
        "mov %%r15, %[r15]\n"
        : [pid] "=g"(pid), [rbx] "=g"(rbx), [r12] "=g"(r12), [r13] "=g"(r13), [r14] "=g"(r14), [r15] "=g"(r15)
        :
        : "rax", "r12", "r13", "r14", "r15", "rbx"
    );

    ASSERT_NO_ERR(pid);
    if (pid == 0) {
        ASSERT(rbx == 0xdeadbeef);
        ASSERT(r12 == 0xdeadbeef);
        ASSERT(r13 == 0xdeadbeef);
        ASSERT(r14 == 0xdeadbeef);
        ASSERT(r15 == 0xdeadbeef);
        exit(0);
    }

    int status;
    pid_t wpid = ASSERT_NO_ERR(waitpid(pid, &status, 0));
    ASSERT(wpid == pid);
    ASSERT_MSG(WIFEXITED(status) && WEXITSTATUS(status) == 0, "status: %x", status);
}
#endif
