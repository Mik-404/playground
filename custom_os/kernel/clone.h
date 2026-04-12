#pragma once

#include <cstdint>

#include "kernel/error.h"
#include "kernel/sched.h"

#define CLONE_KERNEL_THREAD (1 << 0)
#define CLONE_PID1          (1 << 1)

namespace kern {

kern::Result<sched::TaskPtr> DoClone(void* ip, void* sp, int flags) noexcept;

}
