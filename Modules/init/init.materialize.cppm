module;

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <tuple>

export module init.materialize;

import init.binding;
import init.connection;
import init.graph;
import init.meta;
import init.plan;
import init.recipe;
import util.core;
import util.error;

namespace init::detail {
    template <typename T>
    inline constexpr bool unsupported_materialize_item_v = false;

    inline util::Result<void> noop_init(void*) noexcept {
        return {};
    }

    template <typename T>
    inline constexpr bool is_bound_recipe_v = false;

    template <typename Recipe>
    inline constexpr bool is_bound_recipe_v<bound_recipe<Recipe>> = true;

    template <typename T>
    inline constexpr bool is_single_node_ref_v = false;

    template <typename Item>
    inline constexpr bool is_single_node_ref_v<single_node_ref<Item>> = true;

    template <typename T>
    inline constexpr bool is_maybe_ref_v = false;

    template <typename Item>
    inline constexpr bool is_maybe_ref_v<maybe_ref<Item>> = true;

    template <typename T>
    inline constexpr bool is_plan_v = false;

    template <typename... Items>
    inline constexpr bool is_plan_v<plan<Items...>> = true;

    template <typename T>
    inline constexpr bool is_after_plan_v = false;

    template <typename Inner, typename Requires>
    inline constexpr bool is_after_plan_v<after_plan<Inner, Requires>> = true;

    template <typename T>
    inline constexpr bool is_phase_limit_plan_v = false;

    template <typename Inner>
    inline constexpr bool is_phase_limit_plan_v<phase_limit_plan<Inner>> = true;

    template <typename T>
    inline constexpr bool is_runlevel_plan_v = false;

    template <typename Inner>
    inline constexpr bool is_runlevel_plan_v<runlevel_plan<Inner>> = true;

    template <typename T>
    inline constexpr bool is_export_plan_v = false;

    template <typename Inner, typename Cap>
    inline constexpr bool is_export_plan_v<export_plan<Inner, Cap>> = true;

    template <typename T>
    struct after_plan_traits;

    template <typename Inner, typename Requires>
    struct after_plan_traits<after_plan<Inner, Requires>> {
        using requires_type = Requires;
    };

    template <typename T>
    struct export_plan_traits;

    template <typename Inner, typename Cap>
    struct export_plan_traits<export_plan<Inner, Cap>> {
        using cap_type = Cap;
    };

    template <util::usize MaxCaps>
    struct cap_set {
        std::array<CapId, MaxCaps> ids{};
        std::array<std::string_view, MaxCaps> names{};
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
    constexpr util::usize find_index(const cap_set<MaxCaps>& caps, CapId id) noexcept {
        for (util::usize i = 0; i < caps.count; ++i) {
            if (caps.ids[i] == id) {
                return i;
            }
        }
        return caps.count;
    }

    template <util::usize MaxCaps>
    constexpr bool contains(const cap_set<MaxCaps>& caps, CapId id) noexcept {
        return find_index(caps, id) != caps.count;
    }

    template <util::usize MaxCaps>
    util::Result<void> append_unique(cap_set<MaxCaps>& caps,
                                     CapId id,
                                     std::string_view name = {}) noexcept {
        if (id == 0) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        const auto index = find_index(caps, id);
        if (index != caps.count) {
            if (caps.names[index].empty() && !name.empty()) {
                caps.names[index] = name;
            }
            return {};
        }
        if (caps.count >= MaxCaps) {
            return util::unexpected(util::Errc::buffer_overflow);
        }
        caps.ids[caps.count] = id;
        caps.names[caps.count] = name;
        ++caps.count;
        return {};
    }

    template <util::usize MaxCaps>
    util::Result<void> merge_into(cap_set<MaxCaps>& dst, const cap_set<MaxCaps>& src) noexcept {
        for (util::usize i = 0; i < src.count; ++i) {
            auto r = append_unique(dst, src.ids[i], src.names[i]);
            if (!r) {
                return util::unexpected(r.error());
            }
        }
        return {};
    }

