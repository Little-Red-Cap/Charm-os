module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

export module init.graph;

import init.node;
import util.core;
import util.error;

#if defined(__GNUC__) || defined(__clang__)
#define CHARM_INIT_WEAK __attribute__((weak))
#else
#define CHARM_INIT_WEAK
#endif

extern "C" CHARM_INIT_WEAK void charm_init_debug_duplicate_cap(util::u32) noexcept {
}

#undef CHARM_INIT_WEAK

export namespace init {
    template <util::usize MaxNodes, util::usize MaxCaps>
    class Graph {
    public:
        util::Result<void> build(std::span<const Node* const> nodes,
                                 util::u32 runlevel_mask = static_cast<util::u32>(Runlevel::all),
                                 Phase max_phase = Phase::app) noexcept {
            reset();
            if (nodes.size() > MaxNodes) {
                return util::unexpected(util::Errc::buffer_overflow);
            }
            for (util::usize i = 0; i < nodes.size(); ++i) {
                const auto* node = nodes[i];
                if (!node) continue;
                if (!(node->runlevel_mask & runlevel_mask)) continue;
                if (static_cast<util::u8>(node->phase) > static_cast<util::u8>(max_phase)) continue;
                nodes_[node_count_++] = node;
            }
            if (node_count_ == 0) return {};

            for (util::usize i = 0; i < node_count_; ++i) {
                const auto* node = nodes_[i];
                for (util::usize j = 0; j < node->provides.size(); ++j) {
                    const auto cap = node->provides[j];
                    if (cap == 0) return util::unexpected(util::Errc::invalid_arg);
                    if (find_cap_index(cap) >= 0) {
                        charm_init_debug_duplicate_cap(cap);
                        return util::unexpected(util::Errc::exist);
                    }
                    if (cap_count_ >= MaxCaps) {
                        return util::unexpected(util::Errc::buffer_overflow);
                    }
                    caps_[cap_count_++] = CapEntry{cap, static_cast<util::u16>(i)};
                }
            }

            for (util::usize i = 0; i < node_count_; ++i) {
                in_degree_[i] = 0;
                const auto* node = nodes_[i];
                for (util::usize j = 0; j < node->requires_caps.size(); ++j) {
                    const auto req = node->requires_caps[j];
                    if (req == 0) return util::unexpected(util::Errc::invalid_arg);
                    const auto provider = find_cap_index(req);
                    if (provider < 0) {
                        return util::unexpected(util::Errc::noent);
                    }
                    if (static_cast<util::usize>(provider) == i) {
                        return util::unexpected(util::Errc::bad_state);
                    }
                    const auto* prov_node = nodes_[static_cast<util::usize>(provider)];
                    if (static_cast<util::u8>(prov_node->phase) > static_cast<util::u8>(node->phase)) {
                        return util::unexpected(util::Errc::bad_state);
                    }
                    ++in_degree_[i];
                }
            }

            util::usize q_head = 0;
            util::usize q_tail = 0;
            for (util::usize i = 0; i < node_count_; ++i) {
                if (in_degree_[i] == 0) queue_[q_tail++] = static_cast<util::u16>(i);
            }

            while (q_head < q_tail) {
                const auto idx = queue_[q_head++];
                order_[order_count_++] = idx;

                for (util::usize j = 0; j < node_count_; ++j) {
                    if (in_degree_[j] == 0) continue;
                    if (!depends_on(j, idx)) continue;
                    if (--in_degree_[j] == 0) {
                        queue_[q_tail++] = static_cast<util::u16>(j);
                    }
                }
            }

            if (order_count_ != node_count_) {
                return util::unexpected(util::Errc::bad_state);
            }
            return {};
        }

        util::Result<void> start() noexcept {
            for (util::usize i = 0; i < order_count_; ++i) {
                const auto* node = nodes_[order_[i]];
                if (!node || !node->init) continue;
                auto r = node->init(node->ctx);
                if (!r) return util::unexpected(r.error());
            }
            return {};
        }

        util::usize size() const noexcept { return node_count_; }
        util::usize ordered() const noexcept { return order_count_; }

    private:
        struct CapEntry {
            CapId cap{0};
            util::u16 provider{0};
        };

        void reset() noexcept {
            node_count_ = 0;
            cap_count_ = 0;
            order_count_ = 0;
            for (auto& v : in_degree_) v = 0;
        }

        int find_cap_index(CapId cap) const noexcept {
            for (util::usize i = 0; i < cap_count_; ++i) {
                if (caps_[i].cap == cap) return static_cast<int>(caps_[i].provider);
            }
            return -1;
        }

        bool depends_on(util::usize node_idx, util::usize provider_idx) const noexcept {
            const auto* node = nodes_[node_idx];
            const auto* prov = nodes_[provider_idx];
            if (!node || !prov) return false;
            for (util::usize r = 0; r < node->requires_caps.size(); ++r) {
                const auto req = node->requires_caps[r];
                for (util::usize p = 0; p < prov->provides.size(); ++p) {
                    const auto cap = prov->provides[p];
                    if (req == cap) return true;
                }
            }
            return false;
        }

        std::array<const Node*, MaxNodes> nodes_{};
        std::array<CapEntry, MaxCaps> caps_{};
        std::array<util::u16, MaxNodes> queue_{};
        std::array<util::u16, MaxNodes> order_{};
        std::array<util::u16, MaxNodes> in_degree_{};
        util::usize node_count_{0};
        util::usize cap_count_{0};
        util::usize order_count_{0};
    };

#ifndef NDEBUG
    inline util::Result<void> graph_self_check() noexcept {
        static constexpr CapId cap_a = cap_id("cap.a");
        static constexpr CapId cap_b = cap_id("cap.b");
        static constexpr CapId cap_c = cap_id("cap.c");

        static constexpr CapId provides_a[] = {cap_a};
        static constexpr CapId provides_b[] = {cap_b};
        static constexpr CapId provides_c[] = {cap_c};
        static constexpr CapId requires_b[] = {cap_a};
        static constexpr CapId requires_c[] = {cap_b};

        Node a{"A", Phase::core, static_cast<util::u32>(Runlevel::all),
               std::span<const CapId>(provides_a, 1), {}, nullptr, nullptr, nullptr};
        Node b{"B", Phase::core, static_cast<util::u32>(Runlevel::all),
               std::span<const CapId>(provides_b, 1), std::span<const CapId>(requires_b, 1), nullptr, nullptr, nullptr};
        Node c{"C", Phase::service, static_cast<util::u32>(Runlevel::all),
               std::span<const CapId>(provides_c, 1), std::span<const CapId>(requires_c, 1), nullptr, nullptr, nullptr};

        const Node* nodes[] = {&a, &b, &c};
        Graph<4, 8> g{};
        auto r = g.build(std::span<const Node* const>(nodes, 3));
        if (!r) return util::unexpected(r.error());
        if (g.ordered() != 3) return util::unexpected(util::Errc::bad_state);
        return {};
    }
#endif
}
