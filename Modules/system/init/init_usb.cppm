module;

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

        UsbMscBlockInitChain(RegistryT& registry,
                             const usb::device::MscBlockDesc& desc,
                             init::Phase phase = init::Phase::core,
                             util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all)) noexcept
            : binding(registry, desc, phase, runlevel_mask) {
        }

        constexpr auto plan() const noexcept {
            return init::as_plan(binding);
        }
    };
}
