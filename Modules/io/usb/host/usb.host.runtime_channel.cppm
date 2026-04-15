module;

#include <string_view>

export module usb.host.runtime_channel;

import device.bus;
import device.desc;
import device.registry;
import device.runtime_driver;
import device.types;
import io.channel;
import io.channel.slot_export;
import io.reactor;
import io.registry;
import usb.host.runtime;
import util.core;
import util.error;

namespace usb::host::detail {
    template <typename IoRegistryT>
    struct CdcChannelDriverContext {
        io::ChannelSlotExport<IoRegistryT>* exported{nullptr};
        io::Channel* backend{nullptr};
        device::DeviceDesc match{};
    };

    template <typename IoRegistryT>
    bool cdc_channel_probe(CdcChannelDriverContext<IoRegistryT>& ctx,
                           device::Device& dev) noexcept {
        return ctx.exported != nullptr &&
               ctx.backend != nullptr &&
               same_desc(dev.desc, ctx.match);
    }

    template <typename IoRegistryT>
    bool cdc_channel_init(CdcChannelDriverContext<IoRegistryT>& ctx,
                          device::Device&) noexcept {
        if (!ctx.exported || !ctx.backend) {
            return false;
        }
        return static_cast<bool>(ctx.exported->attach(*ctx.backend));
    }

    template <typename IoRegistryT>
    util::Result<void> cdc_channel_try_init(CdcChannelDriverContext<IoRegistryT>& ctx,
                                            device::Device&) noexcept {
        if (!ctx.exported || !ctx.backend) {
            return util::unexpected(util::Errc::bad_state);
        }
        return ctx.exported->attach(*ctx.backend);
    }

    template <typename IoRegistryT>
    void cdc_channel_detach(CdcChannelDriverContext<IoRegistryT>& ctx) noexcept {
        if (ctx.exported) {
            ctx.exported->detach();
        }
    }

    template <typename IoRegistryT>
    void cdc_channel_shutdown(CdcChannelDriverContext<IoRegistryT>& ctx,
                              device::Device&) noexcept {
        cdc_channel_detach(ctx);
    }

    template <typename IoRegistryT>
    void cdc_channel_remove(CdcChannelDriverContext<IoRegistryT>& ctx,
                            device::Device&) noexcept {
        cdc_channel_detach(ctx);
    }
}

export namespace usb::host {
    inline constexpr device::DeviceDesc make_cdc_device_desc(
        util::u16 vendor_id,
        util::u16 product_id,
        std::string_view type = "usb.host.cdc") noexcept {
        return device::DeviceDesc{
            .class_id = 0x02,
            .vendor_id = vendor_id,
            .product_id = product_id,
            .type = type
        };
    }

    template <typename IoRegistryT>
    class CdcChannelRuntimeBinding {
    public:
        using DriverContext = detail::CdcChannelDriverContext<IoRegistryT>;

        CdcChannelRuntimeBinding(IoRegistryT& registry,
                                 std::string_view cap_name,
                                 io::Channel& backend,
                                 const device::DeviceDesc& match,
                                 io::EndpointCaps caps = io::EndpointCaps::duplex,
                                 io::Reactor* reactor = nullptr,
                                 const char* driver_name = "usb.host.cdc.runtime",
                                 const char* bus_name = "usb.host",
                                 util::u32 priority = 0) noexcept
            : exported_(registry, cap_name, caps, reactor),
              runtime_ctx_{&exported_, &backend, match},
              binding_{
                  &runtime_ctx_,
                  device::RuntimeDriverHook<DriverContext>{
                      .probe = &detail::cdc_channel_probe<IoRegistryT>,
                      .init = &detail::cdc_channel_init<IoRegistryT>,
                      .shutdown = &detail::cdc_channel_shutdown<IoRegistryT>,
                      .remove = &detail::cdc_channel_remove<IoRegistryT>,
                      .try_init = &detail::cdc_channel_try_init<IoRegistryT>
                  }
              },
              discovery_(match, &binding_, bus_name),
              driver_(device::make_runtime_driver<DriverContext>(match, driver_name, priority)) {
        }

