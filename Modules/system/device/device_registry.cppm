module;

#include <array>
#include <cstddef>

export module device.registry;

import util.core;
import device.desc;
import device.driver;
import device.types;

export namespace device {
    struct RegistryBase {
        virtual ~RegistryBase() = default;
        virtual bool add_device(const DeviceDesc& desc, void* ctx = nullptr) noexcept = 0;
    };

    template <util::usize MaxDevices, util::usize MaxDrivers>
    class Registry : public RegistryBase {
    public:
        [[nodiscard]] constexpr util::usize device_count() const noexcept { return device_count_; }
        [[nodiscard]] constexpr util::usize driver_count() const noexcept { return driver_count_; }
        Device& device_at(util::usize idx) noexcept { return devices_[idx]; }
        const Device& device_at(util::usize idx) const noexcept { return devices_[idx]; }

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
                select_driver(dev);
                if (!dev.driver) continue;
                if (!dispatch(dev, DeviceEvent::probe)) {
                    dev.driver = nullptr;
                    continue;
                }
                if (!dispatch(dev, DeviceEvent::init)) {
                    dispatch(dev, DeviceEvent::remove);
                    dev.driver = nullptr;
                    dev.state = DeviceState::detected;
                    continue;
                }
                (void)dispatch(dev, DeviceEvent::start);
            }
        }

        void shutdown_all() noexcept {
            for (util::usize i = 0; i < device_count_; ++i) {
                (void)dispatch(devices_[i], DeviceEvent::shutdown);
            }
        }

        void remove_all() noexcept {
            for (util::usize i = 0; i < device_count_; ++i) {
                (void)dispatch(devices_[i], DeviceEvent::remove);
            }
        }

        void init_all() noexcept {
            match_all();
        }

        void suspend_all() noexcept {
            for (util::usize i = 0; i < device_count_; ++i) {
                (void)dispatch(devices_[i], DeviceEvent::suspend);
            }
        }

        void resume_all() noexcept {
            for (util::usize i = 0; i < device_count_; ++i) {
                (void)dispatch(devices_[i], DeviceEvent::resume);
            }
        }

        bool dispatch(Device& dev, DeviceEvent ev) noexcept {
            if (ev == DeviceEvent::attach) {
                dev.state = DeviceState::detected;
            }
            if (!dev.driver) return false;
            bool ok = true;
            switch (ev) {
            case DeviceEvent::probe:
                if (dev.driver->ops.probe) ok = dev.driver->ops.probe(dev);
                break;
            case DeviceEvent::init:
                if (dev.driver->ops.init) ok = dev.driver->ops.init(dev);
                if (ok) dev.state = DeviceState::initialized;
                break;
            case DeviceEvent::start:
                dev.state = DeviceState::running;
                break;
            case DeviceEvent::suspend:
                if (dev.driver->ops.suspend) ok = dev.driver->ops.suspend(dev);
                if (ok) dev.state = DeviceState::suspended;
                break;
            case DeviceEvent::resume:
                if (dev.driver->ops.resume) ok = dev.driver->ops.resume(dev);
                if (ok) dev.state = DeviceState::running;
                break;
            case DeviceEvent::shutdown:
                if (dev.driver->ops.shutdown) dev.driver->ops.shutdown(dev);
                dev.state = DeviceState::stopped;
                break;
            case DeviceEvent::remove:
                if (dev.driver->ops.remove) dev.driver->ops.remove(dev);
                dev.driver = nullptr;
                dev.match_score = 0;
                dev.state = DeviceState::detected;
                break;
            case DeviceEvent::error:
                dev.state = DeviceState::stopped;
                ok = false;
                break;
            case DeviceEvent::attach:
            default:
                break;
            }
            if (dev.driver && dev.driver->ops.on_event) {
                dev.driver->ops.on_event(dev, ev);
            }
            return ok;
        }

    private:
        struct MatchResult {
            const Driver* drv{nullptr};
            util::u32 score{0};
        };

        static util::u32 match_score(const DeviceDesc& dev, const DeviceDesc& drv) noexcept {
            util::u32 score = 0;
            if (drv.class_id) {
                if (drv.class_id != dev.class_id) return 0;
                score += 4;
            }
            if (drv.vendor_id) {
                if (drv.vendor_id != dev.vendor_id) return 0;
                score += 3;
            }
            if (drv.product_id) {
                if (drv.product_id != dev.product_id) return 0;
                score += 2;
            }
            if (!drv.type.empty()) {
                if (drv.type.compare(dev.type) != 0) return 0;
                score += 1;
            }
            return score;
        }

        void select_driver(Device& dev) noexcept {
            MatchResult best{};
            for (util::usize j = 0; j < driver_count_; ++j) {
                const auto* drv = drivers_[j];
                if (!drv) continue;
                const util::u32 score = match_score(dev.desc, drv->match);
                if (score == 0 && drv->match.class_id == 0 && drv->match.vendor_id == 0 &&
                    drv->match.product_id == 0 && drv->match.type.empty()) {
                    // Generic driver, score stays 0.
                } else if (score == 0) {
                    continue;
                }
                if (!best.drv || score > best.score ||
                    (score == best.score && drv->priority > best.drv->priority)) {
                    best.drv = drv;
                    best.score = score;
                }
            }
            dev.driver = best.drv;
            dev.match_score = best.score;
        }

        std::array<Device, MaxDevices> devices_{};
        std::array<const Driver*, MaxDrivers> drivers_{};
        util::usize device_count_{0};
        util::usize driver_count_{0};
    };
}
