module;

#include <cstdint>

export module device.types;

import util.core;
import device.desc;
export namespace device {
    struct Device;

    struct DriverOps {
        bool (*probe)(Device& dev) noexcept { nullptr };
        bool (*init)(Device& dev) noexcept { nullptr };
        void (*shutdown)(Device& dev) noexcept { nullptr };
        void (*remove)(Device& dev) noexcept { nullptr };
        bool (*suspend)(Device& dev) noexcept { nullptr };
        bool (*resume)(Device& dev) noexcept { nullptr };
    };

    struct Driver {
        const char* name{nullptr};
        DeviceDesc match{};
        DriverOps ops{};
    };

    enum class DeviceState : util::u8 {
        detected,
        initialized,
        running,
        suspended,
        stopped
    };

    struct Device {
        DeviceDesc desc{};
        void* ctx{nullptr};
        DeviceState state{DeviceState::detected};
        const Driver* driver{nullptr};
    };
}
