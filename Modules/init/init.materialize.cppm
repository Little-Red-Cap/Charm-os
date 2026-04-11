module;

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <tuple>

export module init.materialize;

import init.graph;
import init.meta;
import init.plan;
import init.recipe;
import init.node;
import util.core;
import util.error;

namespace init::detail {
    template <typename T>
    inline constexpr bool unsupported_legacy_v = false;

    inline util::Result<void> noop_init(void*) noexcept {
        return {};
    }

    template <util::usize MaxCaps>
    struct cap_set {
        std::array<CapId, MaxCaps> ids{};
        util::usize count{0};
    };

    template <util::usize MaxCaps>
    struct materialize_summary {
        cap_set<MaxCaps> provides{};
        Phase max_phase{Phase::early};
        util::u32 common_runlevel_mask{static_cast<util::u32>(Runlevel::all)};
        util::usize leaf_count{0};
        util::usize leaves_with_provides{0};
        bool has_nodes{false};
    };

    template <util::usize MaxCaps>
    struct materialize_constraints {
        cap_set<MaxCaps> required_caps{};
        util::u32 runlevel_mask{static_cast<util::u32>(Runlevel::all)};
        Phase max_phase{Phase::app};
    };

    template <util::usize MaxCaps>
    constexpr bool contains(const cap_set<MaxCaps>& caps, CapId id) noexcept {
        for (util::usize i = 0; i < caps.count; ++i) {
            if (caps.ids[i] == id) {
                return true;
            }
        }
        return false;
    }

    template <util::usize MaxCaps>
    util::Result<void> append_unique(cap_set<MaxCaps>& caps, CapId id) noexcept {
        if (id == 0) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        if (contains(caps, id)) {
            return {};
        }
        if (caps.count >= MaxCaps) {
            return util::unexpected(util::Errc::buffer_overflow);
        }
        caps.ids[caps.count++] = id;
        return {};
    }

    template <util::usize MaxCaps>
    util::Result<void> merge_into(cap_set<MaxCaps>& dst, const cap_set<MaxCaps>& src) noexcept {
        for (util::usize i = 0; i < src.count; ++i) {
            auto r = append_unique(dst, src.ids[i]);
            if (!r) {
                return util::unexpected(r.error());
            }
        }
        return {};
    }

    template <typename List, util::usize MaxCaps>
    util::Result<void> append_type_caps(cap_set<MaxCaps>& dst) noexcept {
        util::Errc error = util::Errc::ok;
        cap_list_traits<List>::for_each([&](CapId id) {
            if (!util::ok(error)) {
                return;
            }
            auto r = append_unique(dst, id);
            if (!r) {
                error = r.error();
            }
        });
        if (!util::ok(error)) {
            return util::unexpected(error);
        }
        return {};
    }

    constexpr Phase phase_max(Phase a, Phase b) noexcept {
        return static_cast<util::u8>(a) >= static_cast<util::u8>(b) ? a : b;
    }

    constexpr Phase phase_min(Phase a, Phase b) noexcept {
        return static_cast<util::u8>(a) <= static_cast<util::u8>(b) ? a : b;
    }
}

export namespace init {
    template <util::usize MaxNodes, util::usize MaxCaps>
    struct materialized_graph {
        std::array<Node, MaxNodes> nodes{};
        std::array<const Node*, MaxNodes> ptrs{};
        std::array<std::array<CapId, MaxCaps>, MaxNodes> provides_storage{};
        std::array<std::array<CapId, MaxCaps>, MaxNodes> requires_storage{};
        std::array<util::usize, MaxNodes> provides_count{};
        std::array<util::usize, MaxNodes> requires_count{};
        std::array<CapId, MaxCaps> provided_caps_seen{};
        util::usize provided_caps_count{0};
        util::usize count{0};
        util::u32 effective_runlevel_mask{0};
        Phase effective_max_phase{Phase::early};

        constexpr std::span<const Node* const> node_ptr_span() const noexcept {
            return std::span<const Node* const>{ptrs.data(), count};
        }

