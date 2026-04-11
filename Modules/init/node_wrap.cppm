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
        util::usize count = 0;
        bool overflow = false;
        if constexpr (requires(const Chain& candidate) {
                          candidate.for_each_legacy_node([](const init::Node&) noexcept {});
                      }) {
            chain.for_each_legacy_node([&](const init::Node& node) noexcept {
                if (count >= Max) {
                    overflow = true;
                    return;
                }
                out.nodes[count] = node;
                out.nodes[count].requires_caps = requires_caps;
                out.ptrs[count] = &out.nodes[count];
                ++count;
            });
            if (overflow) {
                return util::unexpected(util::Errc::buffer_overflow);
            }
        } else {
            const auto span = chain.node_span();
            if (span.size() > Max) {
                return util::unexpected(util::Errc::buffer_overflow);
            }
            for (util::usize i = 0; i < span.size(); ++i) {
                out.nodes[i] = *span[i];
                out.nodes[i].requires_caps = requires_caps;
                out.ptrs[i] = &out.nodes[i];
            }
        }
        return out;
    }
}
