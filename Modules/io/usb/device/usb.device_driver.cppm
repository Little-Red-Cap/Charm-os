module;

#include <cstddef>
#include <span>

export module usb.device_driver;

import usb.common;
import usb.class_cdc;
import usb.class_msc;
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
                return;
            }
            apply_address_immediately(setup);
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
            apply_pending_if_ready(sent_zlp);
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

        void on_connect(bool) noexcept {
        }

        void on_suspend() noexcept {
        }

        void on_resume() noexcept {
        }

        driver::DcdDeviceCallbacks callbacks() noexcept {
            driver::DcdDeviceCallbacks cb{};
            cb.ctx = this;
            cb.on_setup = &DeviceDriver::handle_setup_cb;
            cb.on_out_data = &DeviceDriver::handle_out_cb;
            cb.on_in_complete = &DeviceDriver::handle_in_complete_cb;
            cb.on_reset = &DeviceDriver::handle_reset_cb;
            cb.on_connect = &DeviceDriver::handle_connect_cb;
            cb.on_suspend = &DeviceDriver::handle_suspend_cb;
            cb.on_resume = &DeviceDriver::handle_resume_cb;
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

        static void handle_connect_cb(void* ctx, bool connected) noexcept {
            auto* self = static_cast<DeviceDriver*>(ctx);
            if (self) self->on_connect(connected);
        }

        static void handle_suspend_cb(void* ctx) noexcept {
            auto* self = static_cast<DeviceDriver*>(ctx);
            if (self) self->on_suspend();
        }

        static void handle_resume_cb(void* ctx) noexcept {
            auto* self = static_cast<DeviceDriver*>(ctx);
            if (self) self->on_resume();
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

        void apply_pending_if_ready(bool sent_zlp) noexcept {
            if (dev_.stage() != Ep0Stage::setup) {
                return;
            }
            if (pending_address_valid_) {
                if (!sent_zlp) {
                    return;
                }
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

        void apply_address_immediately(const SetupPacket& setup) noexcept {
            if (request_type(setup.bm_request_type) != RequestType::standard) {
                return;
            }
            if (static_cast<StandardRequest>(setup.b_request) != StandardRequest::set_address) {
                return;
            }
            if (pending_address_valid_ && dcd_ops_.set_address) {
                dcd_ops_.set_address(dcd_ctx_, pending_address_);
                pending_address_valid_ = false;
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

        inline bool send_cdc_serial_state(const driver::DcdOps& dcd,
                                          void* dcd_ctx,
                                          class_driver::CdcAcm& cdc,
                                          u16 state_bits) noexcept {
            if (!dcd.ep.send) return false;
            const auto ep = cdc.config().ep_notify;
            auto data = cdc.serial_state_notification(state_bits);
            return dcd.ep.send(dcd_ctx, ep, data, false);
        }

        inline bool open_cdc_endpoints(const driver::DcdOps& dcd,
                                       void* dcd_ctx,
                                       class_driver::CdcAcm& cdc,
                                       const CdcEndpointCallbacks& cb) noexcept {
            if (!dcd.ep.open) return false;

            const auto cfg = cdc.config();
            driver::EpConfig notify{};
            notify.address = cfg.ep_notify;
            notify.direction = driver::EpDirection::in;
            notify.type = driver::EpType::interrupt;
            notify.max_packet_size = cfg.ep_mps;
            notify.interval = 10;

            driver::EpConfig out{};
            out.address = cfg.ep_out;
            out.direction = driver::EpDirection::out;
            out.type = driver::EpType::bulk;
            out.max_packet_size = cfg.ep_mps;

            driver::EpConfig in{};
            in.address = cfg.ep_in;
            in.direction = driver::EpDirection::in;
            in.type = driver::EpType::bulk;
            in.max_packet_size = cfg.ep_mps;

            driver::EpCallbacks notify_cb = cb.in;
            if (!dcd.ep.open(dcd_ctx, notify, notify_cb)) return false;
            if (!dcd.ep.open(dcd_ctx, out, cb.out)) return false;
            if (!dcd.ep.open(dcd_ctx, in, cb.in)) return false;
            return true;
        }

        inline void close_cdc_endpoints(const driver::DcdOps& dcd,
                                        void* dcd_ctx,
                                        class_driver::CdcAcm& cdc) noexcept {
            if (!dcd.ep.close) return;
            const auto cfg = cdc.config();
            dcd.ep.close(dcd_ctx, cfg.ep_notify);
            dcd.ep.close(dcd_ctx, cfg.ep_out);
            dcd.ep.close(dcd_ctx, cfg.ep_in);
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

        inline bool build_attach_open_cdc(Device& dev,
                                          class_driver::CdcAcm& cdc,
                                          dsl::DeviceBuildContext& build_ctx,
                                          const dsl::DeviceInfo& dev_info,
                                          const dsl::ConfigInfo& cfg_info,
                                          const class_driver::CdcConfig& cdc_cfg,
                                          std::span<const u8> class_desc,
                                          const std::span<const u8>* strings,
                                          std::size_t string_count,
                                          const driver::DcdOps& dcd,
                                          void* dcd_ctx,
                                          const CdcEndpointCallbacks& cb) noexcept {
            if (!build_and_attach_cdc_acm(dev, cdc, build_ctx,
                    dev_info, cfg_info, cdc_cfg, class_desc, strings, string_count)) {
                return false;
            }
            return open_cdc_endpoints(dcd, dcd_ctx, cdc, cb);
        }

        struct MscEndpointCallbacks {
            driver::EpCallbacks out{};
            driver::EpCallbacks in{};
        };

        inline MscEndpointCallbacks make_msc_ep_callbacks(class_driver::MscBot& bot) noexcept {
            MscEndpointCallbacks cb{};
            cb.out.on_out = [] (void* ctx, std::span<const u8> data) noexcept {
                auto* self = static_cast<class_driver::MscBot*>(ctx);
                if (self) self->on_out_packet(data);
            };
            cb.in.on_in_complete = nullptr;
            cb.out.on_stall = nullptr;
            cb.in.on_stall = nullptr;
            cb.out.on_in_complete = nullptr;
            return cb;
        }

        inline bool send_msc_in_packet(const driver::DcdOps& dcd,
                                       void* dcd_ctx,
                                       class_driver::MscBot& bot,
                                       const class_driver::MscConfig& cfg) noexcept {
            if (!dcd.ep.send) return false;
            if (!bot.has_in_data()) return false;
            auto data = bot.on_in_request(cfg.ep_mps);
            if (data.empty()) return false;
            return dcd.ep.send(dcd_ctx, cfg.ep_in, data, false);
        }

        inline bool open_msc_endpoints(const driver::DcdOps& dcd,
                                       void* dcd_ctx,
                                       class_driver::MscDevice& msc,
                                       const MscEndpointCallbacks& cb) noexcept {
            (void)msc;
            if (!dcd.ep.open) return false;

            const auto cfg = msc.config();
            driver::EpConfig out{};
            out.address = cfg.ep_out;
            out.direction = driver::EpDirection::out;
            out.type = driver::EpType::bulk;
            out.max_packet_size = cfg.ep_mps;

            driver::EpConfig in{};
            in.address = cfg.ep_in;
            in.direction = driver::EpDirection::in;
            in.type = driver::EpType::bulk;
            in.max_packet_size = cfg.ep_mps;

            if (!dcd.ep.open(dcd_ctx, out, cb.out)) return false;
            if (!dcd.ep.open(dcd_ctx, in, cb.in)) return false;
            return true;
        }

        inline void close_msc_endpoints(const driver::DcdOps& dcd,
                                        void* dcd_ctx,
                                        class_driver::MscDevice& msc) noexcept {
            if (!dcd.ep.close) return;
            const auto cfg = msc.config();
            dcd.ep.close(dcd_ctx, cfg.ep_out);
            dcd.ep.close(dcd_ctx, cfg.ep_in);
        }

        inline bool attach_msc_device(Device& dev,
                                      class_driver::MscDevice& msc,
                                      DescriptorTable& table,
                                      ConfigTree& config_tree) noexcept {
            if (table.device.size() >= sizeof(DeviceDescriptor)) {
                const auto* desc = reinterpret_cast<const DeviceDescriptor*>(table.device.data());
                dev.set_max_packet_size0(desc->max_packet_size0);
            }
            dev.set_class(&msc, msc.class_ops());
            table.configuration = config_tree.view;
            dev.set_descriptor_provider(make_descriptor_provider(table));
            return !table.configuration.empty();
        }

        inline bool build_and_attach_msc(Device& dev,
                                         class_driver::MscDevice& msc,
                                         dsl::DeviceBuildContext& build_ctx,
                                         const dsl::DeviceInfo& dev_info,
                                         const dsl::ConfigInfo& cfg_info,
                                         const class_driver::MscConfig& msc_cfg,
                                         std::span<const u8> class_desc,
                                         const std::span<const u8>* strings,
                                         std::size_t string_count) noexcept {
            if (!dsl::build_msc_device(build_ctx,
                                       dev_info,
                                       cfg_info,
                                       msc_cfg,
                                       class_desc,
                                       strings,
                                       string_count)) {
                return false;
            }
            return attach_msc_device(dev, msc, *build_ctx.table, *build_ctx.tree);
        }

        inline bool build_attach_open_msc(Device& dev,
                                          class_driver::MscDevice& msc,
                                          dsl::DeviceBuildContext& build_ctx,
                                          const dsl::DeviceInfo& dev_info,
                                          const dsl::ConfigInfo& cfg_info,
                                          const class_driver::MscConfig& msc_cfg,
                                          std::span<const u8> class_desc,
                                          const std::span<const u8>* strings,
                                          std::size_t string_count,
                                          const driver::DcdOps& dcd,
                                          void* dcd_ctx,
                                          const MscEndpointCallbacks& cb) noexcept {
            if (!build_and_attach_msc(dev, msc, build_ctx,
                                      dev_info, cfg_info, msc_cfg,
                                      class_desc, strings, string_count)) {
                return false;
            }
            return open_msc_endpoints(dcd, dcd_ctx, msc, cb);
        }
    }
}
