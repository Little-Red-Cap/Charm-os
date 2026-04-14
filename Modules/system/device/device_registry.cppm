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

        void match_all() noexcept {
            for (util::usize i = 0; i < device_count_; ++i) {
                match_device(devices_[i]);
            }
        }

        void match_detected() noexcept {
            for (util::usize i = 0; i < device_count_; ++i) {
                auto& dev = devices_[i];
                if (dev.state != DeviceState::detected) continue;
                if (dev.driver != nullptr) continue;
                match_device(dev);
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

        void match_device(Device& dev) noexcept {
            select_driver(dev);
            if (!dev.driver) return;
            if (!dispatch(dev, DeviceEvent::probe)) {
                dev.driver = nullptr;
                return;
            }
            if (!dispatch(dev, DeviceEvent::init)) {
                dispatch(dev, DeviceEvent::remove);
                dev.driver = nullptr;
                dev.state = DeviceState::detected;
                return;
            }
            (void)dispatch(dev, DeviceEvent::start);
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
#endif
}
