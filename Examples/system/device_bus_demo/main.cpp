#include <cstdint>

import device.desc;
import device.driver;
import device.registry;
import device.bus;
import device.manager;
import out.print;

namespace {
    bool demo_enumerate(void*, device::RegistryBase& reg) noexcept {
        device::DeviceDesc desc{
            .class_id = 0x08,
            .vendor_id = 0x1234,
            .product_id = 0x5678,
            .type = "usb.msc"
        };
        return reg.add_device(desc, nullptr);
    }

    bool demo_probe(device::Device& dev) noexcept {
        (void)out::println<"[bus] probe type={}">(
            dev.desc.type.empty() ? "(none)" : dev.desc.type);
        return true;
    }

    bool demo_init(device::Device& dev) noexcept {
        (void)out::println<"[bus] init class={}">(
            dev.desc.class_id);
        return true;
    }
}

int main() {
    device::Driver drv{
        .name = "demo.msc",
        .match = device::DeviceDesc{
            .class_id = 0x08,
            .vendor_id = 0,
            .product_id = 0,
            .type = "usb.msc"
        },
        .ops = device::DriverOps{
            .probe = demo_probe,
            .init = demo_init
        }
    };

    device::System<8, 8, 4> sys{};
    (void)sys.add_driver(drv);

    device::Bus bus{
        .name = "demo.bus",
        .ctx = nullptr,
        .ops = device::BusOps{.enumerate = demo_enumerate}
    };
    (void)sys.add_bus(bus);

    sys.init_all();
    return 0;
}
