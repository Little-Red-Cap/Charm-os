module;

#include <array>
#include <optional>
#include <span>
#include <string_view>

export module usb.class_msc_cdc.node;

import block.device;
import block.registry;
import init.node;
import usb.class_cdc;
import usb.class_msc;
import usb.class_msc_block;
import usb.class_mux;
import usb.common;
import usb.device;
import usb.device_driver;
import usb.driver;
import usb.dsl;
import util.core;
import util.error;

export namespace usb::device {
    struct MscCdcCompositeDesc {
        const char* cap_name{"usb.msc_cdc0"};
        const char* msc_cap_name{"usb.msc0"};
        const char* block_cap{"block.sd0"};
        const char* cdc_cap_name{"usb.cdc0"};
        usb::driver::DcdOps dcd{};
        void* dcd_ctx{nullptr};
        usb::driver::DcdDeviceAdapter* adapter{nullptr};
        usb::dsl::DeviceInfo dev_info{};
        usb::dsl::ConfigInfo cfg_info{};
        std::span<const std::span<const usb::u8>> strings{};
        usb::class_driver::MscConfig msc_cfg{};
        usb::class_driver::CdcConfig cdc_cfg{};
        usb::class_driver::MscBlockConfig storage_cfg{};
        void (*on_ready)(void* ctx,
                         usb::class_driver::MscBot* bot,
                         const usb::class_driver::MscConfig* cfg) noexcept { nullptr };
        void* on_ready_ctx{nullptr};
    };

    template <typename RegistryT,
              util::usize IoBufSize = 4096,
              util::usize DevDescSize = 64,
              util::usize CfgDescSize = 256,
              util::usize CdcBufSize = 512>
    struct MscCdcBinding {
        RegistryT* registry{nullptr};
        MscCdcCompositeDesc desc{};
        const char* registry_cap_name{"block.registry"};
        std::array<init::CapId, 2> provides{};
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
        std::optional<usb::class_driver::CdcAcm> cdc{};
        usb::device::CompositeClassMux<2> mux{};
        std::array<usb::class_driver::MscStorage, 1> luns{};
        std::array<usb::u8, IoBufSize> io_buf{};
        std::array<usb::u8, CdcBufSize> cdc_tx_buf{};
        std::array<usb::u8, CdcBufSize> cdc_rx_buf{};
        std::size_t cdc_tx_len{0};
        std::size_t cdc_rx_len{0};
        std::array<usb::u8, 3> cdc_eps{};
        std::array<usb::u8, 2> msc_eps{};

        MscCdcBinding(RegistryT& reg,
                      const MscCdcCompositeDesc& d,
                      init::Phase phase = init::Phase::core,
                      util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all)) noexcept
            : registry(&reg),
              desc(d) {
            provides[0] = init::cap_id(desc.msc_cap_name);
            provides[1] = init::cap_id(desc.cdc_cap_name);
            requires_caps[0] = init::cap_id("block.registry");
            requires_caps[1] = init::cap_id(desc.block_cap);
            node = init::Node{
                desc.cap_name,
                phase,
                runlevel_mask,
                std::span<const init::CapId>(provides.data(), provides.size()),
                std::span<const init::CapId>(requires_caps.data(), requires_caps.size()),
                &MscCdcBinding::init_trampoline,
                &MscCdcBinding::deinit_trampoline,
                this
            };
        }

        constexpr std::string_view capability_name(init::CapId id) const noexcept {
            if (id == provides[0]) {
                return std::string_view{desc.msc_cap_name ? desc.msc_cap_name : ""};
            }
            if (id == provides[1]) {
                return std::string_view{desc.cdc_cap_name ? desc.cdc_cap_name : ""};
            }
            if (id == requires_caps[0]) {
                return std::string_view{registry_cap_name};
            }
            if (id == requires_caps[1]) {
                return std::string_view{desc.block_cap ? desc.block_cap : ""};
            }
            return {};
        }

