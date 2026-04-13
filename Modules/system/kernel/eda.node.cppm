module;

#include <array>
#include <span>
#include <string_view>

export module kernel.eda.node;

import init.node;
import util.core;

export namespace kernel {
    struct EdaBinding {
        std::array<init::CapId, 1> provides{};
        init::Node node{};

        explicit EdaBinding(const char* cap_name = "kernel.eda",
                            init::Phase phase = init::Phase::core,
                            util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all)) noexcept {
            provides[0] = init::cap_id(cap_name);
            node = init::Node{
                cap_name,
                phase,
                runlevel_mask,
                std::span<const init::CapId>(provides.data(), provides.size()),
                {},
                nullptr,
                nullptr,
                nullptr
            };
        }

        constexpr std::string_view capability_name(init::CapId id) const noexcept {
            return id == provides[0]
                ? node.name
                : std::string_view{};
        }
    };
}
