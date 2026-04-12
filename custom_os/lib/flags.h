#pragma once

#include <type_traits>

template <typename T, typename = typename std::enable_if<std::is_enum_v<T>>>
class BitFlags {
public:
    using RawFlags = std::underlying_type_t<T>;

private:
    RawFlags flags_ = 0;

    explicit BitFlags(RawFlags flags) : flags_(flags) {}

public:
    BitFlags() = default;
    BitFlags(T f) noexcept
        : flags_(static_cast<RawFlags>(f))
    {}

    bool Has(T flag) const noexcept {
        return flags_ & static_cast<RawFlags>(flag);
    }

    bool HasAll(BitFlags f) const noexcept {
        return (flags_ & f.flags_) == f.flags_;
    }

    constexpr explicit operator bool() const {
        return flags_ != 0;
    }

    constexpr friend BitFlags operator|(BitFlags l, BitFlags r) noexcept {
        return BitFlags(l.flags_ | r.flags_);
    }
    constexpr friend BitFlags operator|(T l, BitFlags r) noexcept {
        return BitFlags(static_cast<RawFlags>(l) | r.flags_);
    }
    constexpr friend BitFlags operator|(BitFlags l, T r) noexcept {
        return BitFlags(l.flags_ | static_cast<RawFlags>(r));
    }

    constexpr friend BitFlags operator&(BitFlags l, BitFlags r) noexcept {
        return BitFlags(l.flags_ & r.flags_);
    }
    constexpr friend BitFlags operator&(T l, BitFlags r) noexcept {
        return BitFlags(static_cast<RawFlags>(l) & r.flags_);
    }
    constexpr friend BitFlags operator&(BitFlags l, T r) noexcept {
        return BitFlags(l.flags_ & static_cast<RawFlags>(r));
    }

    constexpr friend BitFlags operator^(BitFlags l, BitFlags r) noexcept {
        return BitFlags(l.flags_ ^ r.flags_);
    }
    constexpr friend BitFlags operator^(T l, BitFlags r) noexcept {
        return BitFlags(static_cast<RawFlags>(l) ^ r.flags_);
    }
    constexpr friend BitFlags operator^(BitFlags l, T r) noexcept {
        return BitFlags(l.flags_ ^ static_cast<RawFlags>(r));
    }

    template <typename T2>
    BitFlags& operator|=(const T2& r) noexcept {
        *this = *this | r;
        return *this;
    }

    template <typename T2>
    BitFlags& operator&=(const T2& r) noexcept {
        *this = *this & r;
        return *this;
    }

    template <typename T2>
    BitFlags& operator^=(const T2& r) noexcept {
        *this = *this ^ r;
        return *this;
    }
};

template <typename T>
constexpr BitFlags<T> operator|(T l, T r) noexcept {
    return static_cast<BitFlags<T>>(l) | r;
}

template <typename T>
constexpr BitFlags<T> operator&(T l, T r) noexcept {
    return static_cast<BitFlags<T>>(l) & r;
}

template <typename T>
constexpr BitFlags<T> operator^(T l, T r) noexcept {
    return static_cast<BitFlags<T>>(l) ^ r;
}
