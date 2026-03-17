module;
#include <cstdint>
#include <expected>

export module charm.port;

import out.core;
import out.sink;

extern "C" {
    void charm_port_init_core(void);
    void* charm_port_init_console(void);
    int charm_port_console_write(void* uart, const uint8_t* data, uint16_t len);
    uint32_t charm_port_now_ms(void);
}

export namespace charm::port {
    using ClockTick = std::uint64_t;
    using ClockFn = ClockTick (*)(void* ctx) noexcept;

    struct ConsoleSink {
        void* ctx{};

        out::result<std::size_t> write(out::bytes b) noexcept {
            if (!ctx) return out::result<std::size_t>{std::unexpected(out::errc::bad_state)};
            if (b.size() == 0) return out::ok<std::size_t>(0u);
            auto* data = reinterpret_cast<const uint8_t*>(b.data());
            auto len = static_cast<uint16_t>(b.size());
            if (!charm_port_console_write(ctx, data, len)) {
                return out::result<std::size_t>{std::unexpected(out::errc::io)};
            }
            return out::ok(b.size());
        }
    };

    struct Kit {
        ConsoleSink console{};
        void* time_ctx{};
    };

    Kit init() noexcept;
    ClockTick now_ms(void* ctx) noexcept;
}

namespace charm::port {
    inline Kit init() noexcept {
        charm_port_init_core();
        auto* console = charm_port_init_console();
        return Kit{
            ConsoleSink{console},
            nullptr
        };
    }

    inline ClockTick now_ms(void*) noexcept {
        return static_cast<ClockTick>(charm_port_now_ms());
    }
}
