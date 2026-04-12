#include <tuple>

#include "arch/signal.h"
#include "kernel/panic.h"
#include "kernel/signal.h"
#include "kernel/sched.h"
#include "kernel/syscall.h"

namespace kern {

namespace {

bool SignalIgnoredByDefault(Signal sig) noexcept {
    return sig == Signal::SIGCHLD;
}

Signal PickSignal(sched::Task* task) {
    for (int sig = 0; sig < (int)Signal::MAX; sig++) {
        if (task->pending_signals.TryPick(Signal(sig))) {
            return Signal(sig);
        }
    }
    return Signal::INVALID;
}

}

void SignalSend(sched::Task* target, Signal sig) {
    sched::Task* curr = sched::Current();
    BUG_ON_NULL(curr);

    if (target->blocked_signals.Has(sig)) {
        return;
    }

    target->pending_signals.Add(sig);
    if (curr != target) {
        sched::WakeTask(target);
    }
}

void SignalDeliver() {
    sched::Task* task = sched::Current();
    BUG_ON_NULL(task);

    Signal sig = PickSignal(task);
    if (sig == Signal::INVALID) {
        return;
    }

    if (sig == kern::Signal::SIGKILL) {
        sched::TaskExit(sched::TASK_EXIT_KILLED | uint32_t(sig));
    }

    SigAction sigact = WithIrqSafeLocked(task->lock, [&]() {
        return task->sigactions[sig];
    });

    if (sigact == SigAction::Ignored()) {
        return;
    }

    if (sigact == SigAction::Default()) {
        if (SignalIgnoredByDefault(sig)) {
            return;
        }
        sched::TaskExit(sched::TASK_EXIT_KILLED | uint32_t(sig));
    }

    std::ignore = arch::SignalDeliver(sig, sigact.handler);

    // arch::SignalDeliver returns only if error occurred, so we have no other choice.
    sched::TaskExit(sched::TASK_EXIT_KILLED | uint32_t(sig));
}

kern::Errno SysKill(sched::Task*, int32_t pid, int sig) noexcept {
    if (sig < 0 || sig >= (int)kern::Signal::MAX) {
        return kern::EINVAL;
    }

    if (sig == 0) {
        return {};
    }

    sched::TaskPtr task = sched::TaskByPid(pid);
    if (!task) {
        return kern::ESRCH;
    }

    kern::SignalSend(task.Get(), kern::Signal(sig));
    return kern::ENOERR;
}
REGISTER_SYSCALL(kill, SysKill);

kern::Errno SysSignal(sched::Task* task, int sig, void* handler) noexcept {
    if (sig < 0 || sig >= (int)kern::Signal::MAX) {
        return kern::EINVAL;
    }

    kern::SigAction sigact = {
        .handler = handler,
    };

    if (sigact != kern::SigAction::Default() && sigact != kern::SigAction::Ignored() && handler >= (void*)USERSPACE_ADDRESS_MAX) {
        return kern::EINVAL;
    }

    WithIrqSafeLocked(task->lock, [&]() {
        task->sigactions[kern::Signal(sig)] = sigact;
    });

    return {};
}
REGISTER_SYSCALL(signal, SysSignal);

}
