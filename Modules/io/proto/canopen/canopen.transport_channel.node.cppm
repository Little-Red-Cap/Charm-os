module;

#include <array>
#include <span>

export module canopen.transport_channel.node;

import init.node;
import io.registry;
import canopen.transport_channel;
import canopen.transport;
import util.core;
import util.error;

export namespace canopen {
    template <typename RegistryT, util::usize RxBufSize = 44>
    struct ChannelTransportBinding {
        RegistryT* registry{nullptr};
        const char* io_cap{"io.can0"};
        ChannelTransport<RxBufSize> adapter{};
        Transport transport{};
        std::array<init::CapId, 1> provides{};
        std::array<init::CapId, 2> requires_caps{};
        init::Node node{};

        explicit ChannelTransportBinding(RegistryT& reg,
                                          const char* io_cap_name = "io.can0",
                                          const char* cap_name = "canopen.transport",
                                          const char* registry_cap = "io.registry",
                                          init::Phase phase = init::Phase::core,
                                          util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all)) noexcept
            : registry(&reg), io_cap(io_cap_name), adapter(), transport(adapter.transport()) {
            provides[0] = init::cap_id(cap_name);
            requires_caps[0] = init::cap_id(registry_cap);
            requires_caps[1] = init::cap_id(io_cap_name);
            node = init::Node{
                cap_name,
                phase,
                runlevel_mask,
                std::span<const init::CapId>(provides.data(), provides.size()),
                std::span<const init::CapId>(requires_caps.data(), requires_caps.size()),
                &ChannelTransportBinding::init_trampoline,
                nullptr,
                this
            };
        }

        static util::Result<void> init_trampoline(void* ctx) noexcept {
            auto* self = static_cast<ChannelTransportBinding*>(ctx);
            if (!self || !self->registry) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            auto* ch = self->registry->open_channel(self->io_cap);
            if (!ch) {
                return util::unexpected(util::Errc::noent);
            }
            self->adapter.bind(*ch);
            return {};
        }
    };
}
