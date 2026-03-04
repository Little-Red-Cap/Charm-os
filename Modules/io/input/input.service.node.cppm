module;

#include <array>
#include <span>

export module input.service.node;

import init.node;
import input.service;
import util.core;
import util.error;

export namespace input {
    struct ServiceBinding {
        InputService* service{nullptr};
        std::array<init::CapId, 1> provides{};
        std::array<init::CapId, 1> requires_caps{};
        init::Node node{};

        explicit ServiceBinding(InputService& svc,
                                const char* cap_name = "input.service",
                                const char* clock_cap_name = "system.clock",
                                init::Phase phase = init::Phase::core,
                                util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all)) noexcept
            : service(&svc) {
            provides[0] = init::cap_id(cap_name);
            requires_caps[0] = init::cap_id(clock_cap_name);
            node = init::Node{
                cap_name,
                phase,
                runlevel_mask,
                std::span<const init::CapId>(provides.data(), provides.size()),
                std::span<const init::CapId>(requires_caps.data(), requires_caps.size()),
                &ServiceBinding::init_trampoline,
                nullptr,
                this
            };
        }

        static util::Result<void> init_trampoline(void* ctx) noexcept {
            auto* self = static_cast<ServiceBinding*>(ctx);
            if (!self || !self->service) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if (g_service && g_service != self->service) {
                return util::unexpected(util::Errc::exist);
            }
            set_service(*self->service);
            return {};
        }
    };
}
