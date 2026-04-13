module;

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <span>
#include <string_view>

export module init.observe;

import init.meta;
import init.materialize;
import init.node;
import init.plan;
import init.recipe;
import util.core;
import util.error;

namespace init::detail {
    inline std::size_t append_text(char* out,
                                   std::size_t max,
                                   std::size_t offset,
                                   std::string_view sv) noexcept {
        if (!out || max == 0 || offset >= max) {
            return offset;
        }
        const std::size_t avail = max - offset - 1u;
        const std::size_t n = sv.size() < avail ? sv.size() : avail;
        if (n > 0) {
            std::memcpy(out + offset, sv.data(), n);
        }
        out[offset + n] = '\0';
        return offset + n;
    }

    inline std::size_t append_char(char* out,
                                   std::size_t max,
                                   std::size_t offset,
                                   char value) noexcept {
        if (!out || max == 0 || offset >= max) {
            return offset;
        }
        if (offset + 1 >= max) {
            out[max - 1] = '\0';
            return max - 1;
        }
        out[offset] = value;
        out[offset + 1] = '\0';
        return offset + 1;
    }

    template <typename... Args>
    inline std::size_t append_fmt(char* out,
                                  std::size_t max,
                                  std::size_t offset,
                                  const char* fmt,
                                  Args... args) noexcept {
        if (!out || max == 0 || offset >= max) {
            return offset;
        }
        const int written = std::snprintf(out + offset, max - offset, fmt, args...);
        if (written <= 0) {
            out[offset] = '\0';
            return offset;
        }
        const auto count = static_cast<std::size_t>(written);
        if (count >= max - offset) {
            out[max - 1] = '\0';
            return max - 1;
        }
        return offset + count;
    }

    inline std::size_t append_cap_id(char* out,
                                     std::size_t max,
                                     std::size_t offset,
                                     CapId cap) noexcept {
        return append_fmt(out,
                          max,
                          offset,
                          "0x%08llX",
                          static_cast<unsigned long long>(cap));
    }

    inline std::size_t append_cap_display(char* out,
                                          std::size_t max,
                                          std::size_t offset,
                                          CapId cap,
                                          std::string_view name) noexcept {
        if (!name.empty()) {
            offset = append_text(out, max, offset, name);
            offset = append_text(out, max, offset, " (");
            offset = append_cap_id(out, max, offset, cap);
            offset = append_char(out, max, offset, ')');
            return offset;
        }
        return append_cap_id(out, max, offset, cap);
    }

    inline std::size_t append_runlevel_mask(char* out,
                                            std::size_t max,
                                            std::size_t offset,
                                            util::u32 mask) noexcept {
        if (mask == static_cast<util::u32>(Runlevel::all)) {
            return append_text(out, max, offset, "all");
        }
        if (mask == static_cast<util::u32>(Runlevel::none)) {
            return append_text(out, max, offset, "none");
        }

        bool appended = false;
        const auto known = static_cast<util::u32>(Runlevel::tiny)
                         | static_cast<util::u32>(Runlevel::full);
        if ((mask & static_cast<util::u32>(Runlevel::tiny)) != 0u) {
            offset = append_text(out, max, offset, "tiny");
            appended = true;
        }
        if ((mask & static_cast<util::u32>(Runlevel::full)) != 0u) {
            if (appended) {
                offset = append_char(out, max, offset, '|');
            }
            offset = append_text(out, max, offset, "full");
            appended = true;
        }

        const auto extra = mask & ~known;
        if (extra != 0u || !appended) {
            if (appended) {
                offset = append_char(out, max, offset, '|');
            }
            offset = append_fmt(out,
                                max,
                                offset,
                                "0x%08llX",
                                static_cast<unsigned long long>(extra != 0u ? extra : mask));
        }
        return offset;
    }

    inline std::size_t append_cap_list(char* out,
                                       std::size_t max,
                                       std::size_t offset,
                                       std::span<const CapId> caps,
                                       std::span<const std::string_view> names) noexcept {
        if (caps.empty()) {
            return append_text(out, max, offset, "-");
        }
        for (std::size_t i = 0; i < caps.size(); ++i) {
            if (i > 0) {
                offset = append_text(out, max, offset, ", ");
            }
            const auto name = i < names.size() ? names[i] : std::string_view{};
            offset = append_cap_display(out, max, offset, caps[i], name);
        }
        return offset;
    }

