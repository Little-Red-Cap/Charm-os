#include <cstdint>
#include <cstdio>

import device.bus;
import device.desc;
import device.manager;
import device.registry;
import device.types;
import util.error;

namespace {
    bool expect_ok(const util::Result<void>& result, const char* message) {
        if (!result) {
            std::fprintf(stderr,
                         "[ERR] %s err=%d\n",
                         message,
                         static_cast<int>(result.error()));
            return false;
        }
        return true;
    }

    util::Result<void> demo_try_enumerate(void*, device::RegistryBase& reg) noexcept {
        device::DeviceDesc desc{
            .class_id = 0x08,
            .vendor_id = 0x1234,
            .product_id = 0x5678,
            .type = "usb.msc"
        };
        return reg.try_add_device(desc, nullptr);
    }

    bool demo_enumerate(void* ctx, device::RegistryBase& reg) noexcept {
        return static_cast<bool>(demo_try_enumerate(ctx, reg));
    }

    util::Result<void> demo_try_probe(device::Device& dev) noexcept {
        std::printf("[bus] probe type=%s\n",
                    dev.desc.type.empty() ? "(none)" : dev.desc.type.data());
        return {};
    }

    bool demo_probe(device::Device& dev) noexcept {
        return static_cast<bool>(demo_try_probe(dev));
    }

    util::Result<void> demo_try_init(device::Device& dev) noexcept {
        std::printf("[bus] init class=%u\n",
                    static_cast<unsigned>(dev.desc.class_id));
        return {};
    }

    bool demo_init(device::Device& dev) noexcept {
        return static_cast<bool>(demo_try_init(dev));
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
            .init = demo_init,
            .try_probe = demo_try_probe,
            .try_init = demo_try_init
        }
    };

    device::System<8, 8, 4> sys{};
    if (!expect_ok(sys.try_add_driver(drv), "failed to add demo driver")) return 1;

    device::Bus bus{
        .name = "demo.bus",
        .ctx = nullptr,
        .ops = device::BusOps{
            .enumerate = demo_enumerate,
            .try_enumerate = demo_try_enumerate
        }
    };
    if (!expect_ok(sys.try_add_bus(bus), "failed to add demo bus")) return 1;

    return expect_ok(sys.try_init_all(), "demo system init failed") ? 0 : 1;
}
