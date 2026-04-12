#pragma once

#define VSYSCALL_DATA __attribute__((section(".vsyscall.data")))
#define VSYSCALL __attribute__((section(".vsyscall")))

