#include <climits>

#include "kernel/wait.h"
#include "kernel/sched.h"
#include "kernel/panic.h"
#include "lib/stack_unwind.h"

namespace kern {

void WaitQueue::Prepare(WaitQueue::Waiter& wt) noexcept {
    wt.task = sched::Current();

    IrqSafeScopeLocker locker(lock_);
    waiters_head_.InsertLast(wt);
    wt.task->state = sched::TASK_WAITING;
}

void WaitQueue::Wait() noexcept {
    sched::Yield();
}

void WaitQueue::Finish(WaitQueue::Waiter& wt) noexcept {
    sched::Task* task = wt.task;
    BUG_ON(task != sched::Current());

    IrqSafeScopeLocker locker(lock_);
    wt.list.Remove();

    // Don't call sched::WakeTask, because we don't want put task on scheduler's queue.
    wt.task->state = sched::TASK_RUNNABLE;
}

void WaitQueue::WakeWaiter(const WaitQueue::Waiter& wt) noexcept {
    sched::WakeTask(wt.task);
}

void WaitQueue::WakeAtMost(unsigned n) noexcept {
    IrqSafeScopeLocker locker(lock_);
    for (const Waiter& wt : waiters_head_) {
        WakeWaiter(wt);
        if (--n == 0) {
            break;
        }
    }
}

}
