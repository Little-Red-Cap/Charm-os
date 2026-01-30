module;

#include <utility>

export module kernel.startup;

import kernel.init_list;
import kernel.scheduler;

export namespace kernel {
    inline void service_init() noexcept { }
    inline void hal_init() noexcept { }
    inline void component_init() noexcept { }

    template <typename Config, typename Registry, typename Caps>
    auto boot(Registry& registry, Caps& caps,
        InitHook service = service_init,
        InitHook hal = hal_init,
        InitHook component = component_init) {
        if (service) {
            service();
        }
        if (hal) {
            hal();
        }
        auto created = make_scheduler<Config>(registry, caps);
        auto running = start(std::move(created));
        if (component) {
            component();
        }
        return running;
    }

    template <typename Config, typename Registry, typename Caps, std::size_t S, std::size_t H, std::size_t C>
    auto boot(Registry& registry, Caps& caps,
        const InitList<S>& services,
        const InitList<H>& hals,
        const InitList<C>& components) {
        services.run();
        hals.run();
        auto created = make_scheduler<Config>(registry, caps);
        auto running = start(std::move(created));
        components.run();
        return running;
    }
}
