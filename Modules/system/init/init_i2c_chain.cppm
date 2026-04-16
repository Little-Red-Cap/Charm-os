module;

export module charm.system.init_i2c;

import init.node;
import init.plan;
import hal_i2c;
import hal_i2c.node;
import util.core;

export namespace charm::system {
    struct I2cInitChain {
        hal::I2cBinding i2c_binding;

        I2cInitChain(hal::I2cIoHandle handle,
                     const hal::I2cConfig& cfg,
                     const char* hal_cap = "hal.i2c1",
                     const char* irq_cap = "platform.irq") noexcept
            : i2c_binding(handle, cfg, hal_cap, irq_cap) {
        }

        constexpr auto plan() const noexcept {
            return init::as_plan(i2c_binding);
        }

    };
}
