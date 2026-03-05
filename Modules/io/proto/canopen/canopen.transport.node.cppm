module;

#include <array>
#include <span>

export module canopen.transport.node;

import init.node;
import canopen.transport;
import util.core;
import util.error;

export namespace canopen {
    struct TransportBinding {
        Transport* transport{nullptr};
        std::array<init::CapId, 1> provides{};
        init::Node node{};

        explicit TransportBinding(Transport& transport_in,
                                  const char* cap_name = "canopen.transport",
                                  init::Phase phase = init::Phase::core,
                                  util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all)) noexcept
            : transport(&transport_in) {
            provides[0] = init::cap_id(cap_name);
            node = init::Node{
                cap_name,
                phase,
                runlevel_mask,
                std::span<const init::CapId>(provides.data(), provides.size()),
                {},
                &TransportBinding::init_trampoline,
                nullptr,
                this
            };
        }

        static util::Result<void> init_trampoline(void* ctx) noexcept {
            auto* self = static_cast<TransportBinding*>(ctx);
            if (!self || !self->transport) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if (!self->transport->ops.recv || !self->transport->ops.send) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            return {};
        }
    };
}
