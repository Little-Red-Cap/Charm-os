module;

#include <string_view>

export module usb.host.runtime_block;

import block.device;
import block.device.slot_export;
import block.registry;
import device.bus;
import device.desc;
import device.registry;
import device.runtime_driver;
import device.types;
import usb.host.runtime;
import util.core;
import util.error;

namespace usb::host::detail {
    template <typename BlockRegistryT>
    struct MscBlockDriverContext {
        block::DeviceSlotExport<BlockRegistryT>* exported{nullptr};
        block::Device* backend{nullptr};
        device::DeviceDesc match{};
    };

    template <typename BlockRegistryT>
    bool msc_block_probe(MscBlockDriverContext<BlockRegistryT>& ctx,
                         device::Device& dev) noexcept {
        return ctx.exported != nullptr &&
               ctx.backend != nullptr &&
               same_desc(dev.desc, ctx.match);
    }

    template <typename BlockRegistryT>
    bool msc_block_init(MscBlockDriverContext<BlockRegistryT>& ctx,
                        device::Device&) noexcept {
        if (!ctx.exported || !ctx.backend) {
            return false;
        }
        return static_cast<bool>(ctx.exported->attach(*ctx.backend));
    }

    template <typename BlockRegistryT>
    void msc_block_detach(MscBlockDriverContext<BlockRegistryT>& ctx) noexcept {
        if (ctx.exported) {
            ctx.exported->detach();
        }
    }

    template <typename BlockRegistryT>
    void msc_block_shutdown(MscBlockDriverContext<BlockRegistryT>& ctx,
                            device::Device&) noexcept {
        msc_block_detach(ctx);
    }

    template <typename BlockRegistryT>
    void msc_block_remove(MscBlockDriverContext<BlockRegistryT>& ctx,
                          device::Device&) noexcept {
        msc_block_detach(ctx);
    }
}

export namespace usb::host {
    inline constexpr device::DeviceDesc make_msc_device_desc(
        util::u16 vendor_id,
        util::u16 product_id,
        std::string_view type = "usb.host.msc") noexcept {
        return device::DeviceDesc{
            .class_id = 0x08,
            .vendor_id = vendor_id,
            .product_id = product_id,
            .type = type
        };
    }

    template <typename BlockRegistryT>
    class MscBlockRuntimeBinding {
    public:
        using DriverContext = detail::MscBlockDriverContext<BlockRegistryT>;

        MscBlockRuntimeBinding(BlockRegistryT& registry,
                               std::string_view cap_name,
                               block::Device& backend,
                               const device::DeviceDesc& match,
                               const char* driver_name = "usb.host.msc.runtime",
                               const char* bus_name = "usb.host",
                               util::u32 priority = 0) noexcept
            : exported_(registry, cap_name),
              runtime_ctx_{&exported_, &backend, match},
              binding_{
                  &runtime_ctx_,
                  device::RuntimeDriverHook<DriverContext>{
                      &detail::msc_block_probe<BlockRegistryT>,
                      &detail::msc_block_init<BlockRegistryT>,
                      &detail::msc_block_shutdown<BlockRegistryT>,
                      &detail::msc_block_remove<BlockRegistryT>,
                      nullptr,
                      nullptr,
                      nullptr
                  }
              },
              discovery_(match, &binding_, bus_name),
              driver_(device::make_runtime_driver<DriverContext>(match, driver_name, priority)) {
        }

        MscBlockRuntimeBinding(BlockRegistryT& registry,
                               std::string_view cap_name,
                               block::Device& backend,
                               util::u16 vendor_id,
                               util::u16 product_id,
                               std::string_view type = "usb.host.msc",
                               const char* driver_name = "usb.host.msc.runtime",
                               const char* bus_name = "usb.host",
                               util::u32 priority = 0) noexcept
            : MscBlockRuntimeBinding(registry,
                                     cap_name,
                                     backend,
                                     make_msc_device_desc(vendor_id, product_id, type),
                                     driver_name,
                                     bus_name,
                                     priority) {
        }

        MscBlockRuntimeBinding(const MscBlockRuntimeBinding&) = delete;
        MscBlockRuntimeBinding& operator=(const MscBlockRuntimeBinding&) = delete;
        MscBlockRuntimeBinding(MscBlockRuntimeBinding&&) = delete;
        MscBlockRuntimeBinding& operator=(MscBlockRuntimeBinding&&) = delete;

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

        void set_backend(block::Device& backend) noexcept {
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

        [[nodiscard]] block::Device* backend() const noexcept {
            return runtime_ctx_.backend;
        }

        [[nodiscard]] const device::DeviceDesc& match() const noexcept {
            return runtime_ctx_.match;
        }

        [[nodiscard]] RuntimeDeviceRecord device_record() const noexcept {
            return RuntimeDeviceRecord{
                runtime_ctx_.match,
                const_cast<device::RuntimeDriverBinding<DriverContext>*>(&binding_),
                false
            };
        }

        block::DeviceSlotExport<BlockRegistryT>& exported_slot() noexcept {
            return exported_;
        }

        const block::DeviceSlotExport<BlockRegistryT>& exported_slot() const noexcept {
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
        block::DeviceSlotExport<BlockRegistryT> exported_;
        DriverContext runtime_ctx_{};
        device::RuntimeDriverBinding<DriverContext> binding_{};
        SingleDeviceRuntimeBus discovery_;
        device::Driver driver_{};
    };
}
