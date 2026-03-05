module;

#include <array>
#include <span>

export module charm.system.init_block;

import init.node;
import init.graph;
import block.device;
import block.device.node;
import block.registry;
import util.core;
import util.error;

export namespace charm::system {
    template <typename RegistryT>
    struct BlockInitChain {
        block::DeviceBinding<RegistryT> binding;
        std::array<const init::Node*, 1> nodes{};

        BlockInitChain(RegistryT& registry,
                       block::Device& dev,
                       const char* cap_name,
                       const char* hal_cap = nullptr) noexcept
            : binding(registry, dev, cap_name, hal_cap) {
            nodes = {&binding.node};
        }

        std::span<const init::Node* const> node_span() const noexcept {
            return std::span<const init::Node* const>(nodes.data(), nodes.size());
        }

        template <util::usize MaxNodes, util::usize MaxCaps>
        util::Result<void> build(init::Graph<MaxNodes, MaxCaps>& graph,
                                 util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all),
                                 init::Phase max_phase = init::Phase::app) noexcept {
            return graph.build(node_span(), runlevel_mask, max_phase);
        }
    };
}
