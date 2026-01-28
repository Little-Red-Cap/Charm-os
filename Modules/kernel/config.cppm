export module kernel.config;

import <cstddef>;

export namespace kernel {
    struct KernelConfig {
        static constexpr bool enable_timer = false;
        static constexpr bool enable_dynamic_priority = false;
        static constexpr std::size_t priority_levels = 4;
        static constexpr std::size_t evtq_capacity = 64;
        static constexpr std::size_t timer_capacity = 16;
    };

    template <typename Config>
    consteval void validate_config() {
        static_assert(Config::priority_levels >= 1);
        static_assert(Config::evtq_capacity >= 8);
        if constexpr (Config::enable_timer) {
            static_assert(Config::timer_capacity >= 1);
        }
    }
}
