module;

#include <array>
#include <cstddef>

export module device.registry;

import util.core;
import util.error;
import device.desc;
import device.types;

export namespace device {
    struct RegistryBase {
        virtual ~RegistryBase() = default;
        [[nodiscard]] virtual util::Result<void> try_add_device(const DeviceDesc& desc,
                                                                void* ctx = nullptr) noexcept = 0;

        virtual bool add_device(const DeviceDesc& desc, void* ctx = nullptr) noexcept {
            return static_cast<bool>(try_add_device(desc, ctx));
        }
    };

    template <util::usize MaxDevices, util::usize MaxDrivers>
    class Registry : public RegistryBase {
    public:
        [[nodiscard]] constexpr util::usize device_count() const noexcept { return device_count_; }
        [[nodiscard]] constexpr util::usize driver_count() const noexcept { return driver_count_; }
        Device& device_at(util::usize idx) noexcept { return devices_[idx]; }
        const Device& device_at(util::usize idx) const noexcept { return devices_[idx]; }

        [[nodiscard]] util::usize find_device_index(const DeviceDesc& desc,
                                                    void* ctx = nullptr) const noexcept {
            for (util::usize i = 0; i < device_count_; ++i) {
                const auto& dev = devices_[i];
                if (!desc_equal(dev.desc, desc)) continue;
                if (ctx != nullptr && dev.ctx != ctx) continue;
                return i;
            }
            return device_count_;
        }

        Device* find_device(const DeviceDesc& desc, void* ctx = nullptr) noexcept {
            const auto index = find_device_index(desc, ctx);
            return index < device_count_ ? &devices_[index] : nullptr;
        }

        const Device* find_device(const DeviceDesc& desc, void* ctx = nullptr) const noexcept {
            const auto index = find_device_index(desc, ctx);
            return index < device_count_ ? &devices_[index] : nullptr;
        }

        [[nodiscard]] util::Result<void> try_add_device(const DeviceDesc& desc,
                                                        void* ctx = nullptr) noexcept override {
            if (find_exact_device_index(desc, ctx) < device_count_) {
                return {};
            }
            if (device_count_ >= MaxDevices) {
                return util::unexpected(util::Errc::buffer_overflow);
            }
            devices_[device_count_++] = Device{desc, ctx, DeviceState::detected, nullptr};
            return {};
        }

        bool add_device(const DeviceDesc& desc, void* ctx = nullptr) noexcept override {
            return static_cast<bool>(try_add_device(desc, ctx));
        }

        [[nodiscard]] util::Result<void> try_remove_device(util::usize index) noexcept {
            if (index >= device_count_) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if (devices_[index].driver) {
                (void)dispatch(devices_[index], DeviceEvent::remove);
            }
            for (util::usize i = index + 1; i < device_count_; ++i) {
                devices_[i - 1] = devices_[i];
            }
            if (device_count_ > 0) {
                devices_[device_count_ - 1] = {};
                --device_count_;
            }
            return {};
        }

        bool remove_device(util::usize index) noexcept {
            return static_cast<bool>(try_remove_device(index));
        }

        [[nodiscard]] util::Result<void> try_remove_matching(const DeviceDesc& desc,
                                                             void* ctx = nullptr) noexcept {
            const auto index = find_device_index(desc, ctx);
            if (index >= device_count_) {
                return util::unexpected(util::Errc::noent);
            }
            return try_remove_device(index);
        }

        bool remove_matching(const DeviceDesc& desc, void* ctx = nullptr) noexcept {
            return static_cast<bool>(try_remove_matching(desc, ctx));
        }

        [[nodiscard]] util::Result<void> try_add_driver(const Driver& drv) noexcept {
            if (driver_count_ >= MaxDrivers) {
                return util::unexpected(util::Errc::buffer_overflow);
            }
            drivers_[driver_count_++] = &drv;
            return {};
        }

        bool add_driver(const Driver& drv) noexcept {
            return static_cast<bool>(try_add_driver(drv));
        }

        void clear() noexcept {
            device_count_ = 0;
            driver_count_ = 0;
            for (auto& d : devices_) d = {};
            for (auto& r : drivers_) r = nullptr;
        }

        [[nodiscard]] util::Result<void> try_match_all() noexcept {
            util::Errc first_error = util::Errc::ok;
            for (util::usize i = 0; i < device_count_; ++i) {
                note_first_error(first_error, try_match_device(devices_[i]));
            }
            return finish_error(first_error);
        }

        void match_all() noexcept {
            (void)try_match_all();
        }

