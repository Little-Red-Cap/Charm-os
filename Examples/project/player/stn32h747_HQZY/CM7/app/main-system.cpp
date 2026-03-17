#include <cstddef>
#include <cstdint>
#include <expected>

import out.api;
import charm.system.clock;
import charm.system.time;
import player.stm32h7.system_entry;

extern "C" {
void charm_boot_init_core(void);
void* charm_boot_init_uart(void);
int charm_boot_uart_write_bytes(void* uart, const uint8_t* data, uint16_t len);
uint32_t charm_boot_get_tick(void);

void MX_I2S1_Init(void);
}

namespace {
    charm::system::ClockTick hal_now_ms(void*) noexcept {
        return static_cast<charm::system::ClockTick>(charm_boot_get_tick());
    }

    charm::system::Clock g_clock{nullptr, charm::system::ClockOps{&hal_now_ms, nullptr}};

    struct uart_sink {
        void* uart{};

        out::result<std::size_t> write(out::bytes b) noexcept {
            if (!uart) {
                return out::result<std::size_t>{std::unexpected(out::errc::bad_state)};
            }
            if (b.size() == 0) return out::ok<std::size_t>(0u);
            auto* data = reinterpret_cast<const uint8_t*>(b.data());
            auto len = static_cast<uint16_t>(b.size());
            auto ok = charm_boot_uart_write_bytes(uart, data, len) != 0;
            if (!ok) return out::result<std::size_t>{std::unexpected(out::errc::io)};
            return out::ok(b.size());
        }
    };
}

int main(void) {
    charm_boot_init_core();
    charm::system::time::bind(g_clock);
    MX_I2S1_Init();
    auto* uart = charm_boot_init_uart();
    uart_sink sink{uart};
    out::Scope scope{sink};
    out::println<"boot: system main">();

    system_run(uart);
}