        CdcChannelRuntimeBinding(IoRegistryT& registry,
                                 std::string_view cap_name,
                                 io::Channel& backend,
                                 util::u16 vendor_id,
                                 util::u16 product_id,
                                 std::string_view type = "usb.host.cdc",
                                 io::EndpointCaps caps = io::EndpointCaps::duplex,
                                 io::Reactor* reactor = nullptr,
                                 const char* driver_name = "usb.host.cdc.runtime",
                                 const char* bus_name = "usb.host",
                                 util::u32 priority = 0) noexcept
            : CdcChannelRuntimeBinding(registry,
                                       cap_name,
                                       backend,
                                       make_cdc_device_desc(vendor_id, product_id, type),
                                       caps,
                                       reactor,
                                       driver_name,
                                       bus_name,
                                       priority) {
        }

        CdcChannelRuntimeBinding(const CdcChannelRuntimeBinding&) = delete;
        CdcChannelRuntimeBinding& operator=(const CdcChannelRuntimeBinding&) = delete;
        CdcChannelRuntimeBinding(CdcChannelRuntimeBinding&&) = delete;
        CdcChannelRuntimeBinding& operator=(CdcChannelRuntimeBinding&&) = delete;

        util::Result<void> ensure_exported() noexcept {
            return exported_.ensure_exported();
        }

        template <typename RuntimeRegistryT>
        util::Result<void> try_enumerate(RuntimeRegistryT& registry) noexcept {
            return discovery_.try_enumerate(registry);
        }

        template <typename RuntimeRegistryT>
        bool enumerate(RuntimeRegistryT& registry) noexcept {
            return static_cast<bool>(try_enumerate(registry));
        }

        device::Bus bus() const noexcept {
            return discovery_.bus();
        }

        const device::Driver& driver() const noexcept {
            return driver_;
        }

        void set_backend(io::Channel& backend) noexcept {
            runtime_ctx_.backend = &backend;
        }

        void reset_enumeration() noexcept {
            discovery_.reset_enumeration();
        }

        [[nodiscard]] bool enumerated() const noexcept {
            return discovery_.enumerated();
        }

        [[nodiscard]] bool exported() const noexcept {
            return exported_.exported();
        }

        [[nodiscard]] bool attached() const noexcept {
            return exported_.attached();
        }

        [[nodiscard]] util::u32 generation() const noexcept {
            return exported_.generation();
        }

        [[nodiscard]] io::Channel* backend() const noexcept {
            return runtime_ctx_.backend;
        }

        [[nodiscard]] const device::DeviceDesc& match() const noexcept {
            return runtime_ctx_.match;
        }

        [[nodiscard]] io::Reactor* reactor() const noexcept {
            return exported_.reactor();
        }

        [[nodiscard]] RuntimeDeviceRecord device_record() const noexcept {
            return RuntimeDeviceRecord{
                runtime_ctx_.match,
                const_cast<device::RuntimeDriverBinding<DriverContext>*>(&binding_),
                false
            };
        }

        io::ChannelSlotExport<IoRegistryT>& exported_slot() noexcept {
            return exported_;
        }

        const io::ChannelSlotExport<IoRegistryT>& exported_slot() const noexcept {
            return exported_;
        }

        template <typename RuntimeRegistryT>
        device::Device* find_device(RuntimeRegistryT& registry) noexcept {
            void* binding_ctx = &binding_;
            for (util::usize i = 0; i < registry.device_count(); ++i) {
                auto& dev = registry.device_at(i);
                if (dev.ctx == binding_ctx && same_desc(dev.desc, runtime_ctx_.match)) {
                    return &dev;
                }
            }
            return nullptr;
        }

        template <typename RuntimeRegistryT>
        const device::Device* find_device(const RuntimeRegistryT& registry) const noexcept {
            const void* binding_ctx = &binding_;
            for (util::usize i = 0; i < registry.device_count(); ++i) {
                const auto& dev = registry.device_at(i);
                if (dev.ctx == binding_ctx && same_desc(dev.desc, runtime_ctx_.match)) {
                    return &dev;
                }
            }
            return nullptr;
        }

        template <typename RuntimeRegistryT>
        util::Result<void> try_remove(RuntimeRegistryT& registry) noexcept {
            return registry.try_remove_matching(runtime_ctx_.match, &binding_);
        }

        template <typename RuntimeRegistryT>
        bool remove(RuntimeRegistryT& registry) noexcept {
            return static_cast<bool>(try_remove(registry));
        }

    private:
        io::ChannelSlotExport<IoRegistryT> exported_;
        DriverContext runtime_ctx_{};
        device::RuntimeDriverBinding<DriverContext> binding_{};
        SingleDeviceRuntimeBus discovery_;
        device::Driver driver_{};
    };
}
