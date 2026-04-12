#pragma once

#include <sys/types.h>

#define SIGABRT 6
#define SIGKILL 9
#define SIGSEGV 11
#define SIGPIPE 13
#define SIGTERM 15
#define SIG_MAX 32

int raise(int sig);
int kill(pid_t pid, int sig);

typedef void (*sighandler_t)(int);

sighandler_t signal(int sig, sighandler_t handler);

#define SIG_ERR ((sighandler_t)0xffffffffffffffff)
#define SIG_DFL ((sighandler_t)0xfffffffffffffff0)
#define SIG_IGN ((sighandler_t)0xfffffffffffffff1)

union sigval {
    int sival_int;
    void* sival_ptr;
};

typedef struct {
    int si_signo;
    int si_errno;
    int si_code;
    pid_t si_pid;
    uid_t si_uid;
    void* si_addr;
    long si_band;
    union sigval si_value;
} siginfo_t;

struct sigaction {
    void (*sa_handler)(int);
    void (*sa_sigaction)(int, siginfo_t*, void*);
    sigset_t sa_mask;
    int sa_flags;
    void (*sa_restorer)(void);
};

int sigaction(int signum, const struct sigaction* restrict act, struct sigaction* restrict oldact);

static inline int sigemptyset(sigset_t* set) {
    *set = 0;
    return 0;
}

static inline int sigfillset(sigset_t* set) {
    *set = UINT64_MAX;
    return 0;
}

static inline int sigaddset(sigset_t* set, int signum) {
    *set |= 1ull << signum;
    return 0;
}

static inline int sigdelset(sigset_t* set, int signum) {
    *set &= ~(1ull << signum);
    return 0;
}

static inline int sigismember(const sigset_t* set, int signum) {
    if (*set & (1ull << signum)) {
        return 1;
    }
    return 0;
}
