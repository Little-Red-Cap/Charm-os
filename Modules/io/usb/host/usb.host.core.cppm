module;

#include <cstdint>

export module usb.host.core;

import device.bus;
import device.desc;
import device.registry;
import util.error;

export namespace usb::host {
    struct HostOps {
        bool (*enumerate)(void* ctx, device::RegistryBase& reg) noexcept { nullptr };
        void (*attach)(void* ctx, const device::DeviceDesc& desc) noexcept { nullptr };
        void (*detach)(void* ctx, const device::DeviceDesc& desc) noexcept { nullptr };
        util::Result<void> (*try_enumerate)(void* ctx, device::RegistryBase& reg) noexcept { nullptr };
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
            b.ops.try_enumerate = &HostBus::try_enumerate;
            return b;
        }

    private:
        static util::Result<void> try_enumerate(void* ctx, device::RegistryBase& reg) noexcept {
            auto* self = static_cast<HostBus*>(ctx);
            if (!self) {
                return util::unexpected(util::Errc::bad_state);
            }
            if (self->ops_.try_enumerate) {
                return self->ops_.try_enumerate(self->ctx_, reg);
            }
            if (!self->ops_.enumerate || !self->ops_.enumerate(self->ctx_, reg)) {
                return util::unexpected(util::Errc::bad_state);
            }
            return {};
        }

        static bool enumerate(void* ctx, device::RegistryBase& reg) noexcept {
            return static_cast<bool>(try_enumerate(ctx, reg));
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
