module;

#include <array>
#include <span>

export module block.device.node;

import block.device;
import block.registry;
import init.node;
import util.core;
import util.error;

export namespace block {
    template <typename RegistryT>
    struct DeviceBinding {
        Device* dev{nullptr};
        RegistryT* registry{nullptr};
        DeviceDesc desc{};
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
              desc{endpoint_name, cap_id(endpoint_name)} {
            provides[0] = init::cap_id(endpoint_name);
            requires_caps[0] = init::cap_id("block.registry");
            requires_count = 1;
            if (hal_cap_name) {
                requires_caps[1] = init::cap_id(hal_cap_name);
                requires_count = 2;
            }
            node = init::Node{
                endpoint_name,
                phase,
                runlevel_mask,
                std::span<const init::CapId>(provides.data(), provides.size()),
                std::span<const init::CapId>(requires_caps.data(), requires_count),
                &DeviceBinding::init_trampoline,
                nullptr,
                this
            };
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
            return self->registry->register_device(self->desc, *self->dev);
        }
    };
}
