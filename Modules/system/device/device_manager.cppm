module;

#include <array>
#include <cstddef>

export module device.manager;

import util.core;
import device.bus;
import device.registry;
import device.desc;
import device.driver;

export namespace device {
    template <util::usize MaxBuses>
    class BusManager {
    public:
        bool add_bus(const Bus& bus) noexcept {
            if (bus_count_ >= MaxBuses) return false;
            buses_[bus_count_++] = bus;
            return true;
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
        bool add_driver(const Driver& drv) noexcept { return registry_.add_driver(drv); }
        bool add_device(const DeviceDesc& desc, void* ctx = nullptr) noexcept {
            return registry_.add_device(desc, ctx);
        }
        bool add_bus(const Bus& bus) noexcept { return buses_.add_bus(bus); }

        void init_all() noexcept {
            buses_.enumerate_all(registry_);
            registry_.init_all();
        }

        void shutdown_all() noexcept { registry_.shutdown_all(); }
        void suspend_all() noexcept { registry_.suspend_all(); }
        void resume_all() noexcept { registry_.resume_all(); }

    private:
        Registry<MaxDevices, MaxDrivers> registry_{};
        BusManager<MaxBuses> buses_{};
    };
}
