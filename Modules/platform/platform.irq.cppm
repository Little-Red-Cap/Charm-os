module;

#include <array>
#include <span>

export module platform.irq;

import init.node;
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
            provides[0] = init::cap_id(cap_name);
            node = init::Node{
                cap_name,
                phase,
                runlevel_mask,
                std::span<const init::CapId>(provides.data(), provides.size()),
                {},
                init_fn,
                nullptr,
                ctx
            };
        }
    };
}
