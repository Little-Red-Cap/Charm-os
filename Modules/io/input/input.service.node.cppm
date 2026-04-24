module;

#include <array>
#include <span>
#include <string_view>

export module input.service.node;

import init.binding;
import input.service;
import charm.system.clock;
import util.core;
import util.error;

export namespace input {
    struct ServiceBinding {
        InputService* service{nullptr};
        charm::system::Clock* clock{nullptr};
        const char* clock_cap_name{"system.clock"};
        std::array<init::CapId, 1> provides{};
        std::array<init::CapId, 1> requires_caps{};
        init::Node node{};

        explicit ServiceBinding(InputService& svc,
                                charm::system::Clock& clock_in,
                                const char* cap_name = "input.service",
                                const char* clock_cap_name = "system.clock",
                                init::Phase phase = init::Phase::core,
                                util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all)) noexcept
            : service(&svc), clock(&clock_in), clock_cap_name(clock_cap_name) {
            provides = init::capability_ids(cap_name);
            requires_caps = init::capability_ids(clock_cap_name);
            node = init::make_binding_node(init::capability_name_view(cap_name),
                                           phase,
                                           runlevel_mask,
                                           provides,
                                           requires_caps,
                                           &ServiceBinding::init_trampoline,
                                           nullptr,
                                           this);
        }

        constexpr std::string_view capability_name(init::CapId id) const noexcept {
            return init::lookup_capability_name(id,
                                                provides,
                                                init::capability_names(node.name),
                                                requires_caps,
                                                init::capability_names(clock_cap_name));
        }

        static util::Result<void> init_trampoline(void* ctx) noexcept {
            auto* self = static_cast<ServiceBinding*>(ctx);
            if (!self || !self->service || !self->clock) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            self->service->set_clock(*self->clock);
            return {};
        }
    };
}
