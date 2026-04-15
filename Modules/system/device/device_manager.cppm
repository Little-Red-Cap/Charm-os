module;

#include <array>
#include <cstddef>

export module device.manager;

import util.core;
import util.error;
import device.bus;
import device.registry;
import device.desc;
import device.types;

export namespace device {
    template <util::usize MaxBuses>
    class BusManager {
    public:
        [[nodiscard]] util::Result<void> try_add_bus(const Bus& bus) noexcept {
            if (bus_count_ >= MaxBuses) {
                return util::unexpected(util::Errc::buffer_overflow);
            }
            buses_[bus_count_++] = bus;
            return {};
        }

        bool add_bus(const Bus& bus) noexcept {
            return static_cast<bool>(try_add_bus(bus));
        }

        [[nodiscard]] util::Result<void> try_enumerate_all(RegistryBase& reg) noexcept {
            util::Errc first_error = util::Errc::ok;
            for (util::usize i = 0; i < bus_count_; ++i) {
                auto& b = buses_[i];
                if (b.ops.try_enumerate) {
                    auto enumerated = b.ops.try_enumerate(b.ctx, reg);
                    if (!enumerated && first_error == util::Errc::ok) {
                        first_error = enumerated.error();
                    }
                    continue;
                }
                if (!b.ops.enumerate) {
                    continue;
                }
                if (!b.ops.enumerate(b.ctx, reg) && first_error == util::Errc::ok) {
                    first_error = util::Errc::bad_state;
                }
            }
            if (first_error != util::Errc::ok) {
                return util::unexpected(first_error);
            }
            return {};
        }

        void enumerate_all(RegistryBase& reg) noexcept {
            (void)try_enumerate_all(reg);
        }

    private:
        std::array<Bus, MaxBuses> buses_{};
        util::usize bus_count_{0};
    };

    template <util::usize MaxDevices, util::usize MaxDrivers, util::usize MaxBuses>
    class System {
    public:
        [[nodiscard]] util::Result<void> try_add_driver(const Driver& drv) noexcept {
            return registry_.try_add_driver(drv);
        }

        bool add_driver(const Driver& drv) noexcept { return static_cast<bool>(try_add_driver(drv)); }

        [[nodiscard]] util::Result<void> try_add_device(const DeviceDesc& desc,
                                                        void* ctx = nullptr) noexcept {
            return registry_.try_add_device(desc, ctx);
        }

        bool add_device(const DeviceDesc& desc, void* ctx = nullptr) noexcept {
            return static_cast<bool>(try_add_device(desc, ctx));
        }

        [[nodiscard]] util::Result<void> try_add_bus(const Bus& bus) noexcept {
            return buses_.try_add_bus(bus);
        }

        bool add_bus(const Bus& bus) noexcept { return static_cast<bool>(try_add_bus(bus)); }

        [[nodiscard]] util::Result<void> try_init_all() noexcept {
            auto enumerated = buses_.try_enumerate_all(registry_);
            auto initialized = registry_.try_init_all();
            if (!enumerated) {
                return enumerated;
            }
            return initialized;
        }

        void init_all() noexcept {
            (void)try_init_all();
        }

        [[nodiscard]] util::Result<void> try_shutdown_all() noexcept { return registry_.try_shutdown_all(); }
        void shutdown_all() noexcept { (void)try_shutdown_all(); }

        [[nodiscard]] util::Result<void> try_suspend_all() noexcept { return registry_.try_suspend_all(); }
        void suspend_all() noexcept { (void)try_suspend_all(); }

        [[nodiscard]] util::Result<void> try_resume_all() noexcept { return registry_.try_resume_all(); }
        void resume_all() noexcept { (void)try_resume_all(); }

        [[nodiscard]] util::Result<void> try_dispatch_all(DeviceEvent ev) noexcept {
            util::Errc first_error = util::Errc::ok;
            for (util::usize i = 0; i < registry_.device_count(); ++i) {
                auto& dev = registry_.device_at(i);
                if (dev.driver == nullptr) {
                    continue;
                }
                auto dispatched = registry_.try_dispatch(dev, ev);
                if (!dispatched && first_error == util::Errc::ok) {
                    first_error = dispatched.error();
                }
            }
            if (first_error != util::Errc::ok) {
                return util::unexpected(first_error);
            }
            return {};
        }

        void dispatch_all(DeviceEvent ev) noexcept { (void)try_dispatch_all(ev); }

    private:
        Registry<MaxDevices, MaxDrivers> registry_{};
        BusManager<MaxBuses> buses_{};
    };
}
