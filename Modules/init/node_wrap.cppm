module;

#include <array>
#include <span>

export module init.node_wrap;

import init.node;
import util.core;
import util.error;

export namespace init {
    template <util::usize Max>
    struct WrappedNodes {
        std::array<init::Node, Max> nodes{};
        std::array<const init::Node*, Max> ptrs{};
    };

    template <typename Chain, util::usize Max>
    [[deprecated("use init::after(...) or plan inheritance instead of node patching")]]
    util::Result<WrappedNodes<Max>> wrap_nodes_with_requires(
        const Chain& chain,
        std::span<const init::CapId> requires_caps) {
        WrappedNodes<Max> out{};
        const auto span = chain.node_span();
        if (span.size() > Max) {
            return util::unexpected(util::Errc::buffer_overflow);
        }
        for (util::usize i = 0; i < span.size(); ++i) {
            out.nodes[i] = *span[i];
            out.nodes[i].requires_caps = requires_caps;
            out.ptrs[i] = &out.nodes[i];
        }
        return out;
    }
}
