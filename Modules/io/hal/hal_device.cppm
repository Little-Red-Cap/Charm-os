module;

export module hal_device;

import device.desc;
import device.registry;
import device.types;
import util.core;

export namespace hal {
    struct HalDeviceHook {
        void* ctx{nullptr};
        bool (*probe)(void* ctx) noexcept { nullptr };
        bool (*init)(void* ctx) noexcept { nullptr };
        void (*shutdown)(void* ctx) noexcept { nullptr };
        bool (*suspend)(void* ctx) noexcept { nullptr };
        bool (*resume)(void* ctx) noexcept { nullptr };
    };

    inline device::DeviceDesc make_hal_desc(const char* type) noexcept {
        device::DeviceDesc desc{};
        desc.type = type;
        return desc;
    }

    inline device::Driver make_hal_driver(HalDeviceHook* hook,
                                          const device::DeviceDesc& match,
                                          const char* name,
                                          util::u32 priority) noexcept {
        device::Driver drv{};
        drv.name = name;
        drv.match = match;
        drv.priority = priority;
        drv.ops.probe = [] (device::Device& dev) noexcept -> bool {
            auto* h = static_cast<HalDeviceHook*>(dev.ctx);
            if (!h || !h->probe) return true;
            return h->probe(h->ctx);
        };
        drv.ops.init = [] (device::Device& dev) noexcept -> bool {
            auto* h = static_cast<HalDeviceHook*>(dev.ctx);
            if (!h || !h->init) return true;
            return h->init(h->ctx);
        };
        drv.ops.shutdown = [] (device::Device& dev) noexcept {
            auto* h = static_cast<HalDeviceHook*>(dev.ctx);
            if (h && h->shutdown) h->shutdown(h->ctx);
        };
        drv.ops.remove = drv.ops.shutdown;
        drv.ops.suspend = [] (device::Device& dev) noexcept -> bool {
            auto* h = static_cast<HalDeviceHook*>(dev.ctx);
            if (!h || !h->suspend) return true;
            return h->suspend(h->ctx);
        };
        drv.ops.resume = [] (device::Device& dev) noexcept -> bool {
            auto* h = static_cast<HalDeviceHook*>(dev.ctx);
            if (!h || !h->resume) return true;
            return h->resume(h->ctx);
        };
        return drv;
    }

    inline bool register_hal_device(device::RegistryBase& reg,
                                    const device::DeviceDesc& desc,
                                    HalDeviceHook* hook) noexcept {
        return reg.add_device(desc, hook);
    }
}
