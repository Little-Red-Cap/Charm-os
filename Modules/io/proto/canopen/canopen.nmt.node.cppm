module;

#include <array>
#include <span>
#include <string_view>

export module canopen.nmt.node;

import init.binding;
import canopen.nmt_service;
import util.core;
import util.error;

export namespace canopen {
    struct NmtBinding {
        NmtService* service{nullptr};
        const char* transport_cap_name{"canopen.transport"};
        std::array<init::CapId, 1> provides{};
        std::array<init::CapId, 1> requires_caps{};
        init::Node node{};

        explicit NmtBinding(NmtService& service_in,
                            const char* cap_name = "canopen.nmt",
                            const char* transport_cap = "canopen.transport",
                            init::Phase phase = init::Phase::service,
                            util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all)) noexcept
            : service(&service_in), transport_cap_name(transport_cap) {
            provides = init::capability_ids(cap_name);
            requires_caps = init::capability_ids(transport_cap);
            node = init::make_binding_node(init::capability_name_view(cap_name),
                                           phase,
                                           runlevel_mask,
                                           provides,
                                           requires_caps,
                                           &NmtBinding::init_trampoline,
                                           nullptr,
                                           this);
        }

        constexpr std::string_view capability_name(init::CapId id) const noexcept {
            return init::lookup_capability_name(id,
                                                provides,
                                                init::capability_names(node.name),
                                                requires_caps,
                                                init::capability_names(transport_cap_name));
        }

        static util::Result<void> init_trampoline(void* ctx) noexcept {
            auto* self = static_cast<NmtBinding*>(ctx);
            if (!self || !self->service) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            return {};
        }
    };
}