        constexpr util::usize size() const noexcept {
            return count;
        }

        constexpr util::u32 build_runlevel_mask() const noexcept {
            return count == 0 ? static_cast<util::u32>(Runlevel::all)
                              : effective_runlevel_mask;
        }

        constexpr Phase build_max_phase() const noexcept {
            return count == 0 ? Phase::app : effective_max_phase;
        }
    };
}

namespace init::detail {
    template <util::usize MaxNodes, util::usize MaxCaps>
    constexpr bool contains_provided(const materialized_graph<MaxNodes, MaxCaps>& out,
                                     CapId id) noexcept {
        for (util::usize i = 0; i < out.provided_caps_count; ++i) {
            if (out.provided_caps_seen[i] == id) {
                return true;
            }
        }
        return false;
    }

    template <util::usize MaxNodes, util::usize MaxCaps>
    util::Result<void> append_provided(materialized_graph<MaxNodes, MaxCaps>& out,
                                       CapId id) noexcept {
        if (id == 0) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        if (contains_provided(out, id)) {
            return util::unexpected(util::Errc::exist);
        }
        if (out.provided_caps_count >= MaxCaps) {
            return util::unexpected(util::Errc::buffer_overflow);
        }
        out.provided_caps_seen[out.provided_caps_count++] = id;
        return {};
    }

    template <util::usize MaxNodes, util::usize MaxCaps>
    util::Result<void> append_cap_to_row(materialized_graph<MaxNodes, MaxCaps>& out,
                                         util::usize row,
                                         bool provides,
                                         CapId id) noexcept {
        auto& counts = provides ? out.provides_count : out.requires_count;
        auto& rows = provides ? out.provides_storage : out.requires_storage;
        for (util::usize i = 0; i < counts[row]; ++i) {
            if (rows[row][i] == id) {
                return {};
            }
        }
        if (counts[row] >= MaxCaps) {
            return util::unexpected(util::Errc::buffer_overflow);
        }
        rows[row][counts[row]++] = id;
        return {};
    }

    template <util::usize MaxNodes, util::usize MaxCaps>
    util::Result<materialize_summary<MaxCaps>> finalize_node(materialized_graph<MaxNodes, MaxCaps>& out,
                                                             util::usize row) noexcept {
        auto& node = out.nodes[row];
        node.provides = std::span<const CapId>{out.provides_storage[row].data(), out.provides_count[row]};
        node.requires_caps = std::span<const CapId>{out.requires_storage[row].data(), out.requires_count[row]};
        out.ptrs[row] = &node;
        if (row == 0) {
            out.effective_runlevel_mask = node.runlevel_mask;
            out.effective_max_phase = node.phase;
        } else {
            out.effective_runlevel_mask |= node.runlevel_mask;
            out.effective_max_phase = phase_max(out.effective_max_phase, node.phase);
        }

        materialize_summary<MaxCaps> summary{};
        for (util::usize i = 0; i < out.provides_count[row]; ++i) {
            auto r = append_unique(summary.provides, out.provides_storage[row][i]);
            if (!r) {
                return util::unexpected(r.error());
            }
        }
        summary.max_phase = node.phase;
        summary.common_runlevel_mask = node.runlevel_mask;
        summary.leaf_count = 1;
        summary.leaves_with_provides = out.provides_count[row] > 0 ? 1u : 0u;
        summary.has_nodes = true;
        return summary;
    }