    inline std::size_t append_dot_escaped(char* out,
                                          std::size_t max,
                                          std::size_t offset,
                                          std::string_view sv) noexcept {
        for (char ch : sv) {
            switch (ch) {
                case '\\':
                    offset = append_text(out, max, offset, "\\\\");
                    break;
                case '"':
                    offset = append_text(out, max, offset, "\\\"");
                    break;
                case '\n':
                    offset = append_text(out, max, offset, "\\n");
                    break;
                case '\r':
                    break;
                case '\t':
                    offset = append_text(out, max, offset, "\\t");
                    break;
                default:
                    offset = append_char(out, max, offset, ch);
                    break;
            }
        }
        return offset;
    }

    inline std::size_t append_json_escaped(char* out,
                                           std::size_t max,
                                           std::size_t offset,
                                           std::string_view sv) noexcept {
        for (char ch : sv) {
            switch (ch) {
                case '\\':
                    offset = append_text(out, max, offset, "\\\\");
                    break;
                case '"':
                    offset = append_text(out, max, offset, "\\\"");
                    break;
                case '\b':
                    offset = append_text(out, max, offset, "\\b");
                    break;
                case '\f':
                    offset = append_text(out, max, offset, "\\f");
                    break;
                case '\n':
                    offset = append_text(out, max, offset, "\\n");
                    break;
                case '\r':
                    offset = append_text(out, max, offset, "\\r");
                    break;
                case '\t':
                    offset = append_text(out, max, offset, "\\t");
                    break;
                default:
                    if (static_cast<unsigned char>(ch) < 0x20u) {
                        offset = append_fmt(out,
                                            max,
                                            offset,
                                            "\\u%04X",
                                            static_cast<unsigned int>(static_cast<unsigned char>(ch)));
                    } else {
                        offset = append_char(out, max, offset, ch);
                    }
                    break;
            }
        }
        return offset;
    }

    inline std::size_t append_json_string(char* out,
                                          std::size_t max,
                                          std::size_t offset,
                                          std::string_view sv) noexcept {
        offset = append_char(out, max, offset, '"');
        offset = append_json_escaped(out, max, offset, sv);
        offset = append_char(out, max, offset, '"');
        return offset;
    }

    inline std::size_t append_json_cap_array(char* out,
                                             std::size_t max,
                                             std::size_t offset,
                                             std::span<const CapId> caps,
                                             std::span<const std::string_view> names) noexcept {
        offset = append_char(out, max, offset, '[');
        for (std::size_t i = 0; i < caps.size(); ++i) {
            if (i > 0) {
                offset = append_char(out, max, offset, ',');
            }
            offset = append_text(out, max, offset, "{\"id\":");
            offset = append_char(out, max, offset, '"');
            offset = append_cap_id(out, max, offset, caps[i]);
            offset = append_char(out, max, offset, '"');
            offset = append_text(out, max, offset, ",\"name\":");
            offset = append_json_string(out,
                                        max,
                                        offset,
                                        i < names.size() ? names[i] : std::string_view{});
            offset = append_char(out, max, offset, '}');
        }
        offset = append_char(out, max, offset, ']');
        return offset;
    }

    [[nodiscard]] constexpr const char* node_shape(materialized_node_kind kind) noexcept {
        switch (kind) {
            case materialized_node_kind::recipe: return "box";
            case materialized_node_kind::barrier: return "diamond";
            case materialized_node_kind::legacy: return "ellipse";
            case materialized_node_kind::unknown: return "box";
        }
        return "box";
    }

}

export namespace init {
    [[nodiscard]] constexpr std::string_view to_text(Phase phase) noexcept {
        switch (phase) {
            case Phase::early: return "early";
            case Phase::core: return "core";
            case Phase::service: return "service";
            case Phase::app: return "app";
        }
        return "unknown";
    }

