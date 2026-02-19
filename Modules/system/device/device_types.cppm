module;

#include <cstdint>

export module device.types;

import util.core;
import device.desc;
export namespace device {
    struct Driver;

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
