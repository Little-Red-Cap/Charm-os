module;

#include <array>
#include <span>
#include <string_view>

export module canopen.transport.node;

import init.binding;
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
            provides = init::capability_ids(cap_name);
            node = init::make_binding_node(init::capability_name_view(cap_name),
                                           phase,
                                           runlevel_mask,
                                           provides,
                                           &TransportBinding::init_trampoline,
                                           nullptr,
                                           this);
        }

        constexpr std::string_view capability_name(init::CapId id) const noexcept {
            return init::lookup_capability_name(id,
                                                provides,
                                                init::capability_names(node.name));
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