    [[nodiscard]] constexpr std::string_view to_text(materialized_node_kind kind) noexcept {
        switch (kind) {
            case materialized_node_kind::unknown: return "unknown";
            case materialized_node_kind::recipe: return "recipe";
            case materialized_node_kind::barrier: return "barrier";
            case materialized_node_kind::legacy: return "legacy";
        }
        return "unknown";
    }

    struct materialized_node_view {
        util::usize index{0};
        std::string_view name{};
        Phase phase{Phase::early};
        util::u32 runlevel_mask{0};
        std::span<const CapId> provides{};
        std::span<const std::string_view> provide_names{};
        std::span<const CapId> requires_caps{};
        std::span<const std::string_view> require_names{};
        materialized_node_kind kind{materialized_node_kind::unknown};
    };

    struct materialized_edge_view {
        util::usize provider_index{0};
        util::usize consumer_index{0};
        CapId capability{0};
    };

    template <util::usize MaxNodes, util::usize MaxCaps>
    struct materialized_graph_view {
        static constexpr util::usize max_edges = MaxNodes * MaxCaps;

        std::array<materialized_node_view, MaxNodes> nodes{};
        std::array<materialized_edge_view, max_edges> edges{};
        util::usize node_count{0};
        util::usize edge_count{0};
        util::u32 effective_runlevel_mask{static_cast<util::u32>(Runlevel::all)};
        Phase effective_max_phase{Phase::app};

        [[nodiscard]] constexpr std::span<const materialized_node_view> node_span() const noexcept {
            return std::span<const materialized_node_view>{nodes.data(), node_count};
        }

        [[nodiscard]] constexpr std::span<const materialized_edge_view> edge_span() const noexcept {
            return std::span<const materialized_edge_view>{edges.data(), edge_count};
        }

        [[nodiscard]] constexpr const materialized_node_view* find_node(util::usize index) const noexcept {
            return index < node_count ? &nodes[index] : nullptr;
        }
    };

    template <util::usize MaxNodes, util::usize MaxCaps>
    [[nodiscard]] constexpr materialized_graph_view<MaxNodes, MaxCaps>
    observe(const materialized_graph<MaxNodes, MaxCaps>& mats) noexcept {
        materialized_graph_view<MaxNodes, MaxCaps> out{};
        out.node_count = mats.size();
        out.effective_runlevel_mask = mats.build_runlevel_mask();
        out.effective_max_phase = mats.build_max_phase();

        for (util::usize row = 0; row < mats.size(); ++row) {
            const auto& node = mats.nodes[row];
            out.nodes[row] = materialized_node_view{
                .index = row,
                .name = node.name,
                .phase = node.phase,
                .runlevel_mask = node.runlevel_mask,
                .provides = std::span<const CapId>{mats.provides_storage[row].data(), mats.provides_count[row]},
                .provide_names = std::span<const std::string_view>{mats.provides_name_storage[row].data(), mats.provides_count[row]},
                .requires_caps = std::span<const CapId>{mats.requires_storage[row].data(), mats.requires_count[row]},
                .require_names = std::span<const std::string_view>{mats.requires_name_storage[row].data(), mats.requires_count[row]},
                .kind = mats.node_kinds[row],
            };
        }

        for (util::usize consumer = 0; consumer < out.node_count; ++consumer) {
            const auto& node = out.nodes[consumer];
            for (std::size_t required = 0; required < node.requires_caps.size(); ++required) {
                const auto cap = node.requires_caps[required];
                for (util::usize provider = 0; provider < out.node_count; ++provider) {
                    bool provides_cap = false;
                    for (std::size_t provided = 0; provided < out.nodes[provider].provides.size(); ++provided) {
                        if (out.nodes[provider].provides[provided] == cap) {
                            provides_cap = true;
                            break;
                        }
                    }
                    if (!provides_cap) {
                        continue;
                    }
                    out.edges[out.edge_count++] = materialized_edge_view{
                        .provider_index = provider,
                        .consumer_index = consumer,
                        .capability = cap,
                    };
                    break;
                }
            }
        }

        return out;
    }

