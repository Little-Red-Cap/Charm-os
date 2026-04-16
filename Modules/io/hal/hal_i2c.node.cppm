module;

#include <array>
#include <span>
#include <string_view>

export module hal_i2c.node;

import hal_core;
import hal_i2c;
import init.binding;
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
        const char* irq_cap_name{"platform.irq"};
        std::array<init::CapId, 1> provides{};
        std::array<init::CapId, 1> requires_caps{};
        init::Node node{};

        I2cBinding(I2cIoHandle h,
                   const I2cConfig& cfg,
                   const char* cap_name = "hal.i2c1",
                   const char* irq_cap_name = "platform.irq",
                   init::Phase phase = init::Phase::core,
                   util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all)) noexcept
            : handle(h), config(cfg), irq_cap_name(irq_cap_name) {
            provides = init::capability_ids(cap_name);
            requires_caps = init::capability_ids(irq_cap_name);
            node = init::make_binding_node(init::capability_name_view(cap_name),
                                           phase,
                                           runlevel_mask,
                                           provides,
                                           requires_caps,
                                           &I2cBinding::init_trampoline,
                                           nullptr,
                                           this);
        }

        constexpr std::string_view capability_name(init::CapId id) const noexcept {
            return init::lookup_capability_name(id,
                                                provides,
                                                init::capability_names(node.name),
                                                requires_caps,
                                                init::capability_names(irq_cap_name));
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
