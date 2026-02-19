#include <cstdint>

import device.desc;
import device.driver;
import device.registry;
import out.print;

namespace {
    bool probe(device::Device& dev) noexcept {
        (void)out::println<"[device] probe type={}">(
            dev.desc.type.empty() ? "(none)" : dev.desc.type);
        return true;
    }

    bool init(device::Device& dev) noexcept {
        (void)out::println<"[device] init class={} vendor={} product={}">(
            dev.desc.class_id,
            dev.desc.vendor_id,
            dev.desc.product_id);
        return true;
    }

    void shutdown(device::Device& dev) noexcept {
        (void)out::println<"[device] shutdown type={}">(
            dev.desc.type.empty() ? "(none)" : dev.desc.type);
    }
}

int main() {
    device::Driver drv{
        .name = "demo.driver",
        .match = device::DeviceDesc{
            .class_id = 0x01,
            .vendor_id = 0x1234,
            .product_id = 0x5678,
            .type = "usb.cdc"
        },
        .ops = device::DriverOps{
            .probe = probe,
            .init = init,
            .shutdown = shutdown
        }
    };

    device::Registry<8, 8> reg{};
    (void)reg.add_driver(drv);
    (void)reg.add_device(device::DeviceDesc{
        .class_id = 0x01,
        .vendor_id = 0x1234,
        .product_id = 0x5678,
        .type = "usb.cdc"
    });
    reg.match_all();
    reg.shutdown_all();
    return 0;
}
