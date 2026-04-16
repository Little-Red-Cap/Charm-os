module;

#include <array>
#include <span>
#include <string_view>

export module platform.irq;

import init.binding;
import util.core;
import util.error;

export namespace platform {
    struct IrqBinding {
        init::InitFn init_fn{nullptr};
        void* ctx{nullptr};
        std::array<init::CapId, 1> provides{};
        init::Node node{};

        explicit IrqBinding(init::InitFn fn = nullptr,
                            void* init_ctx = nullptr,
                            const char* cap_name = "platform.irq",
                            init::Phase phase = init::Phase::early,
                            util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all)) noexcept
            : init_fn(fn), ctx(init_ctx) {
            provides = init::capability_ids(cap_name);
            node = init::make_binding_node(init::capability_name_view(cap_name),
                                           phase,
                                           runlevel_mask,
                                           provides,
                                           init_fn,
                                           nullptr,
                                           ctx);
        }

        constexpr std::string_view capability_name(init::CapId id) const noexcept {
            return init::lookup_capability_name(id,
                                                provides,
                                                init::capability_names(node.name));
        }
    };
}
