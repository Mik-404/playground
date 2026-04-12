#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <unistd.h>
#include <memory.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <string.h>

#include "common.h"


extern test_descr_t __start_test_descrs;
extern test_descr_t __stop_test_descrs;

void run_in_child(test_descr_t* test) {
    pid_t pid = ASSERT_NO_ERR(fork());
    if (pid == 0) {
        test->entry();
        exit(0);
    }
    int status = 0;
    pid_t res = ASSERT_NO_ERR(waitpid(pid, &status, 0));
    ASSERT(res == pid);
    ASSERT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

TEST(dummy) {}

int main() {
    ASSERT(getpid() == 1);
    printf("Running tests...\n");

    for (test_descr_t* test = &__start_test_descrs; test < &__stop_test_descrs; test++) {
        // if (strncmp(test->name, "read_from_parent_file", 21) != 0) {
        //     continue;
        // }
        printf("==== [ %s ] ====\n", test->name);
        if (test->run_in_child) {
            run_in_child(test);
        } else {
            test->entry();
        }
        sync();
        printf("==== [ OK: %s ] ====\n", test->name);
    }

    printf("All tests passed!\n");
    return 61;
}
