module;

#include <cstddef>
#include <concepts>
#include <type_traits>

export module kernel.capabilities;

import util.core;

export namespace kernel {
    struct NoopTimeSource {
        using Tick = util::u64;
        static constexpr Tick now() noexcept { return 0; }
    };

    struct NoopIrqGuard {
        struct Token { };
        static constexpr Token enter() noexcept { return {}; }
        static constexpr void leave(Token) noexcept { }
    };

    struct NoopWakeup {
        static constexpr void signal() noexcept { }
    };

    struct NoopSwiTrigger {
        static constexpr void trigger(util::usize) noexcept { }
    };

    template <typename T>
    concept TimeSource = requires {
        typename T::Tick;
        { T::now() } -> std::same_as<typename T::Tick>;
    };

    template <typename T>
    concept IrqGuard = requires {
        typename T::Token;
        { T::enter() } -> std::same_as<typename T::Token>;
        T::leave(typename T::Token{});
    };

    template <typename T>
    concept Wakeup = requires {
        T::signal();
    };

    template <typename T>
    concept SwiTrigger = requires {
        T::trigger(util::usize{});
    };

    template <typename Caps>
    concept Capabilities = TimeSource<typename Caps::TimeSource>
        && IrqGuard<typename Caps::IrqGuard>
        && Wakeup<typename Caps::Wakeup>
        && SwiTrigger<typename Caps::SwiTrigger>;

    struct NoopCapabilities {
        using TimeSource = NoopTimeSource;
        using IrqGuard = NoopIrqGuard;
        using Wakeup = NoopWakeup;
        using SwiTrigger = NoopSwiTrigger;
    };
}
