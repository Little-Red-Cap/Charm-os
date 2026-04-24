#include <cstdint>
#include <cstdio>

import device.desc;
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

    util::Result<void> try_probe(device::Device& dev) noexcept {
        std::printf("[device] probe type=%s\n",
                    dev.desc.type.empty() ? "(none)" : dev.desc.type.data());
        return {};
    }

    bool probe(device::Device& dev) noexcept {
        return static_cast<bool>(try_probe(dev));
    }

    util::Result<void> try_init(device::Device& dev) noexcept {
        std::printf("[device] init class=%u vendor=%u product=%u\n",
                    static_cast<unsigned>(dev.desc.class_id),
                    static_cast<unsigned>(dev.desc.vendor_id),
                    static_cast<unsigned>(dev.desc.product_id));
        return {};
    }

    bool init(device::Device& dev) noexcept {
        return static_cast<bool>(try_init(dev));
    }

    void shutdown(device::Device& dev) noexcept {
        std::printf("[device] shutdown type=%s\n",
                    dev.desc.type.empty() ? "(none)" : dev.desc.type.data());
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
            .shutdown = shutdown,
            .try_probe = try_probe,
            .try_init = try_init
        }
    };

    device::Registry<8, 8> reg{};
    if (!expect_ok(reg.try_add_driver(drv), "failed to add demo driver")) return 1;
    if (!expect_ok(reg.try_add_device(device::DeviceDesc{
        .class_id = 0x01,
        .vendor_id = 0x1234,
        .product_id = 0x5678,
        .type = "usb.cdc"
    }),
                   "failed to add demo device")) {
        return 1;
    }
    if (!expect_ok(reg.try_match_all(), "demo registry match failed")) return 1;
    return expect_ok(reg.try_shutdown_all(), "demo registry shutdown failed") ? 0 : 1;
}
