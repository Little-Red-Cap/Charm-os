module;

#include <array>
#include <span>

export module input.router.node;

import init.node;
import input.router;
import util.core;
import util.error;

export namespace input {
    struct RouterBinding {
        Router* router_ref{nullptr};
        std::array<init::CapId, 1> provides{};
        init::Node node{};

        explicit RouterBinding(Router& router,
                               const char* cap_name = "input.router",
                               init::Phase phase = init::Phase::core,
                               util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all)) noexcept
            : router_ref(&router) {
            provides[0] = init::cap_id(cap_name);
            node = init::Node{
                cap_name,
                phase,
                runlevel_mask,
                std::span<const init::CapId>(provides.data(), provides.size()),
                {},
                &RouterBinding::init_trampoline,
                nullptr,
                this
            };
        }

        static util::Result<void> init_trampoline(void* ctx) noexcept {
            auto* self = static_cast<RouterBinding*>(ctx);
            if (!self || !self->router_ref) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if (g_router && g_router != self->router_ref) {
                return util::unexpected(util::Errc::exist);
            }
            set_router(*self->router_ref);
            return {};
        }
    };
}
