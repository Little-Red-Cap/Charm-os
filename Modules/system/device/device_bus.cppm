module;

#include <cstddef>

export module device.bus;

import util.core;
import device.desc;
import device.registry;

export namespace device {
    struct BusOps {
        bool (*enumerate)(void* ctx, RegistryBase& reg) noexcept { nullptr };
        void (*attach)(void* ctx, const DeviceDesc& desc) noexcept { nullptr };
        void (*detach)(void* ctx, const DeviceDesc& desc) noexcept { nullptr };
    };

    struct Bus {
        const char* name{nullptr};
        void* ctx{nullptr};
        BusOps ops{};
    };
}
