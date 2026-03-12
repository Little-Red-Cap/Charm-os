module;

#include <array>
#include <span>

export module charm.system.init_usb;

import init.node;
import usb.class_msc_block.node;
import block.registry;
import util.core;

export namespace charm::system {
    template <typename RegistryT>
    struct UsbMscBlockInitChain {
        usb::device::MscBlockBinding<RegistryT> binding;
        std::array<const init::Node*, 1> nodes{};

        UsbMscBlockInitChain(RegistryT& registry,
                             const usb::device::MscBlockDesc& desc,
                             init::Phase phase = init::Phase::core,
                             util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all)) noexcept
            : binding(registry, desc, phase, runlevel_mask) {
            nodes = {&binding.node};
        }

        std::span<const init::Node* const> node_span() const noexcept {
            return std::span<const init::Node* const>(nodes.data(), nodes.size());
        }
    };
}
