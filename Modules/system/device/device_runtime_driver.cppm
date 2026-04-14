module;

export module device.runtime_driver;

import device.desc;
import device.registry;
import device.types;
import util.core;

export namespace device {
    template <typename ContextT>
    struct RuntimeDriverHook {
        bool (*probe)(ContextT& ctx, Device& dev) noexcept { nullptr };
        bool (*init)(ContextT& ctx, Device& dev) noexcept { nullptr };
        void (*shutdown)(ContextT& ctx, Device& dev) noexcept { nullptr };
        void (*remove)(ContextT& ctx, Device& dev) noexcept { nullptr };
        bool (*suspend)(ContextT& ctx, Device& dev) noexcept { nullptr };
        bool (*resume)(ContextT& ctx, Device& dev) noexcept { nullptr };
        void (*on_event)(ContextT& ctx, Device& dev, DeviceEvent ev) noexcept { nullptr };
    };

    template <typename ContextT>
    struct RuntimeDriverBinding {
        ContextT* ctx{nullptr};
        RuntimeDriverHook<ContextT> hook{};
    };
}

namespace device::detail {
    template <typename ContextT>
    RuntimeDriverBinding<ContextT>* runtime_binding(Device& dev) noexcept {
        return static_cast<RuntimeDriverBinding<ContextT>*>(dev.ctx);
    }

    template <typename ContextT>
    ContextT* runtime_context(Device& dev) noexcept {
        auto* binding = runtime_binding<ContextT>(dev);
        return binding ? binding->ctx : nullptr;
    }

    template <typename ContextT>
    RuntimeDriverHook<ContextT>* runtime_hook(Device& dev) noexcept {
        auto* binding = runtime_binding<ContextT>(dev);
        return binding ? &binding->hook : nullptr;
    }

    template <typename ContextT>
    bool runtime_probe(Device& dev) noexcept {
        auto* ctx = runtime_context<ContextT>(dev);
        auto* hook = runtime_hook<ContextT>(dev);
        if (!ctx || !hook) {
            return false;
        }
        return hook->probe ? hook->probe(*ctx, dev) : true;
    }

    template <typename ContextT>
    bool runtime_init(Device& dev) noexcept {
        auto* ctx = runtime_context<ContextT>(dev);
        auto* hook = runtime_hook<ContextT>(dev);
        if (!ctx || !hook) {
            return false;
        }
        return hook->init ? hook->init(*ctx, dev) : true;
    }

    template <typename ContextT>
    void runtime_shutdown(Device& dev) noexcept {
        auto* ctx = runtime_context<ContextT>(dev);
        auto* hook = runtime_hook<ContextT>(dev);
        if (!ctx || !hook || !hook->shutdown) {
            return;
        }
        hook->shutdown(*ctx, dev);
    }

    template <typename ContextT>
    void runtime_remove(Device& dev) noexcept {
        auto* ctx = runtime_context<ContextT>(dev);
        auto* hook = runtime_hook<ContextT>(dev);
        if (!ctx || !hook || !hook->remove) {
            return;
        }
        hook->remove(*ctx, dev);
    }

    template <typename ContextT>
    bool runtime_suspend(Device& dev) noexcept {
        auto* ctx = runtime_context<ContextT>(dev);
        auto* hook = runtime_hook<ContextT>(dev);
        if (!ctx || !hook) {
            return false;
        }
        return hook->suspend ? hook->suspend(*ctx, dev) : true;
    }

    template <typename ContextT>
    bool runtime_resume(Device& dev) noexcept {
        auto* ctx = runtime_context<ContextT>(dev);
        auto* hook = runtime_hook<ContextT>(dev);
        if (!ctx || !hook) {
            return false;
        }
        return hook->resume ? hook->resume(*ctx, dev) : true;
    }

    template <typename ContextT>
    void runtime_on_event(Device& dev, DeviceEvent ev) noexcept {
        auto* ctx = runtime_context<ContextT>(dev);
        auto* hook = runtime_hook<ContextT>(dev);
        if (!ctx || !hook || !hook->on_event) {
            return;
        }
        hook->on_event(*ctx, dev, ev);
    }
}

export namespace device {
    template <typename ContextT>
    inline Driver make_runtime_driver(const DeviceDesc& match,
                                      const char* name,
                                      util::u32 priority = 0) noexcept {
        Driver drv{};
        drv.name = name;
        drv.match = match;
        drv.priority = priority;
        drv.ops = DriverOps{
            &detail::runtime_probe<ContextT>,
            &detail::runtime_init<ContextT>,
            &detail::runtime_shutdown<ContextT>,
            &detail::runtime_remove<ContextT>,
            &detail::runtime_suspend<ContextT>,
            &detail::runtime_resume<ContextT>,
            &detail::runtime_on_event<ContextT>
        };
        return drv;
    }

#ifndef NDEBUG
    inline bool runtime_driver_self_check() noexcept {
        struct Context {
            bool probed{false};
            bool initialized{false};
            bool removed{false};
            bool suspended{false};
            bool resumed{false};
            DeviceEvent last_event{DeviceEvent::attach};
        };

        auto probe = [](Context& ctx, Device&) noexcept -> bool {
            ctx.probed = true;
            return true;
        };
        auto init = [](Context& ctx, Device&) noexcept -> bool {
            ctx.initialized = true;
            return true;
        };
        auto remove = [](Context& ctx, Device&) noexcept {
            ctx.removed = true;
        };
        auto suspend = [](Context& ctx, Device&) noexcept -> bool {
            ctx.suspended = true;
            return true;
        };
        auto resume = [](Context& ctx, Device&) noexcept -> bool {
            ctx.resumed = true;
            return true;
        };
        auto on_event = [](Context& ctx, Device&, DeviceEvent ev) noexcept {
            ctx.last_event = ev;
        };

        Context ctx{};
        RuntimeDriverBinding<Context> binding{
            &ctx,
            RuntimeDriverHook<Context>{
                probe,
                init,
                nullptr,
                remove,
                suspend,
                resume,
                on_event
            }
        };

        Registry<2, 2> registry{};
        DeviceDesc desc{
            .class_id = 0x08,
            .vendor_id = 0x1234,
            .product_id = 0x5678,
            .type = "usb.msc"
        };
        auto driver = make_runtime_driver<Context>(desc, "runtime.check");
        if (!registry.add_driver(driver)) return false;
        if (!registry.add_device(desc, &binding)) return false;

        registry.match_all();
        if (!ctx.probed || !ctx.initialized) return false;
        if (registry.device_at(0).state != DeviceState::running) return false;
        if (ctx.last_event != DeviceEvent::start) return false;

        registry.dispatch(registry.device_at(0), DeviceEvent::suspend);
        registry.dispatch(registry.device_at(0), DeviceEvent::resume);
        if (!ctx.suspended || !ctx.resumed) return false;

        registry.dispatch(registry.device_at(0), DeviceEvent::remove);
        return ctx.removed;
    }
#endif
}
