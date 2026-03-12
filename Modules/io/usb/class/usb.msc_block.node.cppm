module;

#include <array>
#include <optional>
#include <span>

export module usb.class_msc_block.node;

import block.device;
import block.registry;
import init.node;
import usb.class_msc;
import usb.class_msc_block;
import usb.device;
import usb.device_driver;
import usb.driver;
import usb.common;
import usb.dsl;
import util.core;
import util.error;

export namespace usb::device {
    struct MscBlockDesc {
        const char* cap_name{"usb.msc0"};
        const char* block_cap{"block.sd0"};
        usb::driver::DcdOps dcd{};
        void* dcd_ctx{nullptr};
        usb::driver::DcdDeviceAdapter* adapter{nullptr};
        usb::dsl::DeviceInfo dev_info{};
        usb::dsl::ConfigInfo cfg_info{};
        usb::class_driver::MscConfig msc_cfg{};
        std::span<const std::span<const usb::u8>> strings{};
        usb::class_driver::MscBlockConfig storage_cfg{};
    };

    template <typename RegistryT,
              util::usize IoBufSize = 4096,
              util::usize DevDescSize = 64,
              util::usize CfgDescSize = 256>
    struct MscBlockBinding {
        RegistryT* registry{nullptr};
        MscBlockDesc desc{};
        std::array<init::CapId, 1> provides{};
        std::array<init::CapId, 2> requires_caps{};
        init::Node node{};

        std::array<usb::u8, DevDescSize> dev_desc{};
        std::array<usb::u8, CfgDescSize> cfg_desc{};
        usb::device::DescriptorTable table{};
        usb::device::ConfigTree tree{};
        usb::device::Device dev{};
        std::optional<usb::device::DeviceDriver> driver{};
        std::optional<usb::class_driver::MscBot> bot{};
        std::optional<usb::class_driver::MscDevice> msc{};
        std::array<usb::class_driver::MscStorage, 1> luns{};
        std::array<usb::u8, IoBufSize> io_buf{};

        MscBlockBinding(RegistryT& reg,
                        const MscBlockDesc& d,
                        init::Phase phase = init::Phase::core,
                        util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all)) noexcept
            : registry(&reg),
              desc(d) {
            provides[0] = init::cap_id(desc.cap_name);
            requires_caps[0] = init::cap_id("block.registry");
            requires_caps[1] = init::cap_id(desc.block_cap);
            node = init::Node{
                desc.cap_name,
                phase,
                runlevel_mask,
                std::span<const init::CapId>(provides.data(), provides.size()),
                std::span<const init::CapId>(requires_caps.data(), requires_caps.size()),
                &MscBlockBinding::init_trampoline,
                &MscBlockBinding::deinit_trampoline,
                this
            };
        }

        static util::Result<void> init_trampoline(void* ctx) noexcept {
            auto* self = static_cast<MscBlockBinding*>(ctx);
            if (!self || !self->registry) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if (!self->desc.dcd.ep.open || !self->desc.dcd.ep.send) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if (self->desc.block_cap == nullptr) {
                return util::unexpected(util::Errc::invalid_arg);
            }

            auto* dev = self->registry->open_device(self->desc.block_cap);
            if (!dev) {
                return util::unexpected(util::Errc::noent);
            }

            const auto block_size = dev->block_size;
            if (block_size == 0 || block_size > self->io_buf.size()) {
                return util::unexpected(util::Errc::buffer_overflow);
            }

            auto storage = usb::class_driver::make_storage_from_block_device(
                *dev, self->desc.storage_cfg);
            self->luns[0] = storage;

            self->bot.emplace(
                std::span<usb::class_driver::MscStorage>(self->luns.data(), self->luns.size()),
                std::span<usb::u8>(self->io_buf.data(), self->io_buf.size()));
            auto ops = usb::class_driver::make_msc_ops(*self->bot);
            self->msc.emplace(&(*self->bot), ops);

            usb::dsl::DeviceBuildContext build_ctx{
                std::span<usb::u8>(self->dev_desc.data(), self->dev_desc.size()),
                std::span<usb::u8>(self->cfg_desc.data(), self->cfg_desc.size()),
                &self->table,
                &self->tree
            };

            const auto strings_ptr =
                self->desc.strings.empty() ? nullptr : self->desc.strings.data();
            const auto strings_count = self->desc.strings.size();

            const auto ok = usb::device::examples::build_attach_open_msc(
                self->dev,
                *self->msc,
                build_ctx,
                self->desc.dev_info,
                self->desc.cfg_info,
                self->desc.msc_cfg,
                usb::dsl::MscClassDescriptors{}.view(),
                strings_ptr,
                strings_count,
                self->desc.dcd,
                self->desc.dcd_ctx,
                usb::device::examples::make_msc_ep_callbacks(*self->bot));

            if (!ok) {
                return util::unexpected(util::Errc::io);
            }

            self->driver.emplace(self->dev, self->desc.dcd_ctx, self->desc.dcd);
            if (self->desc.adapter) {
                self->desc.adapter->callbacks = self->driver->callbacks();
            }
            if (self->desc.dcd.connect) {
                self->desc.dcd.connect(self->desc.dcd_ctx, true);
            }

            return {};
        }

        static void deinit_trampoline(void* ctx) noexcept {
            auto* self = static_cast<MscBlockBinding*>(ctx);
            if (!self) return;
            if (self->desc.dcd.connect) {
                self->desc.dcd.connect(self->desc.dcd_ctx, false);
            }
            if (self->msc && self->desc.dcd.ep.close) {
                usb::device::examples::close_msc_endpoints(
                    self->desc.dcd, self->desc.dcd_ctx, *self->msc);
            }
            self->driver.reset();
            self->msc.reset();
            self->bot.reset();
        }
    };
}
