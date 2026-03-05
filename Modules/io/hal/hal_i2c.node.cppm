module;

#include <array>
#include <span>

export module hal_i2c.node;

import hal_core;
import hal_i2c;
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

    struct I2cBinding {
        I2cIoHandle handle{};
        I2cConfig config{};
        std::array<init::CapId, 1> provides{};
        std::array<init::CapId, 1> requires_caps{};
        init::Node node{};

        I2cBinding(I2cIoHandle h,
                   const I2cConfig& cfg,
                   const char* cap_name = "hal.i2c1",
                   const char* irq_cap_name = "platform.irq",
                   init::Phase phase = init::Phase::core,
                   util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all)) noexcept
            : handle(h), config(cfg) {
            provides[0] = init::cap_id(cap_name);
            requires_caps[0] = init::cap_id(irq_cap_name);
            node = init::Node{
                cap_name,
                phase,
                runlevel_mask,
                std::span<const init::CapId>(provides.data(), provides.size()),
                std::span<const init::CapId>(requires_caps.data(), requires_caps.size()),
                &I2cBinding::init_trampoline,
                nullptr,
                this
            };
        }

        static util::Result<void> init_trampoline(void* ctx) noexcept {
            auto* self = static_cast<I2cBinding*>(ctx);
            if (!self) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            auto r = i2c_init(self->handle, self->config);
            if (!r) return util::unexpected(map_status(r.status));
            r = i2c_enable(self->handle);
            if (!r) return util::unexpected(map_status(r.status));
            return {};
        }
    };
}
