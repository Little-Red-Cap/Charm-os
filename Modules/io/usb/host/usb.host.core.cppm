module;

#include <cstdint>

export module usb.host.core;

import device.bus;
import device.desc;
import device.registry;

export namespace usb::host {
    struct HostOps {
        bool (*enumerate)(void* ctx, device::RegistryBase& reg) noexcept { nullptr };
        void (*attach)(void* ctx, const device::DeviceDesc& desc) noexcept { nullptr };
        void (*detach)(void* ctx, const device::DeviceDesc& desc) noexcept { nullptr };
    };

    class HostBus {
    public:
        HostBus(void* ctx,
                HostOps ops,
                const char* name = "usb.host") noexcept
            : ctx_(ctx),
              ops_(ops),
              name_(name ? name : "usb.host") {
        }

        device::Bus bus() const noexcept {
            device::Bus b{};
            b.name = name_;
            b.ctx = const_cast<HostBus*>(this);
            b.ops.enumerate = &HostBus::enumerate;
            b.ops.attach = &HostBus::attach;
            b.ops.detach = &HostBus::detach;
            return b;
        }

    private:
        static bool enumerate(void* ctx, device::RegistryBase& reg) noexcept {
            auto* self = static_cast<HostBus*>(ctx);
            if (!self || !self->ops_.enumerate) return false;
            return self->ops_.enumerate(self->ctx_, reg);
        }

        static void attach(void* ctx, const device::DeviceDesc& desc) noexcept {
            auto* self = static_cast<HostBus*>(ctx);
            if (self && self->ops_.attach) {
                self->ops_.attach(self->ctx_, desc);
            }
        }

        static void detach(void* ctx, const device::DeviceDesc& desc) noexcept {
            auto* self = static_cast<HostBus*>(ctx);
            if (self && self->ops_.detach) {
                self->ops_.detach(self->ctx_, desc);
            }
        }

        void* ctx_{nullptr};
        HostOps ops_{};
        const char* name_{"usb.host"};
    };
}