    template <util::usize MaxNodes, util::usize MaxCaps>
    util::Result<materialize_summary<MaxCaps>> append_node(materialized_graph<MaxNodes, MaxCaps>& out,
                                                           const Node& base,
                                                           const materialize_constraints<MaxCaps>& constraints) noexcept {
        if (out.count >= MaxNodes) {
            return util::unexpected(util::Errc::buffer_overflow);
        }
        const auto row = out.count++;
        out.nodes[row] = base;
        if (static_cast<util::u8>(out.nodes[row].phase) > static_cast<util::u8>(constraints.max_phase)) {
            return util::unexpected(util::Errc::bad_state);
        }
        out.nodes[row].runlevel_mask &= constraints.runlevel_mask;
        out.provides_count[row] = 0;
        out.requires_count[row] = 0;

        for (util::usize i = 0; i < base.provides.size(); ++i) {
            const auto cap = base.provides[i];
            auto seen = append_provided(out, cap);
            if (!seen) {
                return util::unexpected(seen.error());
            }
            auto r = append_cap_to_row(out, row, true, cap);
            if (!r) {
                return util::unexpected(r.error());
            }
        }

        for (util::usize i = 0; i < base.requires_caps.size(); ++i) {
            auto r = append_cap_to_row(out, row, false, base.requires_caps[i]);
            if (!r) {
                return util::unexpected(r.error());
            }
        }
        for (util::usize i = 0; i < constraints.required_caps.count; ++i) {
            auto r = append_cap_to_row(out, row, false, constraints.required_caps.ids[i]);
            if (!r) {
                return util::unexpected(r.error());
            }
        }

        return finalize_node(out, row);
    }

    template <util::usize MaxNodes, util::usize MaxCaps, typename Recipe>
    util::Result<materialize_summary<MaxCaps>> append_recipe(materialized_graph<MaxNodes, MaxCaps>& out,
                                                             const bound_recipe<Recipe>& item,
                                                             const materialize_constraints<MaxCaps>& constraints) noexcept {
        if (out.count >= MaxNodes) {
            return util::unexpected(util::Errc::buffer_overflow);
        }
        const auto row = out.count++;
        auto& node = out.nodes[row];
        node.name = Recipe::name.sv();
        node.phase = Recipe::phase;
        if (static_cast<util::u8>(node.phase) > static_cast<util::u8>(constraints.max_phase)) {
            return util::unexpected(util::Errc::bad_state);
        }
        node.runlevel_mask = Recipe::runlevel_mask & constraints.runlevel_mask;
        node.init = Recipe::init_fn();
        node.deinit = Recipe::deinit_fn();
        node.ctx = item.ctx;
        out.provides_count[row] = 0;
        out.requires_count[row] = 0;

        util::Errc error = util::Errc::ok;
        cap_list_traits<typename Recipe::provides_list>::for_each([&](CapId cap) {
            if (!util::ok(error)) {
                return;
            }
            auto seen = append_provided(out, cap);
            if (!seen) {
                error = seen.error();
                return;
            }
            auto r = append_cap_to_row(out, row, true, cap);
            if (!r) {
                error = r.error();
            }
        });
        if (!util::ok(error)) {
            return util::unexpected(error);
        }

        cap_list_traits<typename Recipe::requires_list>::for_each([&](CapId cap) {
            if (!util::ok(error)) {
                return;
            }
            auto r = append_cap_to_row(out, row, false, cap);
            if (!r) {
                error = r.error();
            }
        });
        if (!util::ok(error)) {
            return util::unexpected(error);
        }

        for (util::usize i = 0; i < constraints.required_caps.count; ++i) {
            auto r = append_cap_to_row(out, row, false, constraints.required_caps.ids[i]);
            if (!r) {
                return util::unexpected(r.error());
            }
        }

        return finalize_node(out, row);
    }

    template <util::usize MaxCaps>
    util::Result<void> absorb_summary(materialize_summary<MaxCaps>& dst,
                                      const materialize_summary<MaxCaps>& src) noexcept {
        auto r = merge_into(dst.provides, src.provides);
        if (!r) {
            return util::unexpected(r.error());
        }
        dst.leaf_count += src.leaf_count;
        dst.leaves_with_provides += src.leaves_with_provides;
        if (!src.has_nodes) {
            return {};
        }
        if (!dst.has_nodes) {
            dst.max_phase = src.max_phase;
            dst.common_runlevel_mask = src.common_runlevel_mask;
            dst.has_nodes = true;
            return {};
        }
        dst.max_phase = phase_max(dst.max_phase, src.max_phase);
        dst.common_runlevel_mask &= src.common_runlevel_mask;
        dst.has_nodes = true;
        return {};
    }

