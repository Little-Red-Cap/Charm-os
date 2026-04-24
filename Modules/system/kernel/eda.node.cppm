module;

#include <array>
#include <span>
#include <string_view>

export module kernel.eda.node;

import init.binding;
import util.core;

export namespace kernel {
    struct EdaBinding {
        std::array<init::CapId, 1> provides{};
        init::Node node{};

        explicit EdaBinding(const char* cap_name = "kernel.eda",
                            init::Phase phase = init::Phase::core,
                            util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all)) noexcept {
            provides = init::capability_ids(cap_name);
            node = init::make_binding_node(init::capability_name_view(cap_name),
                                           phase,
                                           runlevel_mask,
                                           provides);
        }

        constexpr std::string_view capability_name(init::CapId id) const noexcept {
            return init::lookup_capability_name(id,
                                                provides,
                                                init::capability_names(node.name));
        }
    };
}
