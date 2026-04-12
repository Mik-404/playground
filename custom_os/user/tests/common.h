#pragma once

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>

#define S1(x) #x
#define S2(x) S1(x)
#define LOCATION __FILE__ ":" S2(__LINE__)

#define FAIL(...) { \
    printf(__VA_ARGS__); \
    printf("\n"); \
    raise(SIGABRT); \
    exit(255); \
    *(volatile uint32_t*)0xc000000000000000 = 0xffffffff; \
}

#define ASSERT_MSG(x, ...) if (!(x)) { FAIL(__VA_ARGS__) }
#define ASSERT_MSG_ERRNO(x, msg, ...) if (!(x)) { FAIL("assertion (" #x ") failed: " msg ": errno=%d\n" __VA_OPT__(,) __VA_ARGS__, errno); }
#define ASSERT(x) ASSERT_MSG(x, LOCATION ": assertion failed: " #x "\n")

#define ASSERT_FORK_OK(pid) { \
    if ((pid) < 1) { \
        FAIL(LOCATION ": fork returned %d (errno = %d)\n", pid, errno); \
    } \
}

#define ASSERT_SIGNALED(status, sig) { \
    if (!WIFSIGNALED(status) || WTERMSIG(status) != sig) { \
        FAIL(LOCATION ": process was not signaled, status=0x%x\n", status); \
    } \
}

#define ASSERT_NO_ERR(call) ({ \
    __auto_type res = (call); \
    if (res < 0) { \
        FAIL(LOCATION ": " #call " returned %d errno=%d\n", res, errno); \
    } \
    res; \
})

#define ASSERT_ERR(call, err) ({ \
    __auto_type res = (call); \
    if (res >= 0 || errno != err) { \
        FAIL(LOCATION ": " #call " was successful or errno != " #err ": result=%d errno=%d\n", res, errno); \
    } \
    res; \
})

#define ASSERT_MMAP_FAILED(call, err) ({ \
    __auto_type res = (call); \
    if (res != MAP_FAILED || errno != err) { \
        FAIL(LOCATION ": " #call " was successful or errno != " #err ": errno=%d\n", errno); \
    } \
    res; \
})

typedef struct {
    const char* name;
    void (*entry)();
    bool run_in_child;
} __attribute__((aligned(16))) test_descr_t;

#define TEST(fn) void fn(); __attribute__((section("test_descrs"))) test_descr_t __desc_##fn = { .name = S2(fn), .entry = fn, .run_in_child = false }; void fn()
#define TEST_FORK(fn) void fn(); __attribute__((section("test_descrs"))) test_descr_t __desc_##fn = { .name = S2(fn), .entry = fn, .run_in_child = true }; void fn()