    template <util::usize MaxNodes, util::usize MaxCaps, typename Item>
    util::Result<materialize_summary<MaxCaps>> materialize_item(materialized_graph<MaxNodes, MaxCaps>& out,
                                                                const Item& item,
                                                                const materialize_constraints<MaxCaps>& constraints) noexcept;

    template <util::usize MaxNodes, util::usize MaxCaps, typename Legacy>
    util::Result<materialize_summary<MaxCaps>> append_legacy_value(materialized_graph<MaxNodes, MaxCaps>& out,
                                                                   const Legacy& value,
                                                                   const materialize_constraints<MaxCaps>& constraints) noexcept {
        materialize_summary<MaxCaps> summary{};
        if constexpr (requires(const Legacy& candidate) {
                          candidate.plan();
                      }) {
            return materialize_item(out, value.plan(), constraints);
        } else if constexpr (requires(const Legacy& candidate) {
                                 candidate.for_each_legacy_node([](const Node&) noexcept {});
                             }) {
            util::Errc error = util::Errc::ok;
            value.for_each_legacy_node([&](const Node& node) noexcept {
                if (!util::ok(error)) {
                    return;
                }
                auto current = append_node(out, node, constraints);
                if (!current) {
                    error = current.error();
                    return;
                }
                auto merge = absorb_summary(summary, *current);
                if (!merge) {
                    error = merge.error();
                }
            });
            if (!util::ok(error)) {
                return util::unexpected(error);
            }
            return summary;
        } else if constexpr (requires(const Legacy& candidate) {
                          candidate.node_span();
                      }) {
            const auto span = value.node_span();
            for (util::usize i = 0; i < span.size(); ++i) {
                if (!span[i]) {
                    continue;
                }
                auto current = append_node(out, *span[i], constraints);
                if (!current) {
                    return util::unexpected(current.error());
                }
                auto merge = absorb_summary(summary, *current);
                if (!merge) {
                    return util::unexpected(merge.error());
                }
            }
            return summary;
        } else if constexpr (requires(const Legacy& candidate) {
                                 candidate.node;
                             }) {
            auto current = append_node(out, value.node, constraints);
            if (!current) {
                return util::unexpected(current.error());
            }
            auto merge = absorb_summary(summary, *current);
            if (!merge) {
                return util::unexpected(merge.error());
            }
            return summary;
        } else {
            static_assert(unsupported_legacy_v<Legacy>,
                          "init::legacy(...) requires plan(), for_each_legacy_node(...), node_span(), or a node member");
        }
    }

    template <std::size_t Index, util::usize MaxNodes, util::usize MaxCaps, typename... Items>
    util::Result<materialize_summary<MaxCaps>> materialize_tuple(materialized_graph<MaxNodes, MaxCaps>& out,
                                                                 const std::tuple<Items...>& items,
                                                                 const materialize_constraints<MaxCaps>& constraints,
                                                                 materialize_summary<MaxCaps> acc = {}) noexcept {
        if constexpr (Index == sizeof...(Items)) {
            return acc;
        } else {
            auto current = materialize_item(out, std::get<Index>(items), constraints);
            if (!current) {
                return util::unexpected(current.error());
            }
            auto merge = absorb_summary(acc, *current);
            if (!merge) {
                return util::unexpected(merge.error());
            }
            return materialize_tuple<Index + 1, MaxNodes, MaxCaps>(out, items, constraints, acc);
        }
    }

    template <util::usize MaxNodes, util::usize MaxCaps, typename Recipe>
    util::Result<materialize_summary<MaxCaps>> materialize_item(materialized_graph<MaxNodes, MaxCaps>& out,
                                                                const bound_recipe<Recipe>& item,
                                                                const materialize_constraints<MaxCaps>& constraints) noexcept {
        return append_recipe(out, item, constraints);
    }

