#pragma once

#include <array>
#include <cstdint>

#include "kernel/error.h"
#include "lib/list.h"
#include "uapi/time.h"

typedef uint64_t time_t;
typedef uint64_t suseconds_t;

namespace time {

constexpr suseconds_t NS_IN_MSEC = 1'000'000;
constexpr suseconds_t NS_IN_SEC = 1'000'000'000;
constexpr suseconds_t MS_2_FS = 1'000'000'000'000;

constexpr suseconds_t TICK_NS = 1 * NS_IN_MSEC;

// https://github.com/protocolbuffers/upb/blob/22182e6e/upb/json/parser.rl#L1697
inline time_t FromGregorian(uint64_t year, uint64_t month, uint64_t day, uint64_t hour, uint64_t min, uint64_t sec) {
    constexpr std::array DAYS_IN_MONTH{ 0, 31,  59,  90,  120, 151, 181, 212, 243, 273, 304, 334 };
    uint32_t yearAdj = year + 4800;
    uint32_t febs = yearAdj - (month <= 2 ? 1 : 0);
    uint32_t leapDays = 1 + (febs / 4) - (febs / 100) + (febs / 400);
    uint32_t days = 365 * yearAdj + leapDays + DAYS_IN_MONTH[month - 1] + day - 1;
    return (((days - 2472692) * 24 + hour) * 60 + min) * 60 + sec;
}

struct Time {
    uint64_t nanoseconds;

    Time() = default;
    Time(uint64_t ns)
        : nanoseconds(ns)
    {}

    Time Add(uint64_t ns) const noexcept {
        return Time(nanoseconds + ns);
    }

    Time Sub(Time other) const noexcept {
        BUG_ON(nanoseconds < other.nanoseconds);
        return Time(nanoseconds - other.nanoseconds);
    }

    bool After(Time x) noexcept {
        return nanoseconds > x.nanoseconds;
    }

    bool Before(Time x) noexcept {
        return nanoseconds < x.nanoseconds;
    }

    uint64_t Seconds() noexcept {
        return nanoseconds / 1'000'000;
    }
};

Time Now() noexcept;
Time NowMonotonic() noexcept;

namespace literals {

constexpr uint64_t operator""_s(unsigned long long int s) {
    return s * 1'000'000'000;
}

constexpr uint64_t operator""_ms(unsigned long long int ms) {
    return ms * 1'000'000;
}

constexpr uint64_t operator""_ns(unsigned long long int ns) {
    return ns;
}

}

class TimerBase {
public:
    Time deadline_;
    ListNode list_;
    bool finished_ = false;

    TimerBase(const TimerBase&) = delete;
    TimerBase(TimerBase&&) = delete;

    TimerBase(Time deadline) noexcept;

    void Cancel() noexcept;

    virtual void operator()() noexcept = 0;
};

template <typename Cb>
class Timer : private TimerBase {
private:
    Cb cb;

    void operator()() noexcept override {
        return cb();
    }

public:
    Timer(Time deadline, Cb&& cb)
        : TimerBase(deadline)
        , cb(cb)
    {}

    ~Timer() noexcept {
        Cancel();
    }

    using TimerBase::Cancel;
};

void SleepUntil(Time deadline) noexcept;

void Update();
void PeriodicTick() noexcept;
void Init();

timeval NanosecondsToTimeval(uint64_t nsecs) noexcept;
}
