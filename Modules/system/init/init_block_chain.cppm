module;

export module charm.system.init_block;

import init.node;
import init.plan;
import block.device;
import block.device.node;
#if !defined(CHARM_BAREMETAL)
import block.file.node;
#endif
import block.registry;
import block.sdmmc;
import block.spi_flash;
import util.core;

export namespace charm::system {
    template <typename RegistryT>
    struct BlockInitChain {
        block::DeviceBinding<RegistryT> binding;

        BlockInitChain(RegistryT& registry,
                       block::Device& dev,
                       const char* cap_name,
                       const char* hal_cap = nullptr) noexcept
            : binding(registry, dev, cap_name, hal_cap) {
        }

        constexpr auto plan() const noexcept {
            return init::as_plan(binding);
        }

    };

#if !defined(CHARM_BAREMETAL)
    template <typename RegistryT>
    struct FileInitChain {
        block::FileBinding<RegistryT> binding;

        FileInitChain(RegistryT& registry,
                      const char* path,
                      util::u64 block_size,
                      const char* cap_name = "block.sd0",
                      init::Phase phase = init::Phase::core,
                      util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all)) noexcept
            : binding(registry, path, block_size, cap_name, phase, runlevel_mask) {
        }

        constexpr auto plan() const noexcept {
            return init::as_plan(binding);
        }
    };
#endif

    template <typename RegistryT>
    struct SdmmcInitChain {
        block::SdmmcBinding<RegistryT> binding;

        SdmmcInitChain(RegistryT& registry,
                       block::SdmmcHandle handle,
                       const block::SdmmcConfig& cfg,
                       const char* cap_name = "block.sd0",
                       const char* hal_cap = nullptr,
                       init::Phase phase = init::Phase::core,
                       util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all)) noexcept
            : binding(registry, handle, cfg, cap_name, hal_cap, phase, runlevel_mask) {
        }

        constexpr auto plan() const noexcept {
            return init::as_plan(binding);
        }

    };

    template <typename RegistryT>
    struct SpiFlashInitChain {
        block::SpiFlashBinding<RegistryT> binding;

        SpiFlashInitChain(RegistryT& registry,
                          block::SpiFlashHandle handle,
                          const block::SpiFlashConfig& cfg,
                          const char* cap_name = "block.flash0",
                          const char* hal_cap = nullptr,
                          init::Phase phase = init::Phase::core,
                          util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all)) noexcept
            : binding(registry, handle, cfg, cap_name, hal_cap, phase, runlevel_mask) {
        }

        constexpr auto plan() const noexcept {
            return init::as_plan(binding);
        }

    };
}