        [[nodiscard]] util::Result<void> try_match_detected() noexcept {
            util::Errc first_error = util::Errc::ok;
            for (util::usize i = 0; i < device_count_; ++i) {
                auto& dev = devices_[i];
                if (dev.state != DeviceState::detected) continue;
                if (dev.driver != nullptr) continue;
                note_first_error(first_error, try_match_device(dev));
            }
            return finish_error(first_error);
        }

        void match_detected() noexcept {
            (void)try_match_detected();
        }

        [[nodiscard]] util::Result<void> try_shutdown_all() noexcept {
            return try_dispatch_bound_all(DeviceEvent::shutdown);
        }

        void shutdown_all() noexcept {
            (void)try_shutdown_all();
        }

        [[nodiscard]] util::Result<void> try_remove_all() noexcept {
            return try_dispatch_bound_all(DeviceEvent::remove);
        }

        void remove_all() noexcept {
            (void)try_remove_all();
        }

        [[nodiscard]] util::Result<void> try_init_all() noexcept {
            return try_match_all();
        }

        void init_all() noexcept {
            (void)try_init_all();
        }

        [[nodiscard]] util::Result<void> try_suspend_all() noexcept {
            return try_dispatch_bound_all(DeviceEvent::suspend);
        }

        void suspend_all() noexcept {
            (void)try_suspend_all();
        }

        [[nodiscard]] util::Result<void> try_resume_all() noexcept {
            return try_dispatch_bound_all(DeviceEvent::resume);
        }

        void resume_all() noexcept {
            (void)try_resume_all();
        }

        [[nodiscard]] util::Result<void> try_dispatch(Device& dev, DeviceEvent ev) noexcept {
            if (ev == DeviceEvent::attach) {
                dev.state = DeviceState::detected;
            }
            if (!dev.driver) {
                return util::unexpected(util::Errc::noent);
            }

            util::Result<void> result{};
            switch (ev) {
            case DeviceEvent::probe:
                result = dispatch_driver_step(dev,
                                              dev.driver->ops.try_probe,
                                              dev.driver->ops.probe);
                break;
            case DeviceEvent::init:
                result = dispatch_driver_step(dev,
                                              dev.driver->ops.try_init,
                                              dev.driver->ops.init);
                if (result) {
                    dev.state = DeviceState::initialized;
                }
                break;
            case DeviceEvent::start:
                dev.state = DeviceState::running;
                break;
            case DeviceEvent::suspend:
                result = dispatch_driver_step(dev,
                                              dev.driver->ops.try_suspend,
                                              dev.driver->ops.suspend);
                if (result) {
                    dev.state = DeviceState::suspended;
                }
                break;
            case DeviceEvent::resume:
                result = dispatch_driver_step(dev,
                                              dev.driver->ops.try_resume,
                                              dev.driver->ops.resume);
                if (result) {
                    dev.state = DeviceState::running;
                }
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
                result = util::unexpected(util::Errc::io);
                break;
            case DeviceEvent::attach:
            default:
                break;
            }
            if (dev.driver && dev.driver->ops.on_event) {
                dev.driver->ops.on_event(dev, ev);
            }
            return result;
        }

        bool dispatch(Device& dev, DeviceEvent ev) noexcept {
            return static_cast<bool>(try_dispatch(dev, ev));
        }

    private:
        struct MatchResult {
            const Driver* drv{nullptr};
            util::u32 score{0};
        };

        static void note_first_error(util::Errc& first_error,
                                     const util::Result<void>& result) noexcept {
            if (!result && first_error == util::Errc::ok) {
                first_error = result.error();
            }
        }

        [[nodiscard]] static util::Result<void> finish_error(util::Errc first_error) noexcept {
            if (first_error != util::Errc::ok) {
                return util::unexpected(first_error);
            }
            return {};
        }

        [[nodiscard]] static util::Result<void> dispatch_driver_step(
            Device& dev,
            util::Result<void> (*try_step)(Device&) noexcept,
            bool (*step)(Device&) noexcept) noexcept {
            if (try_step) {
                return try_step(dev);
            }
            if (!step || step(dev)) {
                return {};
            }
            return util::unexpected(util::Errc::bad_state);
        }

        [[nodiscard]] util::usize find_exact_device_index(const DeviceDesc& desc,
                                                          void* ctx) const noexcept {
            for (util::usize i = 0; i < device_count_; ++i) {
                const auto& dev = devices_[i];
                if (!desc_equal(dev.desc, desc)) continue;
                if (dev.ctx != ctx) continue;
                return i;
            }
            return device_count_;
        }

