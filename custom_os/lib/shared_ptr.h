#pragma once

#include <atomic>

#include "kernel/panic.h"

class RefCounted {
private:
    std::atomic<size_t> ref_count_ = 0;

public:
    void Ref() noexcept {
        ref_count_.fetch_add(1, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_acquire);
    }

    bool Unref() noexcept {
        std::atomic_thread_fence(std::memory_order_release);
        bool last = ref_count_.fetch_sub(1, std::memory_order_relaxed) == 1;
        return last;
    }

    size_t RefCount() const noexcept {
        return ref_count_.load(std::memory_order_relaxed);
    }
};

template <typename T>
class RefCountedDeleterTracker {
public:
    static void Ref(T* t) noexcept {
        t->Ref();
    }

    static void Unref(T* t) noexcept {
        if (t->Unref()) {
            delete t;
        }
    }
};

template <typename T>
class NoOpRefCountedTracker {
public:
    static void Ref(T* t) noexcept {
        t->Ref();
    }

    static void Unref(T* t) noexcept {
        t->Unref();
    }
};

template <typename T, typename Tracker = RefCountedDeleterTracker<T>>
class IntrusiveSharedPtr {
private:
    T* Obj = nullptr;

    void Ref() noexcept {
        if (Obj) {
            Tracker::Ref(Obj);
        }
    }

    void Unref() noexcept {
        if (Obj) {
            Tracker::Unref(Obj);
        }
    }

public:
    IntrusiveSharedPtr() noexcept {}

    explicit IntrusiveSharedPtr(T* ptr) noexcept {
        Unref();
        Obj = ptr;
        Ref();
    }

    IntrusiveSharedPtr(std::nullptr_t) noexcept {}

    IntrusiveSharedPtr(const IntrusiveSharedPtr<T, Tracker>& ptr) noexcept {
        *this = ptr;
    }

    IntrusiveSharedPtr(IntrusiveSharedPtr<T, Tracker>&& ptr) noexcept {
        *this = std::move(ptr);
    }

    IntrusiveSharedPtr<T, Tracker>& operator=(std::nullptr_t) noexcept {
        Unref();
        Obj = nullptr;
        return *this;
    }

    IntrusiveSharedPtr<T, Tracker>& operator=(const IntrusiveSharedPtr<T, Tracker>& ptr) noexcept {
        Unref();
        Obj = ptr.Obj;
        Ref();
        return *this;
    }

    IntrusiveSharedPtr<T, Tracker>& operator=(IntrusiveSharedPtr<T, Tracker>&& ptr) noexcept {
        Unref();
        Obj = ptr.Obj;
        ptr.Obj = nullptr;
        return *this;
    }

    T* Get() noexcept {
        return Obj;
    }

    void Reset() noexcept {
        *this = nullptr;
    }

    void Release() noexcept {
        Obj = nullptr;
    }

    const T* Get() const noexcept {
        return Obj;
    }

    T& operator*() noexcept {
        BUG_ON_NULL(Obj);
        return *Obj;
    }

    T* operator->() noexcept {
        BUG_ON_NULL(Obj);
        return Obj;
    }

    const T& operator*() const noexcept {
        BUG_ON_NULL(Obj);
        return *Obj;
    }

    const T* operator->() const noexcept {
        BUG_ON_NULL(Obj);
        return Obj;
    }

    explicit operator bool() const noexcept {
        return Obj != nullptr;
    }

    ~IntrusiveSharedPtr() noexcept {
        Unref();
    }
};
