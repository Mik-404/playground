#pragma once

#include <climits>

#include "lib/list.h"
#include "lib/spinlock.h"
#include "kernel/error.h"
#include "kernel/time.h"

namespace sched {
struct Task;
}

namespace kern {

class WaitQueue {
public:
    struct Waiter {
        sched::Task* task = nullptr;
        ListNode list;
    };

public:
    SpinLock lock_;
    ListHead<Waiter, &Waiter::list> waiters_head_;

public:
    // Prepare sets TASK_WAITING for current task and inserts its waiter into wait queue.
    void Prepare(Waiter& wt) noexcept;

    // Finish removes waiter from wait queue and wakes it..
    void Finish(WaitQueue::Waiter& wt) noexcept;

    // WakeAtMost wakes at most n waiters
    void WakeAtMost(unsigned n) noexcept;

    // WakeWaiter wakes specified waiter.
    void WakeWaiter(const WaitQueue::Waiter& wt) noexcept;

    // Wait is a low-level wait function.
    void Wait() noexcept;

    // WaitCond performs a non-interruptible wait on the wait queue. Wait is finished when fn returns true.
    template <typename Fn>
    void WaitCond(Fn fn) noexcept {
        for (;;) {
            WaitQueue::Waiter wt;
            Prepare(wt);
            if (fn()) {
                Finish(wt);
                break;
            }
            Wait();
            Finish(wt);
        }
    }

    // WaitCondDeadline performs a non-interruptible wait on the wait queue. Wait is finished when fn returns true or deadline exceeded.
    template <typename Fn>
    void WaitCondDeadline(Fn fn, time::Time deadline) noexcept {
        for (;;) {
            WaitQueue::Waiter wt;
            Prepare(wt);
            if (fn() || !deadline.After(time::NowMonotonic())) {
                Finish(wt);
                break;
            }
            time::Timer timer(deadline, [&]() { WakeWaiter(wt); });
            Wait();
            Finish(wt);
        }
    }

    // WaitCondLocked performs a non-interruptible wait on the wait queue. Locker is released when sleeping and locked back upon return.
    // Wait is finished when fn returns true.
    template <typename Locker,  typename Fn>
    void WaitCondLocked(Locker& locker, Fn fn) noexcept {
        for (;;) {
            WaitQueue::Waiter wt;
            Prepare(wt);
            if (fn()) {
                Finish(wt);
                break;
            }
            locker.Escape([&]() {
                Wait();
            });
            Finish(wt);
        }
    }

    // WakeOne wakes at most one waiter.
    void WakeOne() {
        WakeAtMost(1);
    }

    // WakAll wakes all waiters.
    void WakeAll() {
        WakeAtMost(UINT_MAX);
    }
};

}
