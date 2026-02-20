module;
#include <cstddef>

export module charm.system.init;

import kernel.init_list;
import kernel.startup;

export namespace charm::system {
    using kernel::InitHook;
    using kernel::InitList;

    inline void service_init() noexcept { kernel::service_init(); }
    inline void hal_init() noexcept { kernel::hal_init(); }
    inline void component_init() noexcept { kernel::component_init(); }

    inline void init_sequence(
        InitHook service = service_init,
        InitHook hal = hal_init,
        InitHook component = component_init) noexcept {
        if (service) {
            service();
        }
        if (hal) {
            hal();
        }
        if (component) {
            component();
        }
    }

    template <std::size_t S, std::size_t H, std::size_t C>
    inline void init_sequence(
        const InitList<S>& services,
        const InitList<H>& hals,
        const InitList<C>& components) noexcept {
        services.run();
        hals.run();
        components.run();
    }
}
