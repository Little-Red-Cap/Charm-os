module;

#include <cstdint>

export module device.types;

import util.core;
import device.desc;
export namespace device {
    struct Device;

    enum class DeviceEvent : util::u8 {
        attach,
        probe,
        init,
        start,
        suspend,
        resume,
        shutdown,
        remove,
        error
    };

    struct DriverOps {
        bool (*probe)(Device& dev) noexcept { nullptr };
        bool (*init)(Device& dev) noexcept { nullptr };
        void (*shutdown)(Device& dev) noexcept { nullptr };
        void (*remove)(Device& dev) noexcept { nullptr };
        bool (*suspend)(Device& dev) noexcept { nullptr };
        bool (*resume)(Device& dev) noexcept { nullptr };
        void (*on_event)(Device& dev, DeviceEvent ev) noexcept { nullptr };
    };

    struct Driver {
        const char* name{nullptr};
        DeviceDesc match{};
        util::u32 priority{0};
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
        util::u32 match_score{0};
    };
}
