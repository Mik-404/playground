#include "common.h"

#include <sys/wait.h>
#include <sched.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>

int timeval_compare(struct timeval a, struct timeval b) {
    if (a.tv_sec < b.tv_sec) {
        return -1;
    }
    if (a.tv_sec > b.tv_sec) {
        return 1;
    }
    if (a.tv_usec < b.tv_usec) {
        return -1;
    }
    if (a.tv_usec > b.tv_usec) {
        return 1;
    }
    return 0;
}

struct timeval timeval_add(struct timeval a, struct timeval b) {
    struct timeval res;
    res.tv_sec = a.tv_sec + b.tv_sec;
    res.tv_usec = a.tv_usec + b.tv_usec;
    if (res.tv_usec >= 1000000) {
        ++res.tv_sec;
        res.tv_usec -= 1000000;
    }
    return res;
}

struct timeval timeval_sub(struct timeval a, struct timeval b) {
    struct timeval res;
    res.tv_sec = a.tv_sec - b.tv_sec;
    res.tv_usec = a.tv_usec - b.tv_usec;
    if (res.tv_usec < 0) {
        --res.tv_sec;
        res.tv_usec += 1000000;
    }
    return res;
}

TEST(wait4_rusage_work) {
    pid_t pid = ASSERT_NO_ERR(fork());

    if (pid == 0) {
        volatile unsigned long sum = 0;
        for (unsigned long i = 0; i < 10000000UL; i++) {
            sum += i * i + 4;
        }
        _exit(42);
    } else {
        int status;
        struct rusage ru;
        pid_t ret = wait4(pid, &status, 0, &ru);
        ASSERT(ret == pid);
        ASSERT(WIFEXITED(status));
        ASSERT(WEXITSTATUS(status) == 42);

        printf("User time: %ld sec %ld usec, system time: %ld sec %ld usec\n", ru.ru_utime.tv_sec, ru.ru_utime.tv_usec, ru.ru_stime.tv_sec, ru.ru_stime.tv_usec);
        ASSERT(ru.ru_utime.tv_sec > 0 || ru.ru_utime.tv_usec > 0);
        ASSERT(ru.ru_stime.tv_sec == 0);
        ASSERT(timeval_compare(ru.ru_utime, ru.ru_stime) > 0);
    }
}



TEST(wait4_rusage_syscalls) {
    pid_t pid = ASSERT_NO_ERR(fork());
    if (pid == 0) {
        for (int i = 0; i < 100000; i++) {
            if (write(1, "a", 1) < 0) {
                _exit(1);
            }
        }
        write(1, "\n", 1);
        _exit(43);
    } else {
        int status;
        struct rusage ru;
        pid_t ret = wait4(pid, &status, 0, &ru);
        ASSERT(ret == pid);
        ASSERT(WIFEXITED(status));
        ASSERT(WEXITSTATUS(status) == 43);

        printf("User time: %ld sec %ld usec, system time: %ld sec %ld usec\n", ru.ru_utime.tv_sec, ru.ru_utime.tv_usec, ru.ru_stime.tv_sec, ru.ru_stime.tv_usec);
        ASSERT(ru.ru_utime.tv_sec > 0 || ru.ru_utime.tv_usec > 0);
        ASSERT(ru.ru_stime.tv_usec > 0 || ru.ru_stime.tv_sec > 0);
        ASSERT(timeval_compare(ru.ru_utime, ru.ru_stime) < 0);
    }
}

TEST(wait4_rusage_sleep) {
    pid_t pid = ASSERT_NO_ERR(fork());
    if (pid == 0) {
        sleep(3);
        _exit(44);
    } else {
        int status;
        struct rusage ru;
        pid_t ret = wait4(pid, &status, 0, &ru);
        ASSERT(ret == pid);
        ASSERT(WIFEXITED(status));
        ASSERT(WEXITSTATUS(status) == 44);

        printf("User time: %ld sec %ld usec, system time: %ld sec %ld usec\n", ru.ru_utime.tv_sec, ru.ru_utime.tv_usec, ru.ru_stime.tv_sec, ru.ru_stime.tv_usec);
        ASSERT(ru.ru_stime.tv_sec == 0);
        ASSERT(ru.ru_utime.tv_sec == 0);
    }
}

TEST(wait4_rusage_yield) {
    pid_t pid = ASSERT_NO_ERR(fork());
    if (pid == 0) {
        for (int i = 0; i < 100000; i++) {
            sched_yield();
        }
        _exit(45);
    } else {
        int status;
        struct rusage ru;
        pid_t ret = wait4(pid, &status, 0, &ru);
        ASSERT(ret == pid);
        ASSERT(WIFEXITED(status));
        ASSERT(WEXITSTATUS(status) == 45);

        printf("User time: %ld sec %ld usec, system time: %ld sec %ld usec\n", ru.ru_utime.tv_sec, ru.ru_utime.tv_usec, ru.ru_stime.tv_sec, ru.ru_stime.tv_usec);
        ASSERT(ru.ru_utime.tv_usec + ru.ru_utime.tv_sec > 0);
        ASSERT(ru.ru_stime.tv_sec + ru.ru_stime.tv_usec > 0);
    }
}

TEST(wait4_rusage_sleep_recursive) {
    pid_t pid = ASSERT_NO_ERR(fork());
    if (pid == 0) {
        pid_t pid = ASSERT_NO_ERR(fork());
        if (pid == 0) {
            sleep(3);
            _exit(44);
        } else {
            sleep(3);
            int status;
            struct rusage ru;
            pid_t ret = wait4(pid, &status, 0, &ru);
            ASSERT(ret == pid);
            ASSERT(WIFEXITED(status));
            ASSERT(WEXITSTATUS(status) == 44);

            printf("User time: %ld sec %ld usec, system time: %ld sec %ld usec\n", ru.ru_utime.tv_sec, ru.ru_utime.tv_usec, ru.ru_stime.tv_sec, ru.ru_stime.tv_usec);
            ASSERT(ru.ru_utime.tv_usec + ru.ru_stime.tv_usec > 0);
            ASSERT(ru.ru_stime.tv_sec == 0);
            ASSERT(ru.ru_utime.tv_sec == 0);

            _exit(41);
        }
    } else {
        int status;
        struct rusage ru;
        pid_t ret = wait4(pid, &status, 0, &ru);
        ASSERT(ret == pid);
        ASSERT(WIFEXITED(status));
        ASSERT(WEXITSTATUS(status) == 41);

        printf("User time: %ld sec %ld usec, system time: %ld sec %ld usec\n", ru.ru_utime.tv_sec, ru.ru_utime.tv_usec, ru.ru_stime.tv_sec, ru.ru_stime.tv_usec);
        ASSERT(ru.ru_utime.tv_usec + ru.ru_stime.tv_usec > 0);
        ASSERT(ru.ru_stime.tv_sec == 0);
        ASSERT(ru.ru_utime.tv_sec == 0);
    }
}
