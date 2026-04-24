module;

#include <array>
#include <span>
#include <string_view>

export module io.channel.node;

import init.binding;
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
        const char* registry_cap_name{"io.registry"};
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
            : registry(&reg), channel(&ch), reactor(reactor_in), desc(desc_in), registry_cap_name(registry_cap) {
            provides = init::capability_ids(desc.name);
            requires_caps = init::capability_ids(registry_cap);
            node = init::make_binding_node(desc.name,
                                           phase,
                                           runlevel_mask,
                                           provides,
                                           requires_caps,
                                           &ChannelBinding::init_trampoline,
                                           nullptr,
                                           this);
        }

        constexpr std::string_view capability_name(init::CapId id) const noexcept {
            return init::lookup_capability_name(id,
                                                provides,
                                                init::capability_names(desc.name),
                                                requires_caps,
                                                init::capability_names(registry_cap_name));
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

    template <typename RegistryT>
    struct ChannelAliasBinding {
        RegistryT* registry{nullptr};
        Reactor* reactor{nullptr};
        const char* target_cap{"io.uart1"};
        EndpointDesc desc{};
        const char* registry_cap_name{"io.registry"};
        std::array<init::CapId, 1> provides{};
        std::array<init::CapId, 2> requires_caps{};
        init::Node node{};

        ChannelAliasBinding(RegistryT& reg,
                            Reactor& reactor_in,
                            const char* target_cap_name,
                            const char* alias_name,
                            const char* registry_cap = "io.registry",
                            init::Phase phase = init::Phase::core,
                            util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all)) noexcept
            : registry(&reg),
              reactor(&reactor_in),
              target_cap(target_cap_name),
              desc{alias_name,
                   io::cap_id(alias_name),
                   EndpointKind::channel,
                   EndpointCaps::duplex},
              registry_cap_name(registry_cap) {
            provides = init::capability_ids(alias_name);
            requires_caps = init::capability_ids(registry_cap, target_cap_name);
            node = init::make_binding_node(init::capability_name_view(alias_name),
                                           phase,
                                           runlevel_mask,
                                           provides,
                                           requires_caps,
                                           &ChannelAliasBinding::init_trampoline,
                                           nullptr,
                                           this);
        }

        constexpr std::string_view capability_name(init::CapId id) const noexcept {
            return init::lookup_capability_name(id,
                                                provides,
                                                init::capability_names(desc.name),
                                                requires_caps,
                                                init::capability_names(registry_cap_name,
                                                                       target_cap));
        }

        static util::Result<void> init_trampoline(void* ctx) noexcept {
            auto* self = static_cast<ChannelAliasBinding*>(ctx);
            if (!self || !self->registry || !self->reactor || !self->target_cap) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if (self->desc.name.empty() || self->desc.cap == 0) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if (self->registry->find_channel(self->desc.name) || self->registry->find_channel(self->desc.cap)) {
                return {};
            }
            auto* ch = self->registry->open_channel(io::cap_id(self->target_cap));
            if (!ch) {
                return util::unexpected(util::Errc::noent);
            }
            return self->registry->register_channel(self->desc, *ch, self->reactor);
        }
    };
}
