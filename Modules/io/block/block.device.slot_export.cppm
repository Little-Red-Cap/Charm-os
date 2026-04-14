module;

#include <span>
#include <string_view>

export module block.device.slot_export;

import block.device;
import block.device.slot;
import block.registry;
import util.core;
import util.error;

export namespace block {
    template <typename RegistryT>
    class DeviceSlotExport {
    public:
        DeviceSlotExport(RegistryT& registry, const DeviceDesc& desc) noexcept
            : registry_(&registry), desc_(desc) {
        }

        DeviceSlotExport(RegistryT& registry, std::string_view endpoint_name) noexcept
            : DeviceSlotExport(registry, DeviceDesc{endpoint_name, cap_id(endpoint_name)}) {
        }

        util::Result<void> ensure_exported() noexcept {
            if (!registry_ || desc_.name.empty() || desc_.cap == 0) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if (const auto* ep = registry_->find_device(desc_.cap)) {
                if (ep->desc.name.compare(desc_.name) != 0 || ep->dev != &slot_.device()) {
                    return util::unexpected(util::Errc::exist);
                }
                return {};
            }
            return registry_->register_device(desc_, slot_.device());
        }

        util::Result<void> attach(Device& target) noexcept {
            auto exported = ensure_exported();
            if (!exported) {
                return exported;
            }
            return slot_.attach(target);
        }

        void detach() noexcept {
            slot_.detach();
        }

        [[nodiscard]] bool exported() const noexcept {
            if (!registry_) {
                return false;
            }
            const auto* ep = registry_->find_device(desc_.cap);
            return ep != nullptr &&
                   ep->desc.name.compare(desc_.name) == 0 &&
                   ep->dev == &slot_.device();
        }

        [[nodiscard]] bool attached() const noexcept { return slot_.attached(); }
        [[nodiscard]] util::u32 generation() const noexcept { return slot_.generation(); }
        [[nodiscard]] Device* target() const noexcept { return slot_.target(); }
        [[nodiscard]] const DeviceDesc& desc() const noexcept { return desc_; }

        DeviceSlot& slot() noexcept { return slot_; }
        const DeviceSlot& slot() const noexcept { return slot_; }

        Device& device() noexcept { return slot_.device(); }
        const Device& device() const noexcept { return slot_.device(); }

    private:
        RegistryT* registry_{nullptr};
        DeviceDesc desc_{};
        DeviceSlot slot_{};
    };

#ifndef NDEBUG
    inline bool device_slot_export_self_check() noexcept {
        struct DummyDisk {
            static Status read_cb(void*, util::u64, std::span<util::u8> out) noexcept {
                if (out.empty()) {
                    return Status{Errc::invalid_arg};
                }
                out[0] = 0xA5;
                return {};
            }

            Device dev{};

            DummyDisk() noexcept {
                dev.ctx = this;
                dev.read = &DummyDisk::read_cb;
                dev.block_size = 512;
                dev.block_count = 2;
                dev.caps = to_bits(Caps::read);
            }
        };

        Registry<2> registry{};
        registry.init();

        DeviceSlotExport<Registry<2>> exported{registry, "block.usb0"};
        if (exported.exported()) return false;
        if (!exported.ensure_exported()) return false;
        if (!exported.exported()) return false;
        if (registry.open_device("block.usb0") != &exported.device()) return false;

        DummyDisk disk{};
        if (!exported.attach(disk.dev)) return false;
        if (!exported.attached()) return false;
        if (exported.generation() != 1) return false;

        util::u8 byte = 0;
        auto st = exported.device().read(exported.device().ctx, 0, std::span<util::u8>(&byte, 1));
        if (!st || byte != 0xA5) return false;

        exported.detach();
        if (exported.attached()) return false;
        if (exported.generation() != 2) return false;

        byte = 0;
        st = exported.device().read(exported.device().ctx, 0, std::span<util::u8>(&byte, 1));
        return st.err == Errc::noent;
    }
#endif
}
