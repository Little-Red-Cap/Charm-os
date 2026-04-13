module;

#include <array>
#include <span>
#include <string_view>

export module hal_uart.node;

import hal_core;
import hal_uart;
import hal_core;
import init.node;
import util.core;
import util.error;

export namespace hal {
    constexpr util::Errc map_status(Status s) noexcept {
        switch (s) {
        case Status::ok:
            return util::Errc::ok;
        case Status::busy:
            return util::Errc::busy;
        case Status::timeout:
            return util::Errc::timeout;
        case Status::unsupported:
            return util::Errc::not_supported;
        case Status::error:
        default:
            return util::Errc::io;
        }
    }

    struct UartBinding {
        UartIoHandle handle{};
        UartConfig config{};
        const char* irq_cap_name{"platform.irq"};
        std::array<init::CapId, 1> provides{};
        std::array<init::CapId, 1> requires_caps{};
        init::Node node{};

        UartBinding(UartIoHandle h,
                    const UartConfig& cfg,
                    const char* cap_name = "hal.uart1",
                    const char* irq_cap_name = "platform.irq",
                    init::Phase phase = init::Phase::core,
                    util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all)) noexcept
            : handle(h), config(cfg), irq_cap_name(irq_cap_name) {
            provides[0] = init::cap_id(cap_name);
            requires_caps[0] = init::cap_id(irq_cap_name);
            node = init::Node{
                cap_name,
                phase,
                runlevel_mask,
                std::span<const init::CapId>(provides.data(), provides.size()),
                std::span<const init::CapId>(requires_caps.data(), requires_caps.size()),
                &UartBinding::init_trampoline,
                nullptr,
                this
            };
        }

        constexpr std::string_view capability_name(init::CapId id) const noexcept {
            if (id == provides[0]) {
                return node.name;
            }
            if (id == requires_caps[0]) {
                return std::string_view{irq_cap_name ? irq_cap_name : ""};
            }
            return {};
        }

        static util::Result<void> init_trampoline(void* ctx) noexcept {
            auto* self = static_cast<UartBinding*>(ctx);
            if (!self) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            auto r = uart_init(self->handle, self->config);
            if (!r) return util::unexpected(map_status(r.status));
            r = uart_enable(self->handle);
            if (!r) return util::unexpected(map_status(r.status));
            return {};
        }
    };
}
