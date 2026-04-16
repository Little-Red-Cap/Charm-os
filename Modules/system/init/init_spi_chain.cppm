module;

export module charm.system.init_spi;

import init.node;
import init.plan;
import hal_spi;
import hal_spi.node;
import util.core;

export namespace charm::system {
    struct SpiInitChain {
        hal::SpiBinding spi_binding;

        SpiInitChain(hal::SpiIoHandle handle,
                     const hal::SpiConfig& cfg,
                     const char* hal_cap = "hal.spi1",
                     const char* irq_cap = "platform.irq") noexcept
            : spi_binding(handle, cfg, hal_cap, irq_cap) {
        }

        constexpr auto plan() const noexcept {
            return init::as_plan(spi_binding);
        }

    };
}