    template <typename List, util::usize MaxCaps>
    util::Result<void> append_type_caps(cap_set<MaxCaps>& dst) noexcept {
        util::Errc error = util::Errc::ok;
        cap_list_traits<List>::for_each_named([&](CapId id, std::string_view name) {
            if (!util::ok(error)) {
                return;
            }
            auto r = append_unique(dst, id, name);
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
    enum class materialized_node_kind : util::u8 {
        unknown,
        recipe,
        barrier,
        binding,
        connection,
    };

    template <util::usize MaxNodes, util::usize MaxCaps>
    struct materialized_graph {
        std::array<Node, MaxNodes> nodes{};
        std::array<const Node*, MaxNodes> ptrs{};
        std::array<materialized_node_kind, MaxNodes> node_kinds{};
        std::array<std::array<CapId, MaxCaps>, MaxNodes> provides_storage{};
        std::array<std::array<CapId, MaxCaps>, MaxNodes> requires_storage{};
        std::array<std::array<std::string_view, MaxCaps>, MaxNodes> provides_name_storage{};
        std::array<std::array<std::string_view, MaxCaps>, MaxNodes> requires_name_storage{};
        std::array<util::usize, MaxNodes> provides_count{};
        std::array<util::usize, MaxNodes> requires_count{};
        std::array<std::string_view, MaxNodes> connection_source_storage{};
        std::array<std::string_view, MaxNodes> connection_sink_storage{};
        std::array<std::string_view, MaxNodes> connection_mode_storage{};
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
    struct node_observe_metadata {
        materialized_node_kind kind{materialized_node_kind::binding};
        std::string_view connection_source{};
        std::string_view connection_sink{};
        std::string_view connection_mode{};
    };

    template <typename T>
    concept connection_binding_like = requires(const T& candidate) {
        candidate.source_capability();
        candidate.sink_capability();
        candidate.connection_mode();
    };

    template <typename Item>
    [[nodiscard]] constexpr node_observe_metadata describe_single_node(const Item& value) noexcept {
        if constexpr (connection_binding_like<Item>) {
            return node_observe_metadata{
                .kind = materialized_node_kind::connection,
                .connection_source = std::string_view{value.source_capability()},
                .connection_sink = std::string_view{value.sink_capability()},
                .connection_mode = std::string_view{value.connection_mode()},
            };
        } else {
            return {};
        }
    }

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
                                         CapId id,
                                         std::string_view name = {}) noexcept {
        auto& counts = provides ? out.provides_count : out.requires_count;
        auto& rows = provides ? out.provides_storage : out.requires_storage;
        auto& name_rows = provides ? out.provides_name_storage : out.requires_name_storage;
        for (util::usize i = 0; i < counts[row]; ++i) {
            if (rows[row][i] == id) {
                if (name_rows[row][i].empty() && !name.empty()) {
                    name_rows[row][i] = name;
                }
                return {};
            }
        }
        if (counts[row] >= MaxCaps) {
            return util::unexpected(util::Errc::buffer_overflow);
        }
        rows[row][counts[row]] = id;
        name_rows[row][counts[row]] = name;
        ++counts[row];
        return {};
    }

    template <util::usize MaxNodes, util::usize MaxCaps, typename NameLookup>
    util::Result<materialize_summary<MaxCaps>> append_node_with_lookup(materialized_graph<MaxNodes, MaxCaps>& out,
                                                                       const Node& base,
                                                                       const materialize_constraints<MaxCaps>& constraints,
                                                                       NameLookup&& lookup_name,
                                                                       node_observe_metadata metadata = {}) noexcept;

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
            auto r = append_unique(summary.provides,
                                   out.provides_storage[row][i],
                                   out.provides_name_storage[row][i]);
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
        const auto no_name_lookup = [](CapId) noexcept -> std::string_view { return {}; };
        return append_node_with_lookup(out, base, constraints, no_name_lookup);
    }

    template <util::usize MaxNodes, util::usize MaxCaps, typename NameLookup>
    util::Result<materialize_summary<MaxCaps>> append_node_with_lookup(materialized_graph<MaxNodes, MaxCaps>& out,
                                                                       const Node& base,
                                                                       const materialize_constraints<MaxCaps>& constraints,
                                                                       NameLookup&& lookup_name,
                                                                       node_observe_metadata metadata) noexcept {
        if (out.count >= MaxNodes) {
            return util::unexpected(util::Errc::buffer_overflow);
        }
        const auto row = out.count++;
        out.nodes[row] = base;
        out.node_kinds[row] = metadata.kind;
        out.connection_source_storage[row] = metadata.connection_source;
        out.connection_sink_storage[row] = metadata.connection_sink;
        out.connection_mode_storage[row] = metadata.connection_mode;
        if (static_cast<util::u8>(out.nodes[row].phase) > static_cast<util::u8>(constraints.max_phase)) {
            return util::unexpected(util::Errc::bad_state);
        }
        out.nodes[row].runlevel_mask &= constraints.runlevel_mask;
        out.provides_count[row] = 0;
        out.requires_count[row] = 0;

        for (util::usize i = 0; i < base.provides.size(); ++i) {
            const auto cap = base.provides[i];
            const auto name = lookup_name(cap);
            auto seen = append_provided(out, cap);
            if (!seen) {
                return util::unexpected(seen.error());
            }
            auto r = append_cap_to_row(out, row, true, cap, name);
            if (!r) {
                return util::unexpected(r.error());
            }
        }

        for (util::usize i = 0; i < base.requires_caps.size(); ++i) {
            auto r = append_cap_to_row(out,
                                       row,
                                       false,
                                       base.requires_caps[i],
                                       lookup_name(base.requires_caps[i]));
            if (!r) {
                return util::unexpected(r.error());
            }
        }
        for (util::usize i = 0; i < constraints.required_caps.count; ++i) {
            auto r = append_cap_to_row(out,
                                       row,
                                       false,
                                       constraints.required_caps.ids[i],
                                       constraints.required_caps.names[i]);
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
        out.node_kinds[row] = materialized_node_kind::recipe;
        out.connection_source_storage[row] = {};
        out.connection_sink_storage[row] = {};
        out.connection_mode_storage[row] = {};
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
        cap_list_traits<typename Recipe::provides_list>::for_each_named([&](CapId cap, std::string_view name) {
            if (!util::ok(error)) {
                return;
            }
            auto seen = append_provided(out, cap);
            if (!seen) {
                error = seen.error();
                return;
            }
            auto r = append_cap_to_row(out, row, true, cap, name);
            if (!r) {
                error = r.error();
            }
        });
        if (!util::ok(error)) {
            return util::unexpected(error);
        }

        cap_list_traits<typename Recipe::requires_list>::for_each_named([&](CapId cap, std::string_view name) {
            if (!util::ok(error)) {
                return;
            }
            auto r = append_cap_to_row(out, row, false, cap, name);
            if (!r) {
                error = r.error();
            }
        });
        if (!util::ok(error)) {
            return util::unexpected(error);
        }

        for (util::usize i = 0; i < constraints.required_caps.count; ++i) {
            auto r = append_cap_to_row(out,
                                       row,
                                       false,
                                       constraints.required_caps.ids[i],
                                       constraints.required_caps.names[i]);
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

    template <util::usize MaxNodes, util::usize MaxCaps, typename Item>
    util::Result<materialize_summary<MaxCaps>> append_single_node_value(materialized_graph<MaxNodes, MaxCaps>& out,
                                                                        const Item& value,
                                                                        const materialize_constraints<MaxCaps>& constraints) noexcept {
        materialize_summary<MaxCaps> summary{};
        const auto metadata = describe_single_node(value);
        auto current = append_node_with_lookup(out,
                                               value.node,
                                               constraints,
                                               [&](CapId id) noexcept {
                                                   return init::lookup_capability_name(value, id);
                                               },
                                               metadata);
        if (!current) {
            return util::unexpected(current.error());
        }
        auto merge = absorb_summary(summary, *current);
        if (!merge) {
            return util::unexpected(merge.error());
        }
        return summary;
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

    template <util::usize MaxNodes, util::usize MaxCaps, typename Item>
    util::Result<materialize_summary<MaxCaps>> materialize_item(materialized_graph<MaxNodes, MaxCaps>& out,
                                                                const Item& item,
                                                                const materialize_constraints<MaxCaps>& constraints) noexcept {
        if constexpr (is_bound_recipe_v<Item>) {
            return append_recipe(out, item, constraints);
        } else if constexpr (is_single_node_ref_v<Item>) {
            if (!item.value) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            return append_single_node_value(out, *item.value, constraints);
        } else if constexpr (is_maybe_ref_v<Item>) {
            if (!item.value || !item.value->has_value()) {
                return materialize_summary<MaxCaps>{};
            }
            return materialize_item(out, item.value->value(), constraints);
        } else if constexpr (is_plan_v<Item>) {
            return materialize_tuple<0, MaxNodes, MaxCaps>(out, item.items, constraints);
        } else if constexpr (is_after_plan_v<Item>) {
            using requires_type = typename after_plan_traits<Item>::requires_type;
            auto merged = constraints;
            auto r = append_type_caps<requires_type>(merged.required_caps);
            if (!r) {
                return util::unexpected(r.error());
            }
            return materialize_item(out, item.inner, merged);
        } else if constexpr (is_phase_limit_plan_v<Item>) {
            auto merged = constraints;
            merged.max_phase = phase_min(merged.max_phase, item.value);
            return materialize_item(out, item.inner, merged);
        } else if constexpr (is_runlevel_plan_v<Item>) {
            auto merged = constraints;
            merged.runlevel_mask &= item.mask;
            return materialize_item(out, item.inner, merged);
        } else if constexpr (is_export_plan_v<Item>) {
            using cap_type = typename export_plan_traits<Item>::cap_type;
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
            barrier.name = cap_type::view();
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
            out.node_kinds[row] = materialized_node_kind::barrier;
            out.connection_source_storage[row] = {};
            out.connection_sink_storage[row] = {};
            out.connection_mode_storage[row] = {};
            out.provides_count[row] = 0;
            out.requires_count[row] = 0;

            auto seen = append_provided(out, cap_type::id);
            if (!seen) {
                return util::unexpected(seen.error());
            }
            auto provide_result = append_cap_to_row(out, row, true, cap_type::id, cap_type::view());
            if (!provide_result) {
                return util::unexpected(provide_result.error());
            }
            for (util::usize i = 0; i < summary->provides.count; ++i) {
                auto require_result = append_cap_to_row(out,
                                                        row,
                                                        false,
                                                        summary->provides.ids[i],
                                                        summary->provides.names[i]);
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
        } else if constexpr (requires(const Item& candidate) {
                                 candidate.plan();
                             }) {
            return materialize_item(out, item.plan(), constraints);
        } else if constexpr (requires(const Item& candidate) {
                                 candidate.node;
                             }) {
            return append_single_node_value(out, item, constraints);
        } else {
            static_assert(unsupported_materialize_item_v<Item>,
                          "init::materialize(...) expects recipe bindings, composed plans, optional plans, or single-node bindings");
        }
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
        if (mats->node_kinds[0] != materialized_node_kind::recipe
            || mats->node_kinds[1] != materialized_node_kind::recipe
            || mats->node_kinds[2] != materialized_node_kind::barrier) {
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

            constexpr std::string_view capability_name(CapId id) const noexcept {
                return id == CapA::id ? CapA::view() : std::string_view{};
            }

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
        auto binding_holder = materialize<2, 4>(compose(as_plan(holder)));
        if (!binding_holder || binding_holder->size() != 1) {
            return util::unexpected(util::Errc::bad_state);
        }
        if (binding_holder->node_kinds[0] != materialized_node_kind::binding) {
            return util::unexpected(util::Errc::bad_state);
        }

        std::optional<LegacyNodeHolder> optional_holder{LegacyNodeHolder{}};
        auto maybe_optional = materialize<2, 4>(compose(maybe(optional_holder)));
        if (!maybe_optional || maybe_optional->size() != 1) {
            return util::unexpected(util::Errc::bad_state);
        }
        if (maybe_optional->node_kinds[0] != materialized_node_kind::binding) {
            return util::unexpected(util::Errc::bad_state);
        }

        auto connection = direct_connection<"test.connection", CapA, CapB>();
        auto connection_mats = materialize<2, 4>(compose(as_plan(connection)));
        if (!connection_mats || connection_mats->size() != 1) {
            return util::unexpected(util::Errc::bad_state);
        }
        if (connection_mats->node_kinds[0] != materialized_node_kind::connection) {
            return util::unexpected(util::Errc::bad_state);
        }
        if (connection_mats->connection_source_storage[0] != CapA::view()
            || connection_mats->connection_sink_storage[0] != CapB::view()
            || connection_mats->connection_mode_storage[0] != "direct") {
            return util::unexpected(util::Errc::bad_state);
        }
        return {};
    }
#endif
}
