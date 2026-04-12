#include "lib/mutex.h"
#include "kernel/sched.h"
#include "kernel/panic.h"

constexpr size_t MAX_TRIES_UNTIL_SLEEP = 100;

void Mutex::RawLock() noexcept {
    for (size_t i = 0; i < MAX_TRIES_UNTIL_SLEEP; i++) {
        if (lock_.RawTryLock()) {
            owner_ = sched::Current();
            return;
        }
    }

    wq_.WaitCond([&]() {
        if (lock_.RawTryLock()) {
            owner_ = sched::Current();
            return true;
        }
        return false;
    });
}

void Mutex::RawUnlock() noexcept {
    AssertHeld();
    owner_ = nullptr;
    lock_.RawUnlock();
    wq_.WakeAll();
}

void Mutex::AssertHeld() {
    if (!owner_) {
        panic("mutex is not held by PID %d (nobody holds it)", sched::Current()->pid);
    } else if (owner_ != sched::Current()) {
        panic("mutex is held by PID %d, not by current task (PID %d)", owner_->pid, sched::Current()->pid);
    }
}
