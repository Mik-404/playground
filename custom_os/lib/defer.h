#pragma once
#include <concepts>
#include <utility>

namespace kernel {
    template <std::invocable Handler>
    class Defer {
    public:
        Defer(Handler&& handler) : handler_(std::forward<Handler>(handler)) {}

        ~Defer() {
            handler_();
        }
        
        Defer(Defer&&) noexcept = delete;
        Defer& operator=(Defer&&) noexcept = delete;

        Defer(const Defer&) = delete;
        Defer& operator=(const Defer&) = delete;

    private:
        Handler handler_;
    };
} // namespace kernel