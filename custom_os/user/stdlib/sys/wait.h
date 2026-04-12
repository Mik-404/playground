#pragma once

#include <sys/types.h>
#include <sys/resource.h>

pid_t wait(int* wstatus);
pid_t waitpid(pid_t pid, int* wstatus, int options);

pid_t wait4(pid_t pid, int* wstatus, int options, struct rusage* rusage);

#define WIFSIGNALED(status) ((status) & (1 << 31))
#define WTERMSIG(status) ((status) & 0xff)
#define WIFEXITED(status) (!WIFSIGNALED(status))
#define WEXITSTATUS(status) ((status) & 0xff)