    template <util::usize MaxNodes, util::usize MaxCaps>
    [[nodiscard]] constexpr std::string_view find_capability_name(const materialized_graph_view<MaxNodes, MaxCaps>& view,
                                                                  CapId cap) noexcept {
        for (util::usize node_index = 0; node_index < view.node_count; ++node_index) {
            const auto& node = view.nodes[node_index];
            for (std::size_t i = 0; i < node.provides.size(); ++i) {
                if (node.provides[i] == cap && i < node.provide_names.size() && !node.provide_names[i].empty()) {
                    return node.provide_names[i];
                }
            }
            for (std::size_t i = 0; i < node.requires_caps.size(); ++i) {
                if (node.requires_caps[i] == cap && i < node.require_names.size() && !node.require_names[i].empty()) {
                    return node.require_names[i];
                }
            }
        }
        return {};
    }

    template <util::usize MaxNodes, util::usize MaxCaps>
    std::size_t format_dot(const materialized_graph_view<MaxNodes, MaxCaps>& view,
                           char* out,
                           std::size_t max) noexcept {
        if (!out || max == 0) {
            return 0;
        }

        std::size_t offset = 0;
        out[0] = '\0';
        offset = detail::append_text(out, max, offset, "digraph materialized_graph {\n");
        offset = detail::append_text(out, max, offset, "  rankdir=LR;\n");
        offset = detail::append_text(out, max, offset, "  labelloc=t;\n");
        offset = detail::append_text(out, max, offset, "  label=\"");
        offset = detail::append_text(out, max, offset, "materialized_graph");
        offset = detail::append_text(out, max, offset, "\\nphase=");
        offset = detail::append_text(out, max, offset, to_text(view.effective_max_phase));
        offset = detail::append_text(out, max, offset, "\\nrunlevel=");
        offset = detail::append_runlevel_mask(out, max, offset, view.effective_runlevel_mask);
        offset = detail::append_text(out, max, offset, "\\nnodes=");
        offset = detail::append_fmt(out,
                                    max,
                                    offset,
                                    "%llu",
                                    static_cast<unsigned long long>(view.node_count));
        offset = detail::append_text(out, max, offset, "\\nedges=");
        offset = detail::append_fmt(out,
                                    max,
                                    offset,
                                    "%llu",
                                    static_cast<unsigned long long>(view.edge_count));
        offset = detail::append_text(out, max, offset, "\";\n");

        for (util::usize i = 0; i < view.node_count; ++i) {
            const auto& node = view.nodes[i];
            offset = detail::append_fmt(out,
                                        max,
                                        offset,
                                        "  n%llu [shape=%s, label=\"",
                                        static_cast<unsigned long long>(node.index),
                                        detail::node_shape(node.kind));
            offset = detail::append_fmt(out,
                                        max,
                                        offset,
                                        "%llu: ",
                                        static_cast<unsigned long long>(node.index));
            offset = detail::append_dot_escaped(out,
                                                max,
                                                offset,
                                                node.name.empty() ? std::string_view{"(unnamed)"} : node.name);
            offset = detail::append_text(out, max, offset, "\\nkind=");
            offset = detail::append_text(out, max, offset, to_text(node.kind));
            offset = detail::append_text(out, max, offset, "\\nphase=");
            offset = detail::append_text(out, max, offset, to_text(node.phase));
            offset = detail::append_text(out, max, offset, "\\nrunlevel=");
            offset = detail::append_runlevel_mask(out, max, offset, node.runlevel_mask);
            offset = detail::append_text(out, max, offset, "\\nprovides=");
            offset = detail::append_cap_list(out, max, offset, node.provides, node.provide_names);
            offset = detail::append_text(out, max, offset, "\\nrequires=");
            offset = detail::append_cap_list(out, max, offset, node.requires_caps, node.require_names);
            offset = detail::append_text(out, max, offset, "\"];\n");
        }

        for (util::usize i = 0; i < view.edge_count; ++i) {
            const auto& edge = view.edges[i];
            offset = detail::append_fmt(out,
                                        max,
                                        offset,
                                        "  n%llu -> n%llu [label=\"",
                                        static_cast<unsigned long long>(edge.provider_index),
                                        static_cast<unsigned long long>(edge.consumer_index));
            offset = detail::append_cap_display(out,
                                                max,
                                                offset,
                                                edge.capability,
                                                find_capability_name(view, edge.capability));
            offset = detail::append_text(out, max, offset, "\"];\n");
        }

        offset = detail::append_text(out, max, offset, "}\n");
        return offset >= max ? (max - 1) : offset;
    }