        static util::Result<void> init_trampoline(void* ctx) noexcept {
            auto* self = static_cast<MscCdcBinding*>(ctx);
            if (!self || !self->registry) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if (!self->desc.dcd.ep.open || !self->desc.dcd.ep.send) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if (self->desc.block_cap == nullptr) {
                return util::unexpected(util::Errc::invalid_arg);
            }

            auto* block_dev = self->registry->open_device(self->desc.block_cap);
            if (!block_dev) {
                return util::unexpected(util::Errc::noent);
            }

            const auto block_size = block_dev->block_size;
            if (block_size == 0 || block_size > self->io_buf.size()) {
                return util::unexpected(util::Errc::buffer_overflow);
            }

            auto storage = usb::class_driver::make_storage_from_block_device(
                *block_dev, self->desc.storage_cfg);
            self->luns[0] = storage;

            self->bot.emplace(
                std::span<usb::class_driver::MscStorage>(self->luns.data(), self->luns.size()),
                std::span<usb::u8>(self->io_buf.data(), self->io_buf.size()));
            auto msc_ops = usb::class_driver::make_msc_ops(*self->bot);
            self->msc.emplace(&(*self->bot), msc_ops);
            self->msc->set_config(self->desc.msc_cfg);

            self->cdc.emplace(self, make_cdc_ops(*self));
            self->cdc->set_config(self->desc.cdc_cfg);

            self->mux = {};
            self->cdc_eps = {
                self->desc.cdc_cfg.ep_notify,
                self->desc.cdc_cfg.ep_out,
                self->desc.cdc_cfg.ep_in,
            };
            self->msc_eps = {
                self->desc.msc_cfg.ep_out,
                self->desc.msc_cfg.ep_in,
            };

            if (!self->mux.add_slot(ClassMuxSlot{
                    &(*self->cdc),
                    self->cdc->class_ops(),
                    self->desc.cdc_cfg.ctrl_ifc,
                    2,
                    self->cdc_eps.data(),
                    self->cdc_eps.size(),
                })) {
                return util::unexpected(util::Errc::buffer_overflow);
            }

            if (!self->mux.add_slot(ClassMuxSlot{
                    &(*self->msc),
                    self->msc->class_ops(),
                    self->desc.msc_cfg.interface_number,
                    1,
                    self->msc_eps.data(),
                    self->msc_eps.size(),
                })) {
                return util::unexpected(util::Errc::buffer_overflow);
            }

            usb::dsl::DeviceBuildContext build_ctx{
                std::span<usb::u8>(self->dev_desc.data(), self->dev_desc.size()),
                std::span<usb::u8>(self->cfg_desc.data(), self->cfg_desc.size()),
                &self->table,
                &self->tree,
            };

            const auto strings_ptr = self->desc.strings.empty() ? nullptr : self->desc.strings.data();
            const auto strings_count = self->desc.strings.size();
            const auto cdc_class_desc = usb::dsl::make_cdc_acm_class_descriptors(self->desc.cdc_cfg);

            if (!usb::dsl::build_msc_cdc_device(
                    build_ctx,
                    self->desc.dev_info,
                    self->desc.cfg_info,
                    self->desc.msc_cfg,
                    usb::dsl::MscClassDescriptors{}.view(),
                    self->desc.cdc_cfg,
                    cdc_class_desc.view(),
                    strings_ptr,
                    strings_count)) {
                return util::unexpected(util::Errc::io);
            }

            if (!attach_composite_device(self->dev, self->mux, self->table, self->tree)) {
                return util::unexpected(util::Errc::io);
            }

            self->driver.emplace(self->dev, self->desc.dcd_ctx, self->desc.dcd);
            self->driver->bind_cdc(*self->cdc);
            self->driver->bind_msc(*self->msc, *self->bot);
            if (self->desc.adapter) {
                self->desc.adapter->callbacks = self->driver->callbacks();
            }
            if (self->desc.on_ready) {
                self->desc.on_ready(self->desc.on_ready_ctx, &(*self->bot), &self->desc.msc_cfg);
            }
            if (self->desc.dcd.connect) {
                self->desc.dcd.connect(self->desc.dcd_ctx, true);
            }
            return {};
        }

        static void deinit_trampoline(void* ctx) noexcept {
            auto* self = static_cast<MscCdcBinding*>(ctx);
            if (!self) return;
            if (self->driver) {
                self->driver->stop_class_endpoints();
            }
            if (self->desc.dcd.connect) {
                self->desc.dcd.connect(self->desc.dcd_ctx, false);
            }
            self->driver.reset();
            self->cdc.reset();
            self->msc.reset();
            self->bot.reset();
            self->cdc_tx_len = 0;
            self->cdc_rx_len = 0;
            self->mux = {};
        }

    private:
        static bool attach_composite_device(Device& dev,
                                            CompositeClassMux<2>& mux,
                                            DescriptorTable& table,
                                            ConfigTree& config_tree) noexcept {
            if (table.device.size() >= sizeof(DeviceDescriptor)) {
                const auto* desc = reinterpret_cast<const DeviceDescriptor*>(table.device.data());
                dev.set_max_packet_size0(desc->max_packet_size0);
            }
            dev.set_class(&mux, mux.class_ops());
            table.configuration = config_tree.view;
            dev.set_descriptor_provider(make_descriptor_provider(table));
            return !table.configuration.empty();
        }

        static usb::class_driver::CdcOps make_cdc_ops(MscCdcBinding& self) noexcept {
            usb::class_driver::CdcOps ops{};
            ops.tx_buffer = [] (void* ctx) noexcept -> std::span<usb::u8> {
                auto* self = static_cast<MscCdcBinding*>(ctx);
                return self ? std::span<usb::u8>(self->cdc_tx_buf.data(), self->cdc_tx_buf.size()) : std::span<usb::u8>{};
            };
            ops.rx_buffer = [] (void* ctx) noexcept -> std::span<usb::u8> {
                auto* self = static_cast<MscCdcBinding*>(ctx);
                return self ? std::span<usb::u8>(self->cdc_rx_buf.data(), self->cdc_rx_buf.size()) : std::span<usb::u8>{};
            };
            ops.tx_length = [] (void* ctx) noexcept -> std::size_t {
                auto* self = static_cast<MscCdcBinding*>(ctx);
                return self ? self->cdc_tx_len : 0;
            };
            ops.on_rx_done = [] (void* ctx, std::size_t len) noexcept {
                auto* self = static_cast<MscCdcBinding*>(ctx);
                if (self) {
                    self->cdc_rx_len = len;
                }
            };
            ops.on_tx_done = [] (void* ctx, std::size_t len) noexcept {
                auto* self = static_cast<MscCdcBinding*>(ctx);
                if (!self) return;
                if (len >= self->cdc_tx_len) {
                    self->cdc_tx_len = 0;
                    return;
                }
                const auto remain = self->cdc_tx_len - len;
                for (std::size_t index = 0; index < remain; ++index) {
                    self->cdc_tx_buf[index] = self->cdc_tx_buf[index + len];
                }
                self->cdc_tx_len = remain;
            };
            ops.notify = [] (void* ctx, std::span<const usb::u8> data) noexcept -> bool {
                auto* self = static_cast<MscCdcBinding*>(ctx);
                if (!self || !self->desc.dcd.ep.send) return false;
                return self->desc.dcd.ep.send(self->desc.dcd_ctx,
                                              self->desc.cdc_cfg.ep_notify,
                                              data,
                                              false);
            };
            return ops;
        }
    };
}
