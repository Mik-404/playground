#pragma once

#include "defs.h"
#include "kernel/irq.h"
#include "kernel/panic.h"

namespace sched {
    void PreemptEnable();
    void PreemptDisable();
}

template <typename RawLock>
class RawScopeLocker {
private:
    RawLock* lock_ = nullptr;

public:
    RawScopeLocker(RawLock& lock) noexcept
        : lock_(&lock)
    {
        lock_->RawLock();
    }

    void Unlock() noexcept {
        BUG_ON_NULL(lock_);
        lock_->RawUnlock();
        lock_ = nullptr;
    }

    template <typename Fn>
    void Escape(Fn fn) noexcept {
        lock_->RawUnlock();
        fn();
        lock_->RawLock();
    }

    ~RawScopeLocker() noexcept {
        if (lock_) {
            Unlock();
        }
    }
};

template <typename RawLock>
class PreemptSafeScopeLocker {
private:
    RawLock* lock_ = nullptr;

public:
    PreemptSafeScopeLocker(RawLock& lock) noexcept
        : lock_(&lock)
    {
        sched::PreemptDisable();
        lock_->RawLock();
    }

    void Unlock() noexcept {
        BUG_ON_NULL(lock_);
        lock_->RawUnlock();
        sched::PreemptEnable();
        lock_ = nullptr;
    }

    template <typename Fn>
    void Escape(Fn fn) noexcept {
        lock_->RawUnlock();
        sched::PreemptEnable();
        fn();
        sched::PreemptDisable();
        lock_->RawLock();
    }

    ~PreemptSafeScopeLocker() noexcept {
        if (lock_) {
            Unlock();
        }
    }
};

template <typename RawLock>
class IrqSafeScopeLocker {
private:
    bool restore_;
    RawLock* lock_ = nullptr;

public:
    IrqSafeScopeLocker(RawLock& lock) noexcept
        : lock_(&lock)
    {
        restore_ = kern::IsIrqEnabled();
        if (restore_) {
            kern::IrqDisable();
        }
        lock_->RawLock();
    }

    void Unlock() noexcept {
        BUG_ON_NULL(lock_);
        lock_->RawUnlock();
        if (restore_) {
            kern::IrqEnable();
        }
        lock_ = nullptr;
    }

    template <typename Fn>
    void Escape(Fn fn) noexcept {
        lock_->RawUnlock();
        if (restore_) {
            kern::IrqEnable();
        }
        fn();
        restore_ = kern::IsIrqEnabled();
        if (restore_) {
            kern::IrqDisable();
        }
        lock_->RawLock();
    }

    ~IrqSafeScopeLocker() noexcept {
        if (lock_) {
            Unlock();
        }
    }
};

template <typename RawLock, typename Fn>
auto WithRawLocked(RawLock& lock, Fn fn) {
    RawScopeLocker locker(lock);
    return fn();
}

template <typename RawLock, typename Fn>
auto WithPreemptSafeLocked(RawLock& lock, Fn fn) {
    PreemptSafeScopeLocker locker(lock);
    return fn();
}

template <typename RawLock, typename Fn>
auto WithIrqSafeLocked(RawLock& lock, Fn fn) {
    IrqSafeScopeLocker locker(lock);
    return fn();
}
