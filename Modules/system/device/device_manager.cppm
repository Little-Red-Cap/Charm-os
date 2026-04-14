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

        void enumerate_all(RegistryBase& reg) noexcept {
            for (util::usize i = 0; i < bus_count_; ++i) {
                auto& b = buses_[i];
                if (b.ops.enumerate) {
                    b.ops.enumerate(b.ctx, reg);
                }
            }
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

        void init_all() noexcept {
            buses_.enumerate_all(registry_);
            registry_.init_all();
        }

        void shutdown_all() noexcept { registry_.shutdown_all(); }
        void suspend_all() noexcept { registry_.suspend_all(); }
        void resume_all() noexcept { registry_.resume_all(); }
        void dispatch_all(DeviceEvent ev) noexcept {
            for (util::usize i = 0; i < registry_.device_count(); ++i) {
                registry_.dispatch(registry_.device_at(i), ev);
            }
        }

    private:
        Registry<MaxDevices, MaxDrivers> registry_{};
        BusManager<MaxBuses> buses_{};
    };
}
