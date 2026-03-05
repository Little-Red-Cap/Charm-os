module;

#include <array>
#include <span>

export module io.channel.node;

import init.node;
import io.channel;
import io.reactor;
import io.registry;
import util.core;
import util.error;

export namespace io {
    template <typename RegistryT>
    struct ChannelBinding {
        RegistryT* registry{nullptr};
        Channel* channel{nullptr};
        Reactor* reactor{nullptr};
        EndpointDesc desc{};
        std::array<init::CapId, 1> provides{};
        std::array<init::CapId, 1> requires_caps{};
        init::Node node{};

        ChannelBinding(RegistryT& reg,
                       Channel& ch,
                       EndpointDesc desc_in,
                       Reactor* reactor_in = nullptr,
                       const char* registry_cap = "io.registry",
                       init::Phase phase = init::Phase::core,
                       util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all)) noexcept
            : registry(&reg), channel(&ch), reactor(reactor_in), desc(desc_in) {
            provides[0] = init::cap_id(desc.name);
            requires_caps[0] = init::cap_id(registry_cap);
            node = init::Node{
                desc.name.data(),
                phase,
                runlevel_mask,
                std::span<const init::CapId>(provides.data(), provides.size()),
                std::span<const init::CapId>(requires_caps.data(), requires_caps.size()),
                &ChannelBinding::init_trampoline,
                nullptr,
                this
            };
        }

        static util::Result<void> init_trampoline(void* ctx) noexcept {
            auto* self = static_cast<ChannelBinding*>(ctx);
            if (!self || !self->registry || !self->channel) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if (self->desc.name.empty() || self->desc.cap == 0) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            return self->registry->register_channel(self->desc, *self->channel, self->reactor);
        }
    };
}
