module;

#include <type_traits>

export module init.recipe;

import init.meta;
import init.node;
import util.core;
import util.error;

export namespace init {
    template <fixed_string Name,
              Phase PhaseV,
              util::u32 RunlevelMask,
              typename Provides,
              typename Requires,
              typename Runtime,
              auto StartFn,
              auto StopFn = nullptr>
    struct recipe_desc {
        using provides_list = Provides;
        using requires_list = Requires;
        using runtime_type = Runtime;

        static constexpr auto name = Name;
        static constexpr Phase phase = PhaseV;
        static constexpr util::u32 runlevel_mask = RunlevelMask;

    private:
        template <auto Fn = StartFn>
        static util::Result<void> init_bridge(void* ctx) noexcept {
            if (!ctx) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            return Fn(*static_cast<Runtime*>(ctx));
        }

        template <auto Fn = StopFn>
        static void deinit_bridge(void* ctx) noexcept {
            if (!ctx) {
                return;
            }
            Fn(*static_cast<Runtime*>(ctx));
        }

    public:
        static constexpr InitFn init_fn() noexcept {
            if constexpr (std::is_same_v<std::remove_cvref_t<decltype(StartFn)>, std::nullptr_t>) {
                return nullptr;
            } else {
                return &init_bridge<StartFn>;
            }
        }

        static constexpr DeinitFn deinit_fn() noexcept {
            if constexpr (std::is_same_v<std::remove_cvref_t<decltype(StopFn)>, std::nullptr_t>) {
                return nullptr;
            } else {
                return &deinit_bridge<StopFn>;
            }
        }
    };
}
