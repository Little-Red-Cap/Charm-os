module;

#include <array>
#include <span>
#include <string_view>

export module input.router.node;

import init.binding;
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
            provides = init::capability_ids(cap_name);
            node = init::make_binding_node(init::capability_name_view(cap_name),
                                           phase,
                                           runlevel_mask,
                                           provides,
                                           &RouterBinding::init_trampoline,
                                           nullptr,
                                           this);
        }

        constexpr std::string_view capability_name(init::CapId id) const noexcept {
            return init::lookup_capability_name(id,
                                                provides,
                                                init::capability_names(node.name));
        }

        static util::Result<void> init_trampoline(void* ctx) noexcept {
            auto* self = static_cast<RouterBinding*>(ctx);
            if (!self || !self->router_ref) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            return {};
        }
    };
}
