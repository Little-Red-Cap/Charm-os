module;

#include <cstdint>
#include <span>
#include <array>

export module charm.system.clock;

import init.node;
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

    inline Clock* g_clock = nullptr;

    inline void set_clock(Clock& clock) noexcept {
        g_clock = &clock;
    }

    [[nodiscard]] inline Clock& clock() noexcept {
        static Clock fallback{};
        return g_clock ? *g_clock : fallback;
    }

    struct ClockCaps {
        struct TimeSource {
            using Tick = ClockTick;
            static Tick now() noexcept { return clock().now_ms(); }
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
            provides[0] = init::cap_id(cap_name);
            node = init::Node{
                cap_name,
                phase,
                runlevel_mask,
                std::span<const init::CapId>(provides.data(), provides.size()),
                {},
                &ClockBinding::init_trampoline,
                nullptr,
                this
            };
        }

        static util::Result<void> init_trampoline(void* ctx) noexcept {
            auto* self = static_cast<ClockBinding*>(ctx);
            if (!self || !self->clock_ref) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if (g_clock != nullptr && g_clock != self->clock_ref) {
                return util::unexpected(util::Errc::exist);
            }
            set_clock(*self->clock_ref);
            return {};
        }
    };
}
