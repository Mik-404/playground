#pragma once

#include <expected>

#include "arch/ptr.h"
#include "kernel/panic.h"

namespace kern {

class [[nodiscard("kern::Errno should be checked")]] Errno {
private:
    int code = 0;

public:
    constexpr Errno() : code(0) {}
    explicit constexpr Errno(int code) : code(code) {}

    constexpr bool Ok() const {
        return code == 0;
    }

    constexpr int Code() const {
        return code;
    }

    constexpr bool operator==(const Errno& x) noexcept {
        return code == x.code;
    }
};

constexpr Errno ENOERR = Errno(0);
constexpr Errno ENOSYS = Errno(1);
constexpr Errno ENOMEM = Errno(2);
constexpr Errno EINVAL = Errno(3);
constexpr Errno EBADF = Errno(4);
constexpr Errno EMFILE = Errno(5);
constexpr Errno ENOENT = Errno(6);
constexpr Errno EISDIR = Errno(7);
constexpr Errno EIO = Errno(8);
constexpr Errno EACCES = Errno(9);
constexpr Errno ECHILD = Errno(10);
constexpr Errno EFAULT = Errno(11);
constexpr Errno ENOTDIR = Errno(12);
constexpr Errno ENAMETOOLONG = Errno(13);
constexpr Errno ESRCH = Errno(14);
constexpr Errno EFBIG = Errno(15);
constexpr Errno EAGAIN = Errno(16);
constexpr Errno EPIPE = Errno(17);
constexpr Errno ENOSPC = Errno(18);
constexpr Errno EEXIST = Errno(19);

template <typename T, typename = void>
class [[nodiscard("kern::Result should be checked")]] Result {
private:
    std::expected<T, Errno> res_;

public:
    template <typename U = T>
    Result(typename std::enable_if_t<!std::is_void_v<U>, const U&> t) noexcept : res_(t) {}

    template <typename U = T>
    Result(typename std::enable_if_t<!std::is_void_v<U>, U&&> t) noexcept : res_(std::move(t)) {}

    Result(Errno e) noexcept : res_(std::unexpected(e)) {}

    Result() = default;

    template <typename U = T>
    Result& operator=(typename std::enable_if_t<!std::is_void_v<U>, const U&> t) noexcept {
        res_ = t;
        return *this;
    }

    template <typename U = T>
    Result& operator=(typename std::enable_if_t<!std::is_void_v<U>, U&&> t) noexcept {
        res_ = std::move(t);
        return *this;
    }

    void operator=(Errno err) noexcept {
        res_ = std::unexpected(err);
    }

    bool Ok() const noexcept {
        return res_.has_value() || res_.error().Code() == 0;
    }

    Errno Err() const noexcept {
        return res_.error();
    }

    template <typename U = T>
    typename std::enable_if_t<!std::is_void_v<U>, U&> Val() noexcept {
        return res_.value();
    }

    template <typename U = T>
    typename std::enable_if_t<!std::is_void_v<U>, const U&> Val() const noexcept {
        return res_.value();
    }

    template <typename U = T>
    typename std::enable_if_t<!std::is_void_v<U>, U&> operator*() noexcept {
        return res_.value();
    }

    template <typename U = T>
    typename std::enable_if_t<!std::is_void_v<U>, const U&> operator*() const noexcept {
        return res_.value();
    }

    T& operator->() noexcept {
        return res_.value();
    }

    const T& operator->() const noexcept {
        BUG_ON(!Ok());
        return res_.value();
    }

    Errno ToErrno() const noexcept {
        if (Ok()) {
            return kern::ENOERR;
        }
        return res_.error();
    }
};

template <typename TT>
class [[nodiscard("kern::Result should be checked")]] Result<TT, std::enable_if_t<std::is_pointer_v<TT>>> {
private:
    using T = std::remove_pointer_t<TT>;
    T* res_ = nullptr;

public:
    Result(T* ptr) noexcept : res_(ptr) {}
    Result(Errno e) noexcept
        : res_((T*)arch::ErrorPointer(e.Code()))
    {}
    Result() = default;

    Result& operator=(T* ptr) noexcept {
        res_ = ptr;
        return *this;
    }

    void operator=(Errno err) noexcept {
        res_ = (T*)arch::ErrorPointer(err.Code());
    }

    bool Ok() const noexcept {
        return !arch::IsErrorPointer((uintptr_t)res_);
    }

    Errno Err() const noexcept {
        BUG_ON(Ok());
        return kern::Errno(arch::ErrorFromPointer((uintptr_t)res_));
    }

    T* Val() const noexcept {
        BUG_ON(!Ok());
        return res_;
    }

    T* operator*() const noexcept {
        BUG_ON(!Ok());
        return res_;
    }

    T* operator->() noexcept {
        BUG_ON(!Ok());
        return res_;
    }

    Errno ToErrno() const noexcept {
        if (Ok()) {
            return kern::ENOERR;
        }
        return Err();
    }
};

}
