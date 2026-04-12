#include "common.h"

#include <sched.h>
#include <sys/wait.h>

TEST(fpu_basic) {
    float __attribute__((aligned(16))) a[4] = { 1, 2, 3, 4 };
    float __attribute__((aligned(16))) b[4] = { 4, 3, 2, 1 };
    float res1[4] = { 0, 0, 0, 0 };
    float res2 = 0;

    __asm__ volatile (
        "movaps %2, %%xmm0\n"
        "addps %3, %%xmm0\n"
        "movaps %%xmm0, %0\n"
        "haddps %%xmm0, %%xmm0\n"
        "haddps %%xmm0, %%xmm0\n"
        "movss %%xmm0, %1"
        : "=m"(res1), "=m"(res2)
        : "m"(a), "m"(b)
        : "xmm0"
    );

    ASSERT(res1[0] == 5);
    ASSERT(res1[1] == 5);
    ASSERT(res1[2] == 5);
    ASSERT(res1[3] == 5);
    ASSERT(res2 == 20);
}

#define load_sse_regs(n) \
{ \
    float __attribute__((aligned(16))) data[4] = { n, n + 1, n + 2, n + 3 }; \
    __asm__ volatile ( \
        "movaps %0, %%xmm0\n" \
        "movaps %0, %%xmm1\n" \
        "movaps %0, %%xmm2\n" \
        "movaps %0, %%xmm3\n" \
        "movaps %0, %%xmm4\n" \
        "movaps %0, %%xmm5\n" \
        "movaps %0, %%xmm6\n" \
        "movaps %0, %%xmm7\n" \
        : : "m"(data) : "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7" \
    ); \
}

#define check_sse_regs(n) \
{ \
    float __attribute__((aligned(16))) xmm0[4]; \
    float __attribute__((aligned(16))) xmm1[4]; \
    float __attribute__((aligned(16))) xmm2[4]; \
    float __attribute__((aligned(16))) xmm3[4]; \
    float __attribute__((aligned(16))) xmm4[4]; \
    float __attribute__((aligned(16))) xmm5[4]; \
    float __attribute__((aligned(16))) xmm6[4]; \
    float __attribute__((aligned(16))) xmm7[4]; \
    __asm__ volatile ( \
        "movaps %%xmm0, %[xmm0]\n" \
        "movaps %%xmm1, %[xmm1]\n" \
        "movaps %%xmm2, %[xmm2]\n" \
        "movaps %%xmm3, %[xmm3]\n" \
        "movaps %%xmm4, %[xmm4]\n" \
        "movaps %%xmm5, %[xmm5]\n" \
        "movaps %%xmm6, %[xmm6]\n" \
        "movaps %%xmm7, %[xmm7]\n" \
        : \
            [xmm0] "=m"(xmm0), \
            [xmm1] "=m"(xmm1), \
            [xmm2] "=m"(xmm2), \
            [xmm3] "=m"(xmm3), \
            [xmm4] "=m"(xmm4), \
            [xmm5] "=m"(xmm5), \
            [xmm6] "=m"(xmm6), \
            [xmm7] "=m"(xmm7) \
        : \
        : "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7" \
    ); \
    ASSERT(xmm0[0] == n && xmm0[1] == n + 1 && xmm0[2] == n + 2 && xmm0[3] == n + 3); \
    ASSERT(xmm1[0] == n && xmm1[1] == n + 1 && xmm1[2] == n + 2 && xmm1[3] == n + 3); \
    ASSERT(xmm2[0] == n && xmm2[1] == n + 1 && xmm2[2] == n + 2 && xmm2[3] == n + 3); \
    ASSERT(xmm3[0] == n && xmm3[1] == n + 1 && xmm3[2] == n + 2 && xmm3[3] == n + 3); \
    ASSERT(xmm4[0] == n && xmm4[1] == n + 1 && xmm4[2] == n + 2 && xmm4[3] == n + 3); \
    ASSERT(xmm5[0] == n && xmm5[1] == n + 1 && xmm5[2] == n + 2 && xmm5[3] == n + 3); \
    ASSERT(xmm6[0] == n && xmm6[1] == n + 1 && xmm6[2] == n + 2 && xmm6[3] == n + 3); \
    ASSERT(xmm7[0] == n && xmm7[1] == n + 1 && xmm7[2] == n + 2 && xmm7[3] == n + 3); \
}

static void chaos_monkey_sse(int n) {
    for (int i = 0; i < 100; i++) {
        load_sse_regs(n);
        sched_yield();
        check_sse_regs(n);
    }
}

TEST(fpu_fork) {
    const int N_CHILDREN = 100;

    load_sse_regs(123);

    for (int i = 0; i < N_CHILDREN; i++) {
        pid_t pid = ASSERT_NO_ERR(fork());
        if (pid != 0) {
            continue;
        }

        pid = getpid();
        if (i % 2) {
            chaos_monkey_sse(i);
        } else {
            for (int i = 0; i < 100; i++) {
                sched_yield();
            }
            // XMM registers should be preserved on fork.
            check_sse_regs(123);
        }
        exit(0);
    }

    for (int i = 0; i < N_CHILDREN; i++) {
        int status;
        ASSERT_NO_ERR(wait(&status));
        ASSERT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }
}