    template <util::usize MaxNodes, util::usize MaxCaps, typename Chain>
    util::Result<materialize_summary<MaxCaps>> materialize_item(materialized_graph<MaxNodes, MaxCaps>& out,
                                                                const legacy_ref<Chain>& item,
                                                                const materialize_constraints<MaxCaps>& constraints) noexcept {
        if (!item.chain) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        return append_legacy_value(out, *item.chain, constraints);
    }

    template <util::usize MaxNodes, util::usize MaxCaps, typename Chain>
    util::Result<materialize_summary<MaxCaps>> materialize_item(materialized_graph<MaxNodes, MaxCaps>& out,
                                                                const legacy_optional_ref<Chain>& item,
                                                                const materialize_constraints<MaxCaps>& constraints) noexcept {
        if (!item.chain || !item.chain->has_value()) {
            return materialize_summary<MaxCaps>{};
        }
        return append_legacy_value(out, item.chain->value(), constraints);
    }

    template <util::usize MaxNodes, util::usize MaxCaps, typename Item>
    util::Result<materialize_summary<MaxCaps>> materialize_item(materialized_graph<MaxNodes, MaxCaps>& out,
                                                                const maybe_ref<Item>& item,
                                                                const materialize_constraints<MaxCaps>& constraints) noexcept {
        if (!item.value || !item.value->has_value()) {
            return materialize_summary<MaxCaps>{};
        }

        if constexpr (requires(const Item& candidate) {
                          candidate.plan();
                      } || requires(const Item& candidate) {
                          candidate.for_each_legacy_node([](const Node&) noexcept {});
                      } || requires(const Item& candidate) {
                          candidate.node_span();
                      } || requires(const Item& candidate) {
                          candidate.node;
                      }) {
            return append_legacy_value(out, item.value->value(), constraints);
        } else {
            return materialize_item(out, item.value->value(), constraints);
        }
    }

    template <util::usize MaxNodes, util::usize MaxCaps>
    util::Result<materialize_summary<MaxCaps>> materialize_item(materialized_graph<MaxNodes, MaxCaps>& out,
                                                                const legacy_nodes_ref& item,
                                                                const materialize_constraints<MaxCaps>& constraints) noexcept {
        materialize_summary<MaxCaps> summary{};
        for (util::usize i = 0; i < item.nodes.size(); ++i) {
            if (!item.nodes[i]) {
                continue;
            }
            auto current = append_node(out, *item.nodes[i], constraints);
            if (!current) {
                return util::unexpected(current.error());
            }
            auto merge = absorb_summary(summary, *current);
            if (!merge) {
                return util::unexpected(merge.error());
            }
        }
        return summary;
    }

    template <util::usize MaxNodes, util::usize MaxCaps, typename... Items>
    util::Result<materialize_summary<MaxCaps>> materialize_item(materialized_graph<MaxNodes, MaxCaps>& out,
                                                                const plan<Items...>& item,
                                                                const materialize_constraints<MaxCaps>& constraints) noexcept {
        return materialize_tuple<0, MaxNodes, MaxCaps>(out, item.items, constraints);
    }

    template <util::usize MaxNodes, util::usize MaxCaps, typename Inner, typename Requires>
    util::Result<materialize_summary<MaxCaps>> materialize_item(materialized_graph<MaxNodes, MaxCaps>& out,
                                                                const after_plan<Inner, Requires>& item,
                                                                const materialize_constraints<MaxCaps>& constraints) noexcept {
        auto merged = constraints;
        auto r = append_type_caps<Requires>(merged.required_caps);
        if (!r) {
            return util::unexpected(r.error());
        }
        return materialize_item(out, item.inner, merged);
    }

    template <util::usize MaxNodes, util::usize MaxCaps, typename Inner>
    util::Result<materialize_summary<MaxCaps>> materialize_item(materialized_graph<MaxNodes, MaxCaps>& out,
                                                                const phase_limit_plan<Inner>& item,
                                                                const materialize_constraints<MaxCaps>& constraints) noexcept {
        auto merged = constraints;
        merged.max_phase = phase_min(merged.max_phase, item.value);
        return materialize_item(out, item.inner, merged);
    }

