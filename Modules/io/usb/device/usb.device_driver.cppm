module;

#include <cstddef>
#include <span>

export module usb.device_driver;

import usb.common;
import usb.class_cdc;
import usb.device;
import usb.driver;
import usb.ep0_driver;
import usb.dsl;
import device.desc;
import device.types;
import util.core;

export namespace usb::device {
    class DeviceDriver;

    struct DeviceModelHook {
        Device* dev{nullptr};
        DeviceDriver* driver{nullptr};
        void (*start)(DeviceDriver&) noexcept { nullptr };
        void (*stop)(DeviceDriver&) noexcept { nullptr };
    };

    inline ::device::Driver make_device_driver(DeviceModelHook* hook,
                                             const ::device::DeviceDesc& match,
                                             const char* name,
                                             util::u32 priority) noexcept {
        ::device::Driver drv{};
        drv.name = name;
        drv.match = match;
        drv.priority = priority;
        drv.ops.probe = [](::device::Device& dev) noexcept -> bool {
            auto* h = static_cast<DeviceModelHook*>(dev.ctx);
            return h && h->driver != nullptr;
        };
        drv.ops.init = [](::device::Device& dev) noexcept -> bool {
            auto* h = static_cast<DeviceModelHook*>(dev.ctx);
            if (!h || !h->driver) return false;
            if (h->dev) h->dev->reset();
            if (h->start) h->start(*h->driver);
            return true;
        };
        drv.ops.shutdown = [](::device::Device& dev) noexcept {
            auto* h = static_cast<DeviceModelHook*>(dev.ctx);
            if (!h || !h->driver) return;
            if (h->stop) h->stop(*h->driver);
        };
        drv.ops.remove = drv.ops.shutdown;
        drv.ops.suspend = [](::device::Device& dev) noexcept -> bool {
            auto* h = static_cast<DeviceModelHook*>(dev.ctx);
            if (!h || !h->driver) return false;
            if (h->stop) h->stop(*h->driver);
            return true;
        };
        drv.ops.resume = [](::device::Device& dev) noexcept -> bool {
            auto* h = static_cast<DeviceModelHook*>(dev.ctx);
            if (!h || !h->driver) return false;
            if (h->start) h->start(*h->driver);
            return true;
        };
        return drv;
    }

    class DeviceDriver {
    public:
        DeviceDriver(Device& dev, void* dcd_ctx, driver::DcdOps ops) noexcept
            : dev_(dev),
              dcd_ctx_(dcd_ctx),
              dcd_ops_(ops),
              ep0_(dev_, this, device::Ep0DriverOps{
                  &DeviceDriver::ep0_send_in,
                  &DeviceDriver::ep0_send_zlp,
                  &DeviceDriver::ep0_stall}) {}

        void on_setup(const SetupPacket& setup) noexcept {
            ControlResponse resp{};
            set_pending(setup);
            const auto result = ep0_.on_setup(setup, resp);
            if (result != Ep0Result::ok) {
                clear_pending();
            }
        }

        void on_out_data(std::span<const u8> data) noexcept {
            ControlResponse resp{};
            const auto result = ep0_.on_out_data(data, resp);
            if (result != Ep0Result::ok) {
                clear_pending();
            }
        }

        void on_in_complete(std::size_t sent, bool sent_zlp = false) noexcept {
            ep0_.on_in_complete(sent, sent_zlp);
            apply_pending_if_ready();
        }

        void on_reset() noexcept {
            clear_pending();
            dev_.reset();
            if (dcd_ops_.set_address) {
                dcd_ops_.set_address(dcd_ctx_, 0);
            }
            if (dcd_ops_.set_configured) {
                dcd_ops_.set_configured(dcd_ctx_, false);
            }
        }

        driver::DcdDeviceCallbacks callbacks() noexcept {
            driver::DcdDeviceCallbacks cb{};
            cb.ctx = this;
            cb.on_setup = &DeviceDriver::handle_setup_cb;
            cb.on_out_data = &DeviceDriver::handle_out_cb;
            cb.on_in_complete = &DeviceDriver::handle_in_complete_cb;
            cb.on_reset = &DeviceDriver::handle_reset_cb;
            return cb;
        }

    private:
        static void handle_setup_cb(void* ctx, const SetupPacket& setup) noexcept {
            auto* self = static_cast<DeviceDriver*>(ctx);
            if (self) self->on_setup(setup);
        }

        static void handle_out_cb(void* ctx, std::span<const u8> data) noexcept {
            auto* self = static_cast<DeviceDriver*>(ctx);
            if (self) self->on_out_data(data);
        }

        static void handle_in_complete_cb(void* ctx, std::size_t sent, bool sent_zlp) noexcept {
            auto* self = static_cast<DeviceDriver*>(ctx);
            if (self) self->on_in_complete(sent, sent_zlp);
        }

        static void handle_reset_cb(void* ctx) noexcept {
            auto* self = static_cast<DeviceDriver*>(ctx);
            if (self) self->on_reset();
        }

        static Ep0Result ep0_send_in(void* ctx, std::span<const u8> data, bool zlp) noexcept {
            auto* self = static_cast<DeviceDriver*>(ctx);
            if (!self || !self->dcd_ops_.ep.send) return Ep0Result::stall;
            const auto ok = self->dcd_ops_.ep.send(self->dcd_ctx_, 0x80, data, zlp);
            return ok ? Ep0Result::ok : Ep0Result::stall;
        }

