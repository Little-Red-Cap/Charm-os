module;

#include <atomic>

export module platform.win.irq_guard;

export namespace platform::win {
    struct NoopIrqGuard {
        struct Token { };
        static constexpr Token enter() noexcept { return {}; }
        static constexpr void leave(Token) noexcept { }
    };

    struct SpinIrqGuard {
        struct Token { };

        static Token enter() noexcept {
            while (flag_.test_and_set(std::memory_order_acquire)) {
            }
            return {};
        }

        static void leave(Token) noexcept {
            flag_.clear(std::memory_order_release);
        }

    private:
        inline static std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
    };
}
