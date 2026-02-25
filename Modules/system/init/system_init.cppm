module;
#include <array>
#include <cstddef>
#include <utility>

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

    struct InitItem {
        int priority{0};
        InitHook hook{};
    };

    template <std::size_t Max>
    class InitTable {
    public:
        [[nodiscard]] bool add(int priority, InitHook hook) noexcept {
            if (hook == nullptr || count_ >= Max) {
                return false;
            }
            items_[count_++] = InitItem{priority, hook};
            return true;
        }

        void run() const noexcept {
            for (std::size_t i = 0; i < count_; ++i) {
                const auto best = pick_next(i);
                if (best != i) {
                    std::swap(items_[i], items_[best]);
                }
                items_[i].hook();
            }
        }

        [[nodiscard]] std::size_t size() const noexcept {
            return count_;
        }

    private:
        std::size_t pick_next(std::size_t start) const noexcept {
            std::size_t best = start;
            for (std::size_t i = start + 1; i < count_; ++i) {
                if (items_[i].priority < items_[best].priority) {
                    best = i;
                }
            }
            return best;
        }

        mutable std::array<InitItem, Max> items_{};
        std::size_t count_{0};
    };

    template <std::size_t S, std::size_t H, std::size_t C>
    inline void init_sequence(
        const InitList<S>& services,
        const InitList<H>& hals,
        const InitTable<C>& components) noexcept {
        services.run();
        hals.run();
        components.run();
    }
}
