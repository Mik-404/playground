#pragma once

#include <cstdint>

#include "kernel/error.h"
#include "kernel/sched.h"

namespace kern {

typedef void (*ThreadEntry)(void* arg) noexcept;

Errno CreatePid1(ThreadEntry entry, void* arg) noexcept;

Result<sched::TaskPtr> CreateKthread(ThreadEntry entry, void* arg) noexcept;

inline Result<void*> WaitKthread(sched::Task* task) noexcept {
    return sched::WaitKthread(task);
}

}
