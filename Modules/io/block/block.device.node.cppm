module;

#include <array>
#include <span>
#include <string_view>

export module block.device.node;

import block.device;
import block.registry;
import init.binding;
import util.core;
import util.error;

export namespace block {
    template <typename RegistryT>
    struct DeviceBinding {
        Device* dev{nullptr};
        RegistryT* registry{nullptr};
        DeviceDesc desc{};
        const char* registry_cap_name{"block.registry"};
        const char* hal_cap_name{nullptr};
        std::array<init::CapId, 1> provides{};
        std::array<init::CapId, 2> requires_caps{};
        util::usize requires_count{0};
        init::Node node{};

        DeviceBinding(RegistryT& reg,
                      Device& device,
                      const char* endpoint_name,
                      const char* hal_cap_name = nullptr,
                      init::Phase phase = init::Phase::core,
                      util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all)) noexcept
            : dev(&device),
              registry(&reg),
              hal_cap_name(hal_cap_name),
              desc{endpoint_name, cap_id(endpoint_name)} {
            provides = init::capability_ids(endpoint_name);
            requires_caps = init::capability_ids(registry_cap_name, hal_cap_name);
            requires_count = 1;
            if (hal_cap_name) {
                requires_count = 2;
            }
            node = init::make_binding_node(init::capability_name_view(endpoint_name),
                                           phase,
                                           runlevel_mask,
                                           std::span<const init::CapId>(provides.data(), provides.size()),
                                           std::span<const init::CapId>(requires_caps.data(), requires_count),
                                           &DeviceBinding::init_trampoline,
                                           nullptr,
                                           this);
        }

        constexpr std::string_view capability_name(init::CapId id) const noexcept {
            const auto provide_names = init::capability_names(desc.name);
            const auto require_names = init::capability_names(registry_cap_name, hal_cap_name);
            return init::lookup_capability_name(id,
                                                std::span<const init::CapId>(provides.data(), provides.size()),
                                                std::span<const std::string_view>(provide_names.data(), provide_names.size()),
                                                std::span<const init::CapId>(requires_caps.data(), requires_count),
                                                std::span<const std::string_view>(require_names.data(), requires_count));
        }

        static util::Result<void> init_trampoline(void* ctx) noexcept {
            auto* self = static_cast<DeviceBinding*>(ctx);
            if (!self || !self->registry || !self->dev) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if (self->dev->block_size == 0 || self->dev->block_count == 0) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if (!self->dev->read) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if (self->dev->caps == 0) {
                self->dev->caps = caps_from_ops(*self->dev);
            }
            return self->registry->register_device(self->desc, *self->dev);
        }
    };
}
