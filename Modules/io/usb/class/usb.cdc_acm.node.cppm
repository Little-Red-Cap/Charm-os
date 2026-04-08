module;

#include <array>
#include <optional>
#include <span>

export module usb.class_cdc_acm.node;

import init.node;
import usb.class_cdc;
import usb.common;
import usb.device;
import usb.device_driver;
import usb.driver;
import usb.dsl;
import util.core;
import util.error;

export namespace usb::device {
    struct CdcAcmDesc {
        const char* cap_name{"usb.cdc0"};
        usb::driver::DcdOps dcd{};
        void* dcd_ctx{nullptr};
        usb::driver::DcdDeviceAdapter* adapter{nullptr};
        usb::dsl::DeviceInfo dev_info{};
        usb::dsl::ConfigInfo cfg_info{};
        usb::class_driver::CdcConfig cdc_cfg{};
        std::span<const std::span<const usb::u8>> strings{};
        void* cdc_ctx{nullptr};
        usb::class_driver::CdcOps cdc_ops{};
        void (*on_ready)(void* ctx,
                         usb::class_driver::CdcAcm* cdc,
                         const usb::class_driver::CdcConfig* cfg) noexcept { nullptr };
        void* on_ready_ctx{nullptr};
    };

    template <util::usize DevDescSize = 64,
              util::usize CfgDescSize = 256>
    struct CdcAcmBinding {
        CdcAcmDesc desc{};
        std::array<init::CapId, 1> provides{};
        std::array<init::CapId, 0> requires_caps{};
        init::Node node{};

        std::array<usb::u8, DevDescSize> dev_desc{};
        std::array<usb::u8, CfgDescSize> cfg_desc{};
        usb::device::DescriptorTable table{};
        usb::device::ConfigTree tree{};
        usb::device::Device dev{};
        std::optional<usb::device::DeviceDriver> driver{};
        std::optional<usb::class_driver::CdcAcm> cdc{};

        CdcAcmBinding(const CdcAcmDesc& d,
                      init::Phase phase = init::Phase::core,
                      util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all)) noexcept
            : desc(d) {
            provides[0] = init::cap_id(desc.cap_name);
            node = init::Node{
                desc.cap_name,
                phase,
                runlevel_mask,
                std::span<const init::CapId>(provides.data(), provides.size()),
                std::span<const init::CapId>(requires_caps.data(), requires_caps.size()),
                &CdcAcmBinding::init_trampoline,
                &CdcAcmBinding::deinit_trampoline,
                this
            };
        }

        static util::Result<void> init_trampoline(void* ctx) noexcept {
            auto* self = static_cast<CdcAcmBinding*>(ctx);
            if (!self) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if (!self->desc.dcd.ep.open || !self->desc.dcd.ep.send) {
                return util::unexpected(util::Errc::invalid_arg);
            }

            self->cdc.emplace(self->desc.cdc_ctx, self->desc.cdc_ops);
            self->cdc->set_config(self->desc.cdc_cfg);

            usb::dsl::DeviceBuildContext build_ctx{
                std::span<usb::u8>(self->dev_desc.data(), self->dev_desc.size()),
                std::span<usb::u8>(self->cfg_desc.data(), self->cfg_desc.size()),
                &self->table,
                &self->tree,
            };
            const auto strings_ptr = self->desc.strings.empty() ? nullptr : self->desc.strings.data();
            const auto strings_count = self->desc.strings.size();
            const auto class_desc = usb::dsl::make_cdc_acm_class_descriptors(self->desc.cdc_cfg);

            const auto ok = usb::device::examples::build_attach_open_cdc(
                self->dev,
                *self->cdc,
                build_ctx,
                self->desc.dev_info,
                self->desc.cfg_info,
                self->desc.cdc_cfg,
                class_desc.view(),
                strings_ptr,
                strings_count,
                self->desc.dcd,
                self->desc.dcd_ctx,
                usb::device::examples::make_cdc_ep_callbacks(*self->cdc));
            if (!ok) {
                return util::unexpected(util::Errc::io);
            }

            self->driver.emplace(self->dev, self->desc.dcd_ctx, self->desc.dcd);
            if (self->desc.adapter) {
                self->desc.adapter->callbacks = self->driver->callbacks();
            }
            if (self->desc.on_ready) {
                self->desc.on_ready(self->desc.on_ready_ctx, &(*self->cdc), &self->desc.cdc_cfg);
            }
            if (self->desc.dcd.connect) {
                self->desc.dcd.connect(self->desc.dcd_ctx, true);
            }
            return {};
        }

        static void deinit_trampoline(void* ctx) noexcept {
            auto* self = static_cast<CdcAcmBinding*>(ctx);
            if (!self) return;
            if (self->desc.dcd.connect) {
                self->desc.dcd.connect(self->desc.dcd_ctx, false);
            }
            if (self->cdc && self->desc.dcd.ep.close) {
                usb::device::examples::close_cdc_endpoints(
                    self->desc.dcd, self->desc.dcd_ctx, *self->cdc);
            }
            self->driver.reset();
            self->cdc.reset();
        }
    };
}
