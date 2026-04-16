module;

#include <cstdint>
#include <span>
#include <array>
#include <string_view>

export module charm.system.clock;

import init.binding;
import util.core;
import util.error;

export namespace charm::system {
    using ClockTick = util::u64;
    using ClockFn = ClockTick (*)(void* ctx) noexcept;

    struct ClockOps {
        ClockFn now_ms{nullptr};
        ClockFn now_us{nullptr};
    };

    struct Clock {
        void* ctx{nullptr};
        ClockOps ops{};

        constexpr Clock() noexcept = default;
        constexpr Clock(void* ctx_in, ClockOps ops_in) noexcept : ctx(ctx_in), ops(ops_in) {}

        void reset(void* ctx_in, ClockOps ops_in) noexcept {
            ctx = ctx_in;
            ops = ops_in;
        }

        [[nodiscard]] ClockTick now_ms() const noexcept {
            if (ops.now_ms) {
                return ops.now_ms(ctx);
            }
            if (ops.now_us) {
                return ops.now_us(ctx) / 1000u;
            }
            return 0;
        }

        [[nodiscard]] ClockTick now_us() const noexcept {
            if (ops.now_us) {
                return ops.now_us(ctx);
            }
            if (ops.now_ms) {
                return ops.now_ms(ctx) * 1000u;
            }
            return 0;
        }
    };

    struct ClockRef {
        Clock* clock{nullptr};

        void reset(Clock& clock_in) noexcept { clock = &clock_in; }

        [[nodiscard]] bool valid() const noexcept { return clock != nullptr; }
        [[nodiscard]] ClockTick now_ms() const noexcept { return clock ? clock->now_ms() : 0; }
        [[nodiscard]] ClockTick now_us() const noexcept { return clock ? clock->now_us() : 0; }
    };

    struct ClockCaps {
        struct TimeSource {
            using Tick = ClockTick;
            static Tick now() noexcept;
            static Tick now_us() noexcept;
            static Clock* bound() noexcept;
            static void bind(Clock& clock_in) noexcept;
        };
    };

    struct ClockBinding {
        Clock* clock_ref{nullptr};
        std::array<init::CapId, 1> provides{};
        init::Node node{};

        explicit ClockBinding(Clock& clock_in,
                              const char* cap_name = "system.clock",
                              init::Phase phase = init::Phase::core,
                              util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all)) noexcept
            : clock_ref(&clock_in) {
            provides = init::capability_ids(cap_name);
            node = init::make_binding_node(init::capability_name_view(cap_name),
                                           phase,
                                           runlevel_mask,
                                           provides,
                                           &ClockBinding::init_trampoline,
                                           nullptr,
                                           this);
        }

        constexpr std::string_view capability_name(init::CapId id) const noexcept {
            return init::lookup_capability_name(id,
                                                provides,
                                                init::capability_names(node.name));
        }

        static util::Result<void> init_trampoline(void* ctx) noexcept {
            auto* self = static_cast<ClockBinding*>(ctx);
            if (!self || !self->clock_ref) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            const auto* bound = ClockCaps::TimeSource::bound();
            if (bound != nullptr && bound != self->clock_ref) {
                return util::unexpected(util::Errc::exist);
            }
            ClockCaps::TimeSource::bind(*self->clock_ref);
            return {};
        }
    };
}

namespace {
    charm::system::Clock* g_time_source_clock = nullptr;
}

export namespace charm::system {

    ClockCaps::TimeSource::Tick ClockCaps::TimeSource::now() noexcept {
        return g_time_source_clock ? g_time_source_clock->now_ms() : 0;
    }

    ClockCaps::TimeSource::Tick ClockCaps::TimeSource::now_us() noexcept {
        return g_time_source_clock ? g_time_source_clock->now_us() : 0;
    }

    Clock* ClockCaps::TimeSource::bound() noexcept {
        return g_time_source_clock;
    }

    void ClockCaps::TimeSource::bind(Clock& clock_in) noexcept {
        g_time_source_clock = &clock_in;
    }
}