        static bool desc_equal(const DeviceDesc& lhs, const DeviceDesc& rhs) noexcept {
            return lhs.class_id == rhs.class_id &&
                   lhs.vendor_id == rhs.vendor_id &&
                   lhs.product_id == rhs.product_id &&
                   lhs.type.compare(rhs.type) == 0;
        }

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

        [[nodiscard]] util::Result<void> try_match_device(Device& dev) noexcept {
            select_driver(dev);
            if (!dev.driver) return {};

            auto probed = try_dispatch(dev, DeviceEvent::probe);
            if (!probed) {
                dev.driver = nullptr;
                return probed;
            }

            auto initialized = try_dispatch(dev, DeviceEvent::init);
            if (!initialized) {
                (void)try_dispatch(dev, DeviceEvent::remove);
                dev.driver = nullptr;
                dev.state = DeviceState::detected;
                return initialized;
            }

            return try_dispatch(dev, DeviceEvent::start);
        }

        [[nodiscard]] util::Result<void> try_dispatch_bound_all(DeviceEvent ev) noexcept {
            util::Errc first_error = util::Errc::ok;
            for (util::usize i = 0; i < device_count_; ++i) {
                auto& dev = devices_[i];
                if (dev.driver == nullptr) {
                    continue;
                }
                note_first_error(first_error, try_dispatch(dev, ev));
            }
            return finish_error(first_error);
        }

        std::array<Device, MaxDevices> devices_{};
        std::array<const Driver*, MaxDrivers> drivers_{};
        util::usize device_count_{0};
        util::usize driver_count_{0};
    };

#ifndef NDEBUG
    inline bool registry_add_device_idempotent_self_check() noexcept {
        Registry<4, 1> registry{};
        DeviceDesc desc{
            .class_id = 0x08,
            .vendor_id = 0x1234,
            .product_id = 0x5678,
            .type = "usb.host.msc"
        };
        int marker_a = 1;
        int marker_b = 2;

        if (!registry.add_device(desc, &marker_a)) return false;
        if (!registry.add_device(desc, &marker_a)) return false;
        if (registry.device_count() != 1) return false;
        if (!registry.add_device(desc, &marker_b)) return false;
        if (registry.device_count() != 2) return false;
        Registry<1, 1> limited{};
        if (!limited.try_add_device(desc, &marker_a)) return false;
        auto overflow = limited.try_add_device(desc, &marker_b);
        if (overflow || overflow.error() != util::Errc::buffer_overflow) return false;
        return registry.device_at(0).ctx == &marker_a &&
               registry.device_at(1).ctx == &marker_b;
    }

    inline bool registry_dispatch_result_self_check() noexcept {
        DeviceDesc desc{
            .class_id = 0x08,
            .vendor_id = 0x1234,
            .product_id = 0x5678,
            .type = "usb.host.msc"
        };

        Driver failing_driver{
            .name = "failing.driver",
            .match = desc,
            .ops = DriverOps{
                .probe = [](Device&) noexcept { return true; },
                .init = [](Device&) noexcept { return false; }
            }
        };

        Registry<2, 2> failing_registry{};
        if (!failing_registry.try_add_driver(failing_driver)) return false;
        if (!failing_registry.try_add_device(desc)) return false;
        auto failed_match = failing_registry.try_match_all();
        if (failed_match || failed_match.error() != util::Errc::bad_state) return false;
        if (failing_registry.device_at(0).driver != nullptr) return false;
        if (failing_registry.device_at(0).state != DeviceState::detected) return false;

        Driver good_driver{
            .name = "good.driver",
            .match = desc,
            .ops = DriverOps{
                .probe = [](Device&) noexcept { return true; },
                .init = [](Device&) noexcept { return true; },
                .suspend = [](Device&) noexcept { return false; }
            }
        };

        Registry<2, 2> registry{};
        if (!registry.try_add_driver(good_driver)) return false;
        if (!registry.try_add_device(desc)) return false;
        if (!registry.try_match_all()) return false;
        auto suspend_r = registry.try_dispatch(registry.device_at(0), DeviceEvent::suspend);
        if (suspend_r || suspend_r.error() != util::Errc::bad_state) return false;
        auto remove_r = registry.try_dispatch(registry.device_at(0), DeviceEvent::remove);
        if (!remove_r) return false;
        auto missing_r = registry.try_dispatch(registry.device_at(0), DeviceEvent::resume);
        return !missing_r && missing_r.error() == util::Errc::noent;
    }
#endif
}
