#pragma once

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

int open(const char* path, int flags, ...);
ssize_t write(int fd, const void* buf, size_t count);
ssize_t read(int fd, void* buf, size_t count);
int close(int fd);
pid_t fork();
int execve(const char* pathname, char* const argv[], char* const envp[]);
void exit(int status);
void _exit(int status);
pid_t getpid();
pid_t getppid();
int pipe(int pipefd[2]);
void sync();
unsigned int sleep(unsigned int seconds);
