module;
#include <array>
#include <cstddef>
#include <cstdint>

#include "features.hpp"

export module charm.core.soa_layout;

export import charm.core.soa_kernel;
export import charm.core.container;
export import charm.core.style;
export import charm.core.style_sheet;
export import charm.core.widget_registry;
#if CHARM_VIVID_ENABLE_WIDGET_List
export import charm.widgets.list;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ScrollContainer
export import charm.widgets.scroll_container;
#endif

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
    static constexpr std::size_t kWidgetKindCount = enabled_widget_kind_count;

    struct StyleTable {
        std::array<Style, kWidgetKindCount> styles{};
    };

    SoaKernel& kernel_;
    StyleTable style_table_{};
    std::uint32_t style_version_{0};
    std::uint32_t tokens_version_{0};

    static const Style& style_for_kind(const StyleTable& table, WidgetKind kind) noexcept {
        const auto idx = widget_kind_index[static_cast<std::size_t>(kind)];
        if (idx == invalid_widget_kind_index || idx >= table.styles.size()) {
            const auto fallback_idx = widget_kind_index[static_cast<std::size_t>(WidgetKind::Container)];
            if (fallback_idx == invalid_widget_kind_index || fallback_idx >= table.styles.size()) {
                return table.styles[0];
            }
            return table.styles[fallback_idx];
        }
        return table.styles[idx];
    }

    static StyleState make_state(const SoaKernel& kernel, WidgetHandle h) noexcept {
        const StateCompact state = kernel.state_compact(h);
        const std::uint8_t raw_mask = kernel.layout_state_influence_mask(kernel.kind(h));
        const std::uint8_t mask = kernel.layout_state_influence() ? raw_mask : 0;
        const bool enabled = (mask & static_cast<std::uint8_t>(SoaStateMask::Enabled)) != 0
            ? state.enabled()
            : true;
        const bool hovered = (mask & static_cast<std::uint8_t>(SoaStateMask::Hovered)) != 0
            ? state.hovered()
            : false;
        const bool pressed = (mask & static_cast<std::uint8_t>(SoaStateMask::Pressed)) != 0
            ? state.pressed()
            : false;
        const bool focused = (mask & static_cast<std::uint8_t>(SoaStateMask::Focused)) != 0
            ? state.focused()
            : false;
        return make_style_state(enabled, hovered, pressed, focused, state.variant);
    }

    void refresh_styles() {
        const auto version = Theme::instance().get_tokens().version;
        if (version == style_version_) return;
        style_version_ = version;
        const Style fallback = Theme::instance().get<Container>();
        style_table_.styles.fill(fallback);
        const auto container_idx = widget_kind_index[static_cast<std::size_t>(WidgetKind::Container)];
        if (container_idx != invalid_widget_kind_index && container_idx < style_table_.styles.size()) {
            style_table_.styles[container_idx] = Theme::instance().get<Container>();
        }
#if CHARM_VIVID_ENABLE_WIDGET_List
        const auto list_idx = widget_kind_index[static_cast<std::size_t>(WidgetKind::List)];
        if (list_idx != invalid_widget_kind_index && list_idx < style_table_.styles.size()) {
            style_table_.styles[list_idx] = Theme::instance().get<List>();
        }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ScrollContainer
        const auto scroll_idx = widget_kind_index[static_cast<std::size_t>(WidgetKind::ScrollContainer)];
        if (scroll_idx != invalid_widget_kind_index && scroll_idx < style_table_.styles.size()) {
            style_table_.styles[scroll_idx] = Theme::instance().get<ScrollContainer>();
        }
#endif
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
