module;

#include <array>
#include <span>

export module init.node_wrap;

import init.node;
import util.core;
import util.error;

export namespace init {
    template <typename T>
    inline constexpr bool unsupported_wrap_nodes_v = false;

    template <typename T>
    concept has_legacy_for_each = requires(const T& candidate) {
        candidate.for_each_legacy_node([](const init::Node&) noexcept {});
    };

    template <typename T>
    concept has_legacy_node_span = requires(const T& candidate) {
        candidate.node_span();
    };

    template <util::usize Max>
    struct WrappedNodes {
        std::array<init::Node, Max> nodes{};
        std::array<const init::Node*, Max> ptrs{};
    };

    template <util::usize Max>
    util::Result<WrappedNodes<Max>> wrap_node_span_with_requires(
        std::span<const init::Node* const> span,
        std::span<const init::CapId> requires_caps) {
        WrappedNodes<Max> out{};
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

    template <util::usize Max>
    [[deprecated("use init::after(...) or plan inheritance instead of node patching")]]
    util::Result<WrappedNodes<Max>> wrap_nodes_with_requires(
        std::span<const init::Node* const> nodes,
        std::span<const init::CapId> requires_caps) {
        return wrap_node_span_with_requires<Max>(nodes, requires_caps);
    }

    template <typename Chain, util::usize Max>
    [[deprecated("use init::after(...) or plan inheritance instead of node patching")]]
    util::Result<WrappedNodes<Max>> wrap_nodes_with_requires(
        const Chain& chain,
        std::span<const init::CapId> requires_caps)
        requires has_legacy_for_each<Chain> {
        WrappedNodes<Max> out{};
        util::usize count = 0;
        bool overflow = false;
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
        return out;
    }

    template <typename Chain, util::usize Max>
    [[deprecated("prefer for_each_legacy_node(...) or an explicit raw node span instead of node_span() patching")]]
    util::Result<WrappedNodes<Max>> wrap_nodes_with_requires(
        const Chain& chain,
        std::span<const init::CapId> requires_caps)
        requires (!has_legacy_for_each<Chain> && has_legacy_node_span<Chain>) {
        return wrap_node_span_with_requires<Max>(chain.node_span(), requires_caps);
    }

    template <typename Chain, util::usize Max>
    util::Result<WrappedNodes<Max>> wrap_nodes_with_requires(
        const Chain&,
        std::span<const init::CapId>)
        requires (!has_legacy_for_each<Chain> && !has_legacy_node_span<Chain>) {
        static_assert(unsupported_wrap_nodes_v<Chain>,
                      "init::wrap_nodes_with_requires(...) expects for_each_legacy_node(...) or a raw node span");
    }
}
