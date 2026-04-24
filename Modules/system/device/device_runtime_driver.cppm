module;

export module device.runtime_driver;

import device.desc;
import device.registry;
import device.types;
import util.core;
import util.error;

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
        util::Result<void> (*try_probe)(ContextT& ctx, Device& dev) noexcept { nullptr };
        util::Result<void> (*try_init)(ContextT& ctx, Device& dev) noexcept { nullptr };
        util::Result<void> (*try_suspend)(ContextT& ctx, Device& dev) noexcept { nullptr };
        util::Result<void> (*try_resume)(ContextT& ctx, Device& dev) noexcept { nullptr };
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
    util::Result<void> runtime_try_probe(Device& dev) noexcept {
        auto* ctx = runtime_context<ContextT>(dev);
        auto* hook = runtime_hook<ContextT>(dev);
        if (!ctx || !hook) {
            return util::unexpected(util::Errc::bad_state);
        }
        if (hook->try_probe) {
            return hook->try_probe(*ctx, dev);
        }
        if (!hook->probe || hook->probe(*ctx, dev)) {
            return {};
        }
        return util::unexpected(util::Errc::bad_state);
    }

    template <typename ContextT>
    bool runtime_probe(Device& dev) noexcept {
        return static_cast<bool>(runtime_try_probe<ContextT>(dev));
    }

    template <typename ContextT>
    util::Result<void> runtime_try_init(Device& dev) noexcept {
        auto* ctx = runtime_context<ContextT>(dev);
        auto* hook = runtime_hook<ContextT>(dev);
        if (!ctx || !hook) {
            return util::unexpected(util::Errc::bad_state);
        }
        if (hook->try_init) {
            return hook->try_init(*ctx, dev);
        }
        if (!hook->init || hook->init(*ctx, dev)) {
            return {};
        }
        return util::unexpected(util::Errc::bad_state);
    }

    template <typename ContextT>
    bool runtime_init(Device& dev) noexcept {
        return static_cast<bool>(runtime_try_init<ContextT>(dev));
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
    util::Result<void> runtime_try_suspend(Device& dev) noexcept {
        auto* ctx = runtime_context<ContextT>(dev);
        auto* hook = runtime_hook<ContextT>(dev);
        if (!ctx || !hook) {
            return util::unexpected(util::Errc::bad_state);
        }
        if (hook->try_suspend) {
            return hook->try_suspend(*ctx, dev);
        }
        if (!hook->suspend || hook->suspend(*ctx, dev)) {
            return {};
        }
        return util::unexpected(util::Errc::bad_state);
    }

    template <typename ContextT>
    bool runtime_suspend(Device& dev) noexcept {
        return static_cast<bool>(runtime_try_suspend<ContextT>(dev));
    }

    template <typename ContextT>
    util::Result<void> runtime_try_resume(Device& dev) noexcept {
        auto* ctx = runtime_context<ContextT>(dev);
        auto* hook = runtime_hook<ContextT>(dev);
        if (!ctx || !hook) {
            return util::unexpected(util::Errc::bad_state);
        }
        if (hook->try_resume) {
            return hook->try_resume(*ctx, dev);
        }
        if (!hook->resume || hook->resume(*ctx, dev)) {
            return {};
        }
        return util::unexpected(util::Errc::bad_state);
    }

    template <typename ContextT>
    bool runtime_resume(Device& dev) noexcept {
        return static_cast<bool>(runtime_try_resume<ContextT>(dev));
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
            .probe = &detail::runtime_probe<ContextT>,
            .init = &detail::runtime_init<ContextT>,
            .shutdown = &detail::runtime_shutdown<ContextT>,
            .remove = &detail::runtime_remove<ContextT>,
            .suspend = &detail::runtime_suspend<ContextT>,
            .resume = &detail::runtime_resume<ContextT>,
            .on_event = &detail::runtime_on_event<ContextT>,
            .try_probe = &detail::runtime_try_probe<ContextT>,
            .try_init = &detail::runtime_try_init<ContextT>,
            .try_suspend = &detail::runtime_try_suspend<ContextT>,
            .try_resume = &detail::runtime_try_resume<ContextT>
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

        auto matched = registry.try_match_all();
        if (!matched) return false;
        if (!ctx.probed || !ctx.initialized) return false;
        if (registry.device_at(0).state != DeviceState::running) return false;
        if (ctx.last_event != DeviceEvent::start) return false;

        auto suspended = registry.try_dispatch(registry.device_at(0), DeviceEvent::suspend);
        if (!suspended) return false;
        auto resumed = registry.try_dispatch(registry.device_at(0), DeviceEvent::resume);
        if (!resumed) return false;
        if (!ctx.suspended || !ctx.resumed) return false;

        auto removed = registry.try_dispatch(registry.device_at(0), DeviceEvent::remove);
        if (!removed || !ctx.removed) return false;

        Context failing_ctx{};
        RuntimeDriverBinding<Context> failing_binding{
            &failing_ctx,
            RuntimeDriverHook<Context>{
                .probe = probe,
                .remove = remove,
                .suspend = suspend,
                .resume = resume,
                .on_event = on_event,
                .try_init = [](Context&, Device&) noexcept -> util::Result<void> {
                    return util::unexpected(util::Errc::invalid_arg);
                }
            }
        };
        Registry<2, 2> failing_registry{};
        auto failing_driver = make_runtime_driver<Context>(desc, "runtime.check.fail");
        if (!failing_registry.try_add_driver(failing_driver)) return false;
        if (!failing_registry.try_add_device(desc, &failing_binding)) return false;
        auto failed_match = failing_registry.try_match_all();
        if (failed_match || failed_match.error() != util::Errc::invalid_arg) return false;
        if (!failing_ctx.probed) return false;
        return failing_registry.device_at(0).state == DeviceState::detected &&
               failing_registry.device_at(0).driver == nullptr;
    }
#endif
}
