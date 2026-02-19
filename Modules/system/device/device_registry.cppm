module;

#include <array>
#include <cstddef>

export module device.registry;

import util.core;
import device.desc;
import device.driver;

export namespace device {
    enum class DeviceState : util::u8 {
        detected,
        initialized,
        running,
        suspended,
        stopped
    };

    struct Device {
        DeviceDesc desc{};
        void* ctx{nullptr};
        DeviceState state{DeviceState::detected};
        const Driver* driver{nullptr};
    };

    struct RegistryBase {
        virtual ~RegistryBase() = default;
        virtual bool add_device(const DeviceDesc& desc, void* ctx = nullptr) noexcept = 0;
    };

    template <util::usize MaxDevices, util::usize MaxDrivers>
    class Registry : public RegistryBase {
    public:
        [[nodiscard]] constexpr util::usize device_count() const noexcept { return device_count_; }
        [[nodiscard]] constexpr util::usize driver_count() const noexcept { return driver_count_; }

        bool add_device(const DeviceDesc& desc, void* ctx = nullptr) noexcept override {
            if (device_count_ >= MaxDevices) return false;
            devices_[device_count_++] = Device{desc, ctx, DeviceState::detected, nullptr};
            return true;
        }

        bool add_driver(const Driver& drv) noexcept {
            if (driver_count_ >= MaxDrivers) return false;
            drivers_[driver_count_++] = &drv;
            return true;
        }

        void clear() noexcept {
            device_count_ = 0;
            driver_count_ = 0;
            for (auto& d : devices_) d = {};
            for (auto& r : drivers_) r = nullptr;
        }

        void match_all() noexcept {
            for (util::usize i = 0; i < device_count_; ++i) {
                auto& dev = devices_[i];
                for (util::usize j = 0; j < driver_count_; ++j) {
                    const auto* drv = drivers_[j];
                    if (!drv) continue;
                    if (!match(dev.desc, drv->match)) continue;
                    dev.driver = drv;
                    if (drv->ops.probe && !drv->ops.probe(dev)) {
                        dev.driver = nullptr;
                        continue;
                    }
                    if (drv->ops.init) {
                        dev.state = DeviceState::initialized;
                        if (!drv->ops.init(dev)) {
                            dev.state = DeviceState::detected;
                            dev.driver = nullptr;
                            continue;
                        }
                    }
                    dev.state = DeviceState::running;
                    break;
                }
            }
        }

        void shutdown_all() noexcept {
            for (util::usize i = 0; i < device_count_; ++i) {
                auto& dev = devices_[i];
                if (dev.driver && dev.driver->ops.shutdown) {
                    dev.driver->ops.shutdown(dev);
                }
                dev.state = DeviceState::stopped;
            }
        }

        void remove_all() noexcept {
            for (util::usize i = 0; i < device_count_; ++i) {
                auto& dev = devices_[i];
                if (dev.driver && dev.driver->ops.remove) {
                    dev.driver->ops.remove(dev);
                }
                dev.driver = nullptr;
                dev.state = DeviceState::detected;
            }
        }

        void init_all() noexcept {
            match_all();
        }

        void suspend_all() noexcept {
            for (util::usize i = 0; i < device_count_; ++i) {
                auto& dev = devices_[i];
                if (dev.driver && dev.driver->ops.suspend) {
                    if (dev.driver->ops.suspend(dev)) {
                        dev.state = DeviceState::suspended;
                    }
                }
            }
        }

        void resume_all() noexcept {
            for (util::usize i = 0; i < device_count_; ++i) {
                auto& dev = devices_[i];
                if (dev.driver && dev.driver->ops.resume) {
                    if (dev.driver->ops.resume(dev)) {
                        dev.state = DeviceState::running;
                    }
                }
            }
        }

    private:
        static bool match(const DeviceDesc& dev, const DeviceDesc& drv) noexcept {
            if (drv.class_id && drv.class_id != dev.class_id) return false;
            if (drv.vendor_id && drv.vendor_id != dev.vendor_id) return false;
            if (drv.product_id && drv.product_id != dev.product_id) return false;
            if (!drv.type.empty() && drv.type != dev.type) return false;
            return true;
        }

        std::array<Device, MaxDevices> devices_{};
        std::array<const Driver*, MaxDrivers> drivers_{};
        util::usize device_count_{0};
        util::usize driver_count_{0};
    };
}
