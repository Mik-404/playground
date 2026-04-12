#pragma once

#include <array>
#include <atomic>

#include "kernel/error.h"

namespace sched {
struct Task;
}

namespace kern {

enum class Signal : int {
    INVALID = -1,

    SIGHUP = 1,
    SIGINT = 2,
    SIGQUIT = 3,
    SIGILL = 4,
    SIGTRAP = 5,
    SIGABRT = 6,
    SIGBUS = 7,
    SIGFPE = 8,
    SIGKILL = 9,
    SIGUSR1 = 10,
    SIGSEGV = 11,
    SIGUSR2 = 12,
    SIGPIPE = 13,
    SIGALRM = 14,
    SIGTERM = 15,
    SIGCHLD = 17,
    SIGCONT = 18,
    SIGSTOP = 19,
    SIGTSTP = 20,

    MAX = 32,
};

struct SigAction {
    void* handler = nullptr;

    static SigAction Ignored() noexcept {
        return (SigAction){ .handler = (void*)0xfffffffffffffff1 };
    }

    static SigAction Default() noexcept {
        return (SigAction){ .handler = (void*)0xfffffffffffffff0 };
    }

    bool operator==(const SigAction& other) const noexcept {
        return handler == other.handler;
    }
};

class SigActions {
private:
    std::array<SigAction, (size_t)Signal::MAX> actions;

public:
    SigActions() {
        actions.fill(SigAction::Default());
    }

    SigAction operator[](Signal sig) const noexcept {
        return actions[(size_t)sig];
    }

    SigAction& operator[](Signal sig) noexcept {
        return actions[(size_t)sig];
    }
};

// SignalSend sends a signal to given task.
void SignalSend(sched::Task* task, Signal sig);

// SignalDeliver delivers a pending signal immediately if there are any.
// This routine returns only if there no pending signal.
void SignalDeliver();

struct AtomicSignalMask : private std::atomic<uint32_t> {
private:
    static_assert(sizeof(value_type) * 8 >= (size_t)Signal::MAX);

    static constexpr value_type SignalToBit(Signal sig) noexcept {
        return static_cast<value_type>(1) << static_cast<value_type>(sig);
    }

public:
    AtomicSignalMask()
        : atomic(0)
    {}

    bool Has(Signal sig, std::memory_order mem_ord = std::memory_order_relaxed) noexcept {
        return load(mem_ord) & SignalToBit(sig);
    }

    void Add(Signal sig, std::memory_order mem_ord = std::memory_order_relaxed) noexcept {
        fetch_or(SignalToBit(sig), mem_ord);
    }

    bool TryPick(Signal sig, std::memory_order mem_ord = std::memory_order_relaxed) noexcept {
        return fetch_and(~SignalToBit(sig), mem_ord) & SignalToBit(sig);
    }
};

}