    template <util::usize MaxNodes, util::usize MaxCaps>
    std::size_t format_dot(const materialized_graph<MaxNodes, MaxCaps>& mats,
                           char* out,
                           std::size_t max) noexcept {
        return format_dot(observe(mats), out, max);
    }

    template <util::usize MaxNodes, util::usize MaxCaps>
    std::size_t format_json_sample(const materialized_graph_view<MaxNodes, MaxCaps>& view,
                                   char* out,
                                   std::size_t max) noexcept {
        if (!out || max == 0) {
            return 0;
        }

        std::size_t offset = 0;
        out[0] = '\0';
        offset = detail::append_char(out, max, offset, '{');

        offset = detail::append_text(out, max, offset, "\"schema\":");
        offset = detail::append_json_string(out, max, offset, "materialized_graph.sample/v2");
        offset = detail::append_text(out, max, offset, ",\"effective_max_phase\":");
        offset = detail::append_json_string(out, max, offset, to_text(view.effective_max_phase));
        offset = detail::append_text(out, max, offset, ",\"effective_runlevel_mask\":");
        offset = detail::append_fmt(out,
                                    max,
                                    offset,
                                    "%llu",
                                    static_cast<unsigned long long>(view.effective_runlevel_mask));
        offset = detail::append_text(out, max, offset, ",\"effective_runlevel_text\":");
        offset = detail::append_char(out, max, offset, '"');
        offset = detail::append_runlevel_mask(out, max, offset, view.effective_runlevel_mask);
        offset = detail::append_char(out, max, offset, '"');
        offset = detail::append_text(out, max, offset, ",\"node_count\":");
        offset = detail::append_fmt(out,
                                    max,
                                    offset,
                                    "%llu",
                                    static_cast<unsigned long long>(view.node_count));
        offset = detail::append_text(out, max, offset, ",\"edge_count\":");
        offset = detail::append_fmt(out,
                                    max,
                                    offset,
                                    "%llu",
                                    static_cast<unsigned long long>(view.edge_count));

        offset = detail::append_text(out, max, offset, ",\"nodes\":[");
        for (util::usize i = 0; i < view.node_count; ++i) {
            const auto& node = view.nodes[i];
            if (i > 0) {
                offset = detail::append_char(out, max, offset, ',');
            }
            offset = detail::append_char(out, max, offset, '{');
            offset = detail::append_text(out, max, offset, "\"index\":");
            offset = detail::append_fmt(out,
                                        max,
                                        offset,
                                        "%llu",
                                        static_cast<unsigned long long>(node.index));
            offset = detail::append_text(out, max, offset, ",\"name\":");
            offset = detail::append_json_string(out,
                                                max,
                                                offset,
                                                node.name.empty() ? std::string_view{""} : node.name);
            offset = detail::append_text(out, max, offset, ",\"kind\":");
            offset = detail::append_json_string(out, max, offset, to_text(node.kind));
            offset = detail::append_text(out, max, offset, ",\"phase\":");
            offset = detail::append_json_string(out, max, offset, to_text(node.phase));
            offset = detail::append_text(out, max, offset, ",\"runlevel_mask\":");
            offset = detail::append_fmt(out,
                                        max,
                                        offset,
                                        "%llu",
                                        static_cast<unsigned long long>(node.runlevel_mask));
            offset = detail::append_text(out, max, offset, ",\"runlevel_text\":");
            offset = detail::append_char(out, max, offset, '"');
            offset = detail::append_runlevel_mask(out, max, offset, node.runlevel_mask);
            offset = detail::append_char(out, max, offset, '"');
            offset = detail::append_text(out, max, offset, ",\"provides\":");
            offset = detail::append_json_cap_array(out, max, offset, node.provides, node.provide_names);
            offset = detail::append_text(out, max, offset, ",\"requires\":");
            offset = detail::append_json_cap_array(out, max, offset, node.requires_caps, node.require_names);
            offset = detail::append_char(out, max, offset, '}');
        }
        offset = detail::append_char(out, max, offset, ']');

        offset = detail::append_text(out, max, offset, ",\"edges\":[");
        for (util::usize i = 0; i < view.edge_count; ++i) {
            const auto& edge = view.edges[i];
            if (i > 0) {
                offset = detail::append_char(out, max, offset, ',');
            }
            offset = detail::append_char(out, max, offset, '{');
            offset = detail::append_text(out, max, offset, "\"provider_index\":");
            offset = detail::append_fmt(out,
                                        max,
                                        offset,
                                        "%llu",
                                        static_cast<unsigned long long>(edge.provider_index));
            offset = detail::append_text(out, max, offset, ",\"consumer_index\":");
            offset = detail::append_fmt(out,
                                        max,
                                        offset,
                                        "%llu",
                                        static_cast<unsigned long long>(edge.consumer_index));
            offset = detail::append_text(out, max, offset, ",\"capability\":");
            offset = detail::append_text(out, max, offset, "{\"id\":");
            offset = detail::append_char(out, max, offset, '"');
            offset = detail::append_cap_id(out, max, offset, edge.capability);
            offset = detail::append_char(out, max, offset, '"');
            offset = detail::append_text(out, max, offset, ",\"name\":");
            offset = detail::append_json_string(out,
                                                max,
                                                offset,
                                                find_capability_name(view, edge.capability));
            offset = detail::append_char(out, max, offset, '}');
            offset = detail::append_char(out, max, offset, '}');
        }
        offset = detail::append_text(out, max, offset, "]}");
        return offset >= max ? (max - 1) : offset;
    }

