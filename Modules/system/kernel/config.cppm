module;

#include <cstddef>
#include <cstdint>

export module kernel.config;

export namespace kernel {
    struct KernelConfig {
        static constexpr bool enable_timer = false;
        static constexpr bool enable_dynamic_priority = false;
        static constexpr std::size_t priority_levels = 4;
        static constexpr std::size_t evtq_capacity = 64;
        static constexpr std::size_t timer_capacity = 16;
        static constexpr std::size_t dispatch_budget = 0;
        static constexpr bool enable_event_boost = false;
        static constexpr std::uint32_t boost_mask = 0;
        static constexpr bool enable_timer_merge = false;
        static constexpr bool enable_event_dedup = false;
        static constexpr std::size_t wakeup_batch = 1;
        static constexpr bool enable_event_debounce = false;
        static constexpr std::size_t debounce_window = 0;
        static constexpr bool enable_prio_stats = false;
        static constexpr bool enable_event_coalesce = false;
        static constexpr bool enable_rate_limit = false;
        static constexpr bool enable_task_boost = false;
        static constexpr bool enable_alert = false;
        static constexpr std::size_t alert_queue = 0;
        static constexpr std::size_t alert_timer = 0;
        static constexpr std::size_t alert_filtered = 0;
        static constexpr std::size_t alert_queue_warn = 0;
        static constexpr std::size_t alert_timer_warn = 0;
        static constexpr std::size_t alert_filtered_warn = 0;
        static constexpr std::size_t alert_queue_err = 0;
        static constexpr std::size_t alert_timer_err = 0;
        static constexpr std::size_t alert_filtered_err = 0;
        static constexpr bool drop_oldest = false;
        static constexpr bool enable_trace = false;
        static constexpr std::size_t trace_capacity = 0;
    };

    template <typename Config>
    consteval void validate_config() {
        static_assert(Config::priority_levels >= 1);
        static_assert(Config::evtq_capacity >= 8);
        if constexpr (Config::enable_timer) {
            static_assert(Config::timer_capacity >= 1);
        }
        if constexpr (Config::enable_timer_merge) {
            static_assert(Config::enable_timer, "timer_merge requires enable_timer");
        }
        if constexpr (Config::enable_event_boost) {
            static_assert(Config::boost_mask != 0, "event_boost enabled but boost_mask is zero");
        }
        if constexpr (Config::enable_event_dedup) {
            static_assert(true, "event_dedup enabled");
        }
        static_assert(Config::wakeup_batch >= 1, "wakeup_batch must be >= 1");
        if constexpr (Config::enable_event_debounce) {
            static_assert(Config::debounce_window > 0, "event_debounce requires debounce_window > 0");
        }
        if constexpr (Config::enable_event_coalesce) {
            static_assert(Config::enable_event_dedup || Config::enable_event_debounce || Config::enable_timer_merge,
                "event_coalesce enabled but no upstream filtering enabled");
        }
        if constexpr (Config::enable_trace) {
            static_assert(Config::trace_capacity > 0, "trace enabled but trace_capacity is zero");
        }
        if constexpr (Config::enable_rate_limit) {
            static_assert(true, "rate_limit enabled");
        }
        if constexpr (Config::enable_alert) {
            static_assert(Config::alert_queue_warn > 0 || Config::alert_timer_warn > 0 || Config::alert_filtered_warn > 0
                || Config::alert_queue_err > 0 || Config::alert_timer_err > 0 || Config::alert_filtered_err > 0,
                "alert enabled but thresholds are zero");
        }
    }
}
