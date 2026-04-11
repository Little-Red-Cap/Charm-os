module;

#include <array>
#include <span>

export module charm.system.init_usb;

import init.node;
import init.plan;
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

        constexpr auto plan() const noexcept {
            return init::as_plan(binding);
        }

        template <typename Fn>
        constexpr void for_each_legacy_node(Fn&& fn) const noexcept {
            for (util::usize i = 0; i < nodes.size(); ++i) {
                if (nodes[i]) {
                    fn(*nodes[i]);
                }
            }
        }

        [[deprecated("use plan() or build_graph(...) instead of node_span()")]]
        std::span<const init::Node* const> node_span() const noexcept {
            return std::span<const init::Node* const>(nodes.data(), nodes.size());
        }
    };
}