    template <util::usize MaxNodes, util::usize MaxCaps>
    std::size_t format_json_sample(const materialized_graph<MaxNodes, MaxCaps>& mats,
                                   char* out,
                                   std::size_t max) noexcept {
        return format_json_sample(observe(mats), out, max);
    }

#ifndef NDEBUG
    inline util::Result<void> observe_self_check() noexcept {
        using CapA = cap_c<"observe.a">;
        using CapB = cap_c<"observe.b">;
        using CapDone = cap_c<"observe.done">;

        struct DemoContext {
            util::u32 value{0};
        };

        using RecipeA = recipe_desc<
            "observe.a.init",
            Phase::early,
            static_cast<util::u32>(Runlevel::all),
            cap_list<CapA>,
            cap_list<>,
            DemoContext,
            nullptr>;
        using RecipeB = recipe_desc<
            "observe.b.init",
            Phase::service,
            static_cast<util::u32>(Runlevel::full),
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

        auto view = observe(*mats);
        if (view.node_count != 3 || view.edge_count != 3) {
            return util::unexpected(util::Errc::bad_state);
        }
        if (view.nodes[0].kind != materialized_node_kind::recipe
            || view.nodes[1].kind != materialized_node_kind::recipe
            || view.nodes[2].kind != materialized_node_kind::barrier) {
            return util::unexpected(util::Errc::bad_state);
        }

        std::array<char, 1024> dot{};
        const auto used = format_dot(view, dot.data(), dot.size());
        if (used == 0) {
            return util::unexpected(util::Errc::bad_state);
        }
        if (std::strstr(dot.data(), "digraph materialized_graph") == nullptr
            || std::strstr(dot.data(), "shape=diamond") == nullptr
            || std::strstr(dot.data(), "requires=") == nullptr) {
            return util::unexpected(util::Errc::bad_state);
        }

        std::array<char, 1536> json{};
        const auto json_used = format_json_sample(view, json.data(), json.size());
        if (json_used == 0) {
            return util::unexpected(util::Errc::bad_state);
        }
        if (std::strstr(json.data(), "\"schema\":\"materialized_graph.sample/v2\"") == nullptr
            || std::strstr(json.data(), "\"nodes\":[") == nullptr
            || std::strstr(json.data(), "\"edges\":[") == nullptr
            || std::strstr(json.data(), "\"kind\":\"barrier\"") == nullptr
            || std::strstr(json.data(), "\"name\":\"observe.done\"") == nullptr) {
            return util::unexpected(util::Errc::bad_state);
        }
        return {};
    }
#endif
}
