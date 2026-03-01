module;
#include <array>
#include <cstddef>
#include <cstdint>

export module charm.core.soa_layout;

export import charm.core.soa_kernel;
export import charm.core.container;
export import charm.core.style;
export import charm.core.style_sheet;
export import charm.widgets.list;
export import charm.widgets.scroll_container;

export
class SoaLayoutPass {
public:
    explicit SoaLayoutPass(SoaKernel& kernel) noexcept
        : kernel_(kernel) {}

    void run_if_needed(WidgetHandle root) noexcept {
        if (!root) return;
        const std::uint32_t tokens_version = Theme::instance().get_tokens().version;
        const std::uint32_t dirty = kernel_.layout_dirty_version();
        if (dirty == kernel_.layout_applied_version() && tokens_version == tokens_version_) {
            return;
        }
        refresh_styles();
        layout_tree(root);
        kernel_.set_layout_applied_version(dirty);
#if defined(VIVID_SOA_TRACE_INPUT)
        kernel_.layout_trace_on_pass();
#endif
        tokens_version_ = tokens_version;
    }

private:
    static constexpr std::size_t kWidgetKindCount =
        static_cast<std::size_t>(WidgetKind::Histogram) + 1;

    struct StyleTable {
        std::array<Style, kWidgetKindCount> styles{};
    };

    SoaKernel& kernel_;
    StyleTable style_table_{};
    std::uint32_t style_version_{0};
    std::uint32_t tokens_version_{0};

    static const Style& style_for_kind(const StyleTable& table, WidgetKind kind) noexcept {
        const auto idx = static_cast<std::size_t>(kind);
        if (idx >= table.styles.size()) {
            return table.styles[static_cast<std::size_t>(WidgetKind::Container)];
        }
        return table.styles[idx];
    }

    static StyleState make_state(const SoaKernel& kernel, WidgetHandle h) noexcept {
        const StateCompact state = kernel.state_compact(h);
        const bool enabled = state.enabled();
        const bool influence = kernel.layout_state_influence();
        const bool hovered = influence ? state.hovered() : false;
        const bool pressed = influence ? state.pressed() : false;
        const bool focused = influence ? state.focused() : false;
        return make_style_state(enabled, hovered, pressed, focused, state.variant);
    }

    void refresh_styles() {
        const auto version = Theme::instance().get_tokens().version;
        if (version == style_version_) return;
        style_version_ = version;
        const Style fallback = Theme::instance().get<Container>();
        style_table_.styles.fill(fallback);
        style_table_.styles[static_cast<std::size_t>(WidgetKind::Container)] = Theme::instance().get<Container>();
        style_table_.styles[static_cast<std::size_t>(WidgetKind::List)] = Theme::instance().get<List>();
        style_table_.styles[static_cast<std::size_t>(WidgetKind::ScrollContainer)] = Theme::instance().get<ScrollContainer>();
    }

    const Style& resolve_style(WidgetKind kind, const StyleState& state, Style& scratch) const noexcept {
        const Style& base = style_for_kind(style_table_, kind);
        if (StyleSheet::instance().apply(kind, state, scratch, base)) {
            return scratch;
        }
        return base;
    }

    void layout_tree(WidgetHandle root) noexcept {
        std::array<WidgetHandle, 256> stack{};
        std::size_t sp = 0;
        stack[sp++] = root;
        while (sp > 0) {
            WidgetHandle h = stack[--sp];
            if (!kernel_.valid(h) || !kernel_.visible(h)) continue;
            const SoaLayoutKind kind = kernel_.layout_kind(h);
            if (kind == SoaLayoutKind::List) {
                Style scratch;
                const WidgetKind widget_kind = kernel_.kind(h);
                const StyleState state = make_state(kernel_, h);
                const Style& st = resolve_style(widget_kind, state, scratch);
                kernel_.apply_list_layout(h, st.metrics.padding);
                kernel_.set_scroll_y_clamped(h, kernel_.scroll_y(h));
            }
            for (auto child = kernel_.last_child(h); child; child = kernel_.prev_sibling(child)) {
                if (sp < stack.size()) {
                    stack[sp++] = child;
                }
            }
        }
    }
};