    template <util::usize MaxNodes, util::usize MaxCaps, typename Inner>
    util::Result<materialize_summary<MaxCaps>> materialize_item(materialized_graph<MaxNodes, MaxCaps>& out,
                                                                const runlevel_plan<Inner>& item,
                                                                const materialize_constraints<MaxCaps>& constraints) noexcept {
        auto merged = constraints;
        merged.runlevel_mask &= item.mask;
        return materialize_item(out, item.inner, merged);
    }

    template <util::usize MaxNodes, util::usize MaxCaps, typename Inner, typename Cap>
    util::Result<materialize_summary<MaxCaps>> materialize_item(materialized_graph<MaxNodes, MaxCaps>& out,
                                                                const export_plan<Inner, Cap>& item,
                                                                const materialize_constraints<MaxCaps>& constraints) noexcept {
        auto summary = materialize_item(out, item.inner, constraints);
        if (!summary) {
            return util::unexpected(summary.error());
        }
        if (!summary->has_nodes || summary->provides.count == 0) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        if (summary->leaf_count == 0 || summary->leaves_with_provides != summary->leaf_count) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        if (summary->common_runlevel_mask == 0) {
            return util::unexpected(util::Errc::bad_state);
        }

        Node barrier{};
        barrier.name = Cap::view();
        barrier.phase = summary->max_phase;
        barrier.runlevel_mask = summary->common_runlevel_mask;
        barrier.init = &noop_init;
        barrier.deinit = nullptr;
        barrier.ctx = nullptr;

        if (out.count >= MaxNodes) {
            return util::unexpected(util::Errc::buffer_overflow);
        }
        const auto row = out.count++;
        out.nodes[row] = barrier;
        out.provides_count[row] = 0;
        out.requires_count[row] = 0;

        auto seen = append_provided(out, Cap::id);
        if (!seen) {
            return util::unexpected(seen.error());
        }
        auto provide_result = append_cap_to_row(out, row, true, Cap::id);
        if (!provide_result) {
            return util::unexpected(provide_result.error());
        }
        for (util::usize i = 0; i < summary->provides.count; ++i) {
            auto require_result = append_cap_to_row(out, row, false, summary->provides.ids[i]);
            if (!require_result) {
                return util::unexpected(require_result.error());
            }
        }
        auto barrier_summary = finalize_node(out, row);
        if (!barrier_summary) {
            return util::unexpected(barrier_summary.error());
        }

        auto merge = absorb_summary(*summary, *barrier_summary);
        if (!merge) {
            return util::unexpected(merge.error());
        }
        return *summary;
    }
}

export namespace init {
    template <util::usize MaxNodes, util::usize MaxCaps, typename Plan>
    util::Result<materialized_graph<MaxNodes, MaxCaps>> materialize(const Plan& plan_value) noexcept {
        materialized_graph<MaxNodes, MaxCaps> out{};
        detail::materialize_constraints<MaxCaps> constraints{};
        auto r = detail::materialize_item(out, plan_value, constraints);
        if (!r) {
            return util::unexpected(r.error());
        }
        return out;
    }

    template <util::usize MaxNodes, util::usize MaxCaps>
    util::Result<void> build_graph(Graph<MaxNodes, MaxCaps>& graph,
                                   const materialized_graph<MaxNodes, MaxCaps>& mats) noexcept {
        return graph.build(mats.node_ptr_span(),
                           mats.build_runlevel_mask(),
                           mats.build_max_phase());
    }

    template <util::usize MaxNodes, util::usize MaxCaps, typename Plan>
    util::Result<void> build_graph(Graph<MaxNodes, MaxCaps>& graph,
                                   const Plan& plan_value) noexcept {
        auto mats = materialize<MaxNodes, MaxCaps>(plan_value);
        if (!mats) {
            return util::unexpected(mats.error());
        }
        return build_graph(graph, *mats);
    }

