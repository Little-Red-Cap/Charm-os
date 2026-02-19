module;

export module hal_device_examples;

// Example helpers for wiring HAL devices into the device registry.

import device.driver;
import device.registry;
import hal_device;
import util.core;

export namespace hal::examples {
    template <util::usize MaxDevices, util::usize MaxDrivers>
    inline bool register_spi(device::Registry<MaxDevices, MaxDrivers>& reg,
                             HalDeviceHook& hook,
                             const char* type,
                             const char* name,
                             util::u32 priority) noexcept {
        const auto desc = make_hal_desc(type);
        static device::Driver driver{};
        driver = make_hal_driver(&hook, desc, name, priority);
        return reg.add_device(desc, &hook) && reg.add_driver(driver);
    }

    template <util::usize MaxDevices, util::usize MaxDrivers>
    inline bool register_i2c(device::Registry<MaxDevices, MaxDrivers>& reg,
                             HalDeviceHook& hook,
                             const char* type,
                             const char* name,
                             util::u32 priority) noexcept {
        const auto desc = make_hal_desc(type);
        static device::Driver driver{};
        driver = make_hal_driver(&hook, desc, name, priority);
        return reg.add_device(desc, &hook) && reg.add_driver(driver);
    }
}
