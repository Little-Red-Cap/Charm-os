module;

#include <array>
#include <span>

export module charm.system.init_block;

import init.node;
import init.graph;
import block.device;
import block.device.node;
import block.file.node;
import block.registry;
import block.sdmmc;
import block.spi_flash;
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

    template <typename RegistryT>
    struct FileInitChain {
        block::FileBinding<RegistryT> binding;
        std::array<const init::Node*, 1> nodes{};

        FileInitChain(RegistryT& registry,
                      const char* path,
                      util::u64 block_size,
                      const char* cap_name = "block.sd0",
                      init::Phase phase = init::Phase::core,
                      util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all)) noexcept
            : binding(registry, path, block_size, cap_name, phase, runlevel_mask) {
            nodes = {&binding.node};
        }

        std::span<const init::Node* const> node_span() const noexcept {
            return std::span<const init::Node* const>(nodes.data(), nodes.size());
        }
    };

    template <typename RegistryT>
    struct SdmmcInitChain {
        block::SdmmcBinding<RegistryT> binding;
        std::array<const init::Node*, 1> nodes{};

        SdmmcInitChain(RegistryT& registry,
                       block::SdmmcHandle handle,
                       const block::SdmmcConfig& cfg,
                       const char* cap_name = "block.sd0",
                       const char* hal_cap = nullptr,
                       init::Phase phase = init::Phase::core,
                       util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all)) noexcept
            : binding(registry, handle, cfg, cap_name, hal_cap, phase, runlevel_mask) {
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

    template <typename RegistryT>
    struct SpiFlashInitChain {
        block::SpiFlashBinding<RegistryT> binding;
        std::array<const init::Node*, 1> nodes{};

        SpiFlashInitChain(RegistryT& registry,
                          block::SpiFlashHandle handle,
                          const block::SpiFlashConfig& cfg,
                          const char* cap_name = "block.flash0",
                          const char* hal_cap = nullptr,
                          init::Phase phase = init::Phase::core,
                          util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all)) noexcept
            : binding(registry, handle, cfg, cap_name, hal_cap, phase, runlevel_mask) {
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
