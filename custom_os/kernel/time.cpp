#include "arch/time.h"
#include "kernel/irq.h"
#include "kernel/time.h"
#include "kernel/error.h"
#include "kernel/per_cpu.h"
#include "kernel/sched.h"
#include "kernel/syscall.h"
#include "lib/seqlock.h"

struct HrClock {
    sync::SeqLock seqlock;
    time_t wall_time_start_secs;
    uint64_t clock_last_ns;
    uint64_t clock_total_ns;
};

static HrClock hr_clock;

namespace time {

static void Sync() {
    kern::WithoutIrqs([]() {
        hr_clock.seqlock.Write([]() {
            hr_clock.wall_time_start_secs = arch::GetWallTimeSecs();
            uint64_t curr_ns = arch::GetClockNs();
            hr_clock.clock_total_ns += curr_ns - hr_clock.clock_last_ns;
            hr_clock.clock_last_ns = curr_ns;
        });
    });
}

void Init() {
    Update();
}

Time Now() noexcept {
    uint64_t ns = 0;
    hr_clock.seqlock.Read([&]() {
        uint64_t curr_ns = arch::GetClockNs();
        uint64_t delta_ns = curr_ns - hr_clock.clock_last_ns;
        time_t secs_since_start = delta_ns / NS_IN_SEC;
        ns = (hr_clock.wall_time_start_secs + secs_since_start) * NS_IN_SEC + delta_ns;
    });
    return Time(ns);
}

Time NowMonotonic() noexcept {
    uint64_t ns = 0;
    hr_clock.seqlock.Read([&]() {
        uint64_t curr_ns = arch::GetClockNs();
        ns = curr_ns - hr_clock.clock_last_ns + hr_clock.clock_total_ns;
    });
    return Time(ns);
}

namespace {

ListHead<TimerBase, &TimerBase::list_> AllTimers;
SpinLock AllTimersLock;

void CheckTimers() noexcept {
    auto now = NowMonotonic();

    IrqSafeScopeLocker locker(AllTimersLock);
    if (AllTimers.Empty()) {
        return;
    }

    for (TimerBase& timer : AllTimers) {
        if (timer.deadline_.After(now)) {
            continue;
        }

        timer.list_.Remove();
        timer.finished_ = true;
        locker.Unlock();
        timer();
        break;
    }
}

}

TimerBase::TimerBase(Time deadline) noexcept
    : deadline_(deadline)
{
    IrqSafeScopeLocker locker(AllTimersLock);
    AllTimers.InsertLast(*this);
}

void TimerBase::Cancel() noexcept {
    IrqSafeScopeLocker locker(AllTimersLock);
    if (finished_) {
        return;
    }
    finished_ = true;
    list_.Remove();
}

void SleepUntil(Time deadline) noexcept {
    kern::WaitQueue wq;
    wq.WaitCondDeadline([&]() { return false; }, deadline);
}

void Update() {
    Sync();
}

void PeriodicTick() noexcept {
    CheckTimers();
}

timeval NanosecondsToTimeval(uint64_t ns) noexcept {
    timeval tv;
    tv.tv_sec = ns / NS_IN_SEC;
    tv.tv_usec = (ns % NS_IN_SEC) / 1000;

    if (tv.tv_usec >= 1'000'000) {
        tv.tv_sec += tv.tv_usec / 1'000'000;
        tv.tv_usec %= 1'000'000;
    }
    return tv;
}

kern::Errno SysGettimeofday(sched::Task*, timeval* tv) noexcept {
    Time now = Now();
    auto tmp_tv = NanosecondsToTimeval(now.nanoseconds);
    if (!mm::CopyToUser(tv, tmp_tv)) {
        return kern::EFAULT;
    }
    return kern::ENOERR;
}
REGISTER_SYSCALL(gettimeofday, SysGettimeofday);


}