        static Ep0Result ep0_send_zlp(void* ctx) noexcept {
            auto* self = static_cast<DeviceDriver*>(ctx);
            if (!self || !self->dcd_ops_.ep.send) return Ep0Result::stall;
            const auto ok = self->dcd_ops_.ep.send(self->dcd_ctx_, 0x80, {}, true);
            return ok ? Ep0Result::ok : Ep0Result::stall;
        }

        static Ep0Result ep0_stall(void* ctx) noexcept {
            auto* self = static_cast<DeviceDriver*>(ctx);
            if (!self || !self->dcd_ops_.ep.stall) return Ep0Result::stall;
            self->dcd_ops_.ep.stall(self->dcd_ctx_, 0x00);
            self->dcd_ops_.ep.stall(self->dcd_ctx_, 0x80);
            return Ep0Result::stall;
        }

        void set_pending(const SetupPacket& setup) noexcept {
            if (request_type(setup.bm_request_type) != RequestType::standard) {
                return;
            }
            switch (static_cast<StandardRequest>(setup.b_request)) {
            case StandardRequest::set_address:
                pending_address_valid_ = true;
                pending_address_ = static_cast<u8>(setup.w_value & 0x7F);
                break;
            case StandardRequest::set_configuration:
                pending_config_valid_ = true;
                pending_configured_ = (static_cast<u8>(setup.w_value & 0xFF) != 0);
                break;
            default:
                break;
            }
        }

        void apply_pending_if_ready() noexcept {
            if (dev_.stage() != Ep0Stage::status_out) {
                return;
            }
            if (pending_address_valid_) {
                if (dcd_ops_.set_address) {
                    dcd_ops_.set_address(dcd_ctx_, pending_address_);
                }
                pending_address_valid_ = false;
            }
            if (pending_config_valid_) {
                if (dcd_ops_.set_configured) {
                    dcd_ops_.set_configured(dcd_ctx_, pending_configured_);
                }
                pending_config_valid_ = false;
            }
        }

        void clear_pending() noexcept {
            pending_address_valid_ = false;
            pending_config_valid_ = false;
        }

        Device& dev_;
        void* dcd_ctx_{nullptr};
        driver::DcdOps dcd_ops_{};
        Ep0Driver ep0_;
        bool pending_address_valid_{false};
        u8 pending_address_{0};
        bool pending_config_valid_{false};
        bool pending_configured_{false};
    };

    namespace examples {
        struct CdcEndpointCallbacks {
            driver::EpCallbacks out{};
            driver::EpCallbacks in{};
        };

        inline CdcEndpointCallbacks make_cdc_ep_callbacks(class_driver::CdcAcm& cdc) noexcept {
            CdcEndpointCallbacks cb{};
            cb.out.on_out = [] (void* ctx, std::span<const u8> data) noexcept {
                auto* self = static_cast<class_driver::CdcAcm*>(ctx);
                if (self) self->on_out_packet(data);
            };
            cb.in.on_in_complete = [] (void* ctx, std::size_t sent, bool) noexcept {
                auto* self = static_cast<class_driver::CdcAcm*>(ctx);
                if (self) self->on_tx_done(sent);
            };
            cb.out.on_stall = nullptr;
            cb.in.on_stall = nullptr;
            cb.out.on_in_complete = nullptr;
            return cb;
        }

        inline bool send_cdc_in_packet(const driver::DcdOps& dcd,
                                       void* dcd_ctx,
                                       class_driver::CdcAcm& cdc,
                                       std::size_t max_len) noexcept {
            if (!dcd.ep.send) return false;
            auto data = cdc.on_in_request(max_len);
            if (data.empty()) return false;
            const auto ep = cdc.config().ep_in;
            return dcd.ep.send(dcd_ctx, ep, data, false);
        }

        inline bool attach_cdc_acm(Device& dev,
                                   class_driver::CdcAcm& cdc,
                                   DescriptorTable& table,
                                   ConfigTree& config_tree) noexcept {
            if (table.device.size() >= sizeof(DeviceDescriptor)) {
                const auto* desc = reinterpret_cast<const DeviceDescriptor*>(table.device.data());
                dev.set_max_packet_size0(desc->max_packet_size0);
            }
            dev.set_class(&cdc, cdc.class_ops());
            table.configuration = config_tree.view;
            dev.set_descriptor_provider(make_descriptor_provider(table));
            return !table.configuration.empty();
        }

        inline bool build_and_attach_cdc_acm(Device& dev,
                                             class_driver::CdcAcm& cdc,
                                             dsl::DeviceBuildContext& build_ctx,
                                             const dsl::DeviceInfo& dev_info,
                                             const dsl::ConfigInfo& cfg_info,
                                             const class_driver::CdcConfig& cdc_cfg,
                                             std::span<const u8> class_desc,
                                             const std::span<const u8>* strings,
                                             std::size_t string_count) noexcept {
            if (!dsl::build_cdc_acm_device(build_ctx,
                                           dev_info,
                                           cfg_info,
                                           cdc_cfg,
                                           class_desc,
                                           strings,
                                           string_count)) {
                return false;
            }
            return attach_cdc_acm(dev, cdc, *build_ctx.table, *build_ctx.tree);
        }
    }
}