    template <util::usize MaxNodes, util::usize MaxCaps>
    util::Result<void> start_graph(Graph<MaxNodes, MaxCaps>& graph,
                                   const materialized_graph<MaxNodes, MaxCaps>& mats) noexcept {
        auto r = build_graph(graph, mats);
        if (!r) {
            return util::unexpected(r.error());
        }
        return graph.start();
    }

    template <util::usize MaxNodes, util::usize MaxCaps, typename Plan>
    util::Result<void> start_graph(Graph<MaxNodes, MaxCaps>& graph,
                                   const Plan& plan_value) noexcept {
        auto mats = materialize<MaxNodes, MaxCaps>(plan_value);
        if (!mats) {
            return util::unexpected(mats.error());
        }
        return start_graph(graph, *mats);
    }

#ifndef NDEBUG
    inline util::Result<void> materialize_self_check() noexcept {
        using CapA = cap_c<"test.a">;
        using CapB = cap_c<"test.b">;
        using CapDone = cap_c<"test.done">;

        struct DemoContext {
            util::u32 value{0};
        };

        using RecipeA = recipe_desc<
            "test.a.init",
            Phase::early,
            static_cast<util::u32>(Runlevel::all),
            cap_list<CapA>,
            cap_list<>,
            DemoContext,
            nullptr>;
        using RecipeB = recipe_desc<
            "test.b.init",
            Phase::service,
            static_cast<util::u32>(Runlevel::all),
            cap_list<CapB>,
            cap_list<CapA>,
            DemoContext,
            nullptr>;

        DemoContext ctx{};
        const auto plan_value = compose(
            bind<RecipeA>(ctx),
            bind<RecipeB>(ctx)).ready_as<CapDone>();
        auto mats = materialize<4, 8>(plan_value);
        if (!mats) {
            return util::unexpected(mats.error());
        }
        if (mats->size() != 3) {
            return util::unexpected(util::Errc::bad_state);
        }

        using RecipeNoProvide = recipe_desc<
            "test.no_provide",
            Phase::early,
            static_cast<util::u32>(Runlevel::all),
            cap_list<>,
            cap_list<>,
            DemoContext,
            nullptr>;
        const auto invalid_ready_plan = compose(
            bind<RecipeA>(ctx),
            bind<RecipeNoProvide>(ctx)).ready_as<CapDone>();
        auto invalid_ready = materialize<4, 8>(invalid_ready_plan);
        if (invalid_ready || invalid_ready.error() != util::Errc::invalid_arg) {
            return util::unexpected(util::Errc::bad_state);
        }

        struct LegacyNodeHolder {
            std::array<CapId, 1> provides{CapA::id};

            LegacyNodeHolder() noexcept
                : node{
                    "test.legacy.node",
                    Phase::early,
                    static_cast<util::u32>(Runlevel::all),
                    std::span<const CapId>{provides.data(), provides.size()},
                    {},
                    nullptr,
                    nullptr,
                    this
                } {
            }

            Node node{};
        };

        LegacyNodeHolder holder{};
        auto legacy_holder = materialize<2, 4>(compose(legacy(holder)));
        if (!legacy_holder || legacy_holder->size() != 1) {
            return util::unexpected(util::Errc::bad_state);
        }

        std::optional<LegacyNodeHolder> optional_holder{LegacyNodeHolder{}};
        auto maybe_optional = materialize<2, 4>(compose(maybe(optional_holder)));
        if (!maybe_optional || maybe_optional->size() != 1) {
            return util::unexpected(util::Errc::bad_state);
        }

        auto legacy_optional = materialize<2, 4>(compose(legacy_optional_ref<LegacyNodeHolder>{&optional_holder}));
        if (!legacy_optional || legacy_optional->size() != 1) {
            return util::unexpected(util::Errc::bad_state);
        }
        return {};
    }
#endif
}
