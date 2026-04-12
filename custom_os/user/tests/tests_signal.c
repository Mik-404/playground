#include "common.h"

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

int stop_sig;

void handler(int sig) {
    stop_sig = sig;
}

TEST(signal_basic) {
    for (size_t sig = 1; sig < SIG_MAX; sig++) {
        sighandler_t prev = signal(sig, handler);
        ASSERT(prev != SIG_ERR);
    }

    stop_sig = 0;

    for (int sig = 1; sig < SIG_MAX; sig++) {
        pid_t pid = ASSERT_NO_ERR(fork());
        if (pid == 0) {
            while (stop_sig == 0) {
                // Tell compiler don't optimize this loop.
                __asm__ volatile ("");
            }
            exit(stop_sig);
        }
        ASSERT(pid > 1);

        ASSERT_NO_ERR(kill(pid, sig));

        int status = 0;
        pid_t res = ASSERT_NO_ERR(waitpid(pid, &status, 0));
        ASSERT(res == pid);

        if (sig != SIGKILL) {
            ASSERT_MSG(WIFEXITED(status), "SIGKILL status: %x", status);
            ASSERT_MSG(WEXITSTATUS(status) == sig, "exit code: %d, expected: %d", WEXITSTATUS(status), sig);
        } else {
            ASSERT_MSG(WIFSIGNALED(status), "other status: %x", status);
            ASSERT_MSG(WTERMSIG(status) == SIGKILL, "stop sig: %d, expected: %d", WTERMSIG(status), sig);
        }
    }

    for (size_t sig = 1; sig < SIG_MAX; sig++) {
        sighandler_t prev = signal(sig, SIG_DFL);
        ASSERT(prev != SIG_ERR);
    }
}
