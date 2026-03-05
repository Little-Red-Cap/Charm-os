module;

#include <array>
#include <span>

export module canopen.nmt.node;

import init.node;
import canopen.nmt_service;
import util.core;
import util.error;

export namespace canopen {
    struct NmtBinding {
        NmtService* service{nullptr};
        std::array<init::CapId, 1> provides{};
        std::array<init::CapId, 1> requires_caps{};
        init::Node node{};

        explicit NmtBinding(NmtService& service_in,
                            const char* cap_name = "canopen.nmt",
                            const char* transport_cap = "canopen.transport",
                            init::Phase phase = init::Phase::service,
                            util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all)) noexcept
            : service(&service_in) {
            provides[0] = init::cap_id(cap_name);
            requires_caps[0] = init::cap_id(transport_cap);
            node = init::Node{
                cap_name,
                phase,
                runlevel_mask,
                std::span<const init::CapId>(provides.data(), provides.size()),
                std::span<const init::CapId>(requires_caps.data(), requires_caps.size()),
                &NmtBinding::init_trampoline,
                nullptr,
                this
            };
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
