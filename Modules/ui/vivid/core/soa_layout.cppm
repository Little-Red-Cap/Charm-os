module;
#include <array>
#include <cstddef>
#include <cstdint>

#include "features.hpp"

export module charm.core.soa_layout;

export import charm.core.soa_kernel;
export import charm.core.style;
export import charm.core.style_sheet;
export import charm.core.widget_registry;

export
class SoaLayoutPass {
public:
    explicit SoaLayoutPass(SoaKernel& kernel) noexcept
        : kernel_(kernel) {}

    void run_if_needed(WidgetHandle root) noexcept {
        if (!root) return;
        const std::uint32_t tokens_version = Theme::instance().get_tokens().version;
        const std::uint32_t stylesheet_version = StyleSheet::instance().stylesheet_version();
        const std::uint32_t dirty = kernel_.layout_dirty_version();
        if (dirty == kernel_.layout_applied_version() &&
            tokens_version == tokens_version_ &&
            stylesheet_version == stylesheet_version_) {
            return;
        }
        layout_tree(root);
        kernel_.set_layout_applied_version(dirty);
#if defined(VIVID_SOA_TRACE_INPUT)
        kernel_.layout_trace_on_pass();
#endif
        tokens_version_ = tokens_version;
        stylesheet_version_ = stylesheet_version;
    }

private:
    SoaKernel& kernel_;
    std::uint32_t tokens_version_{0};
    std::uint32_t stylesheet_version_{0};

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

    void layout_tree(WidgetHandle root) noexcept {
        std::array<WidgetHandle, 256> stack{};
        std::size_t sp = 0;
        stack[sp++] = root;
        while (sp > 0) {
            WidgetHandle h = stack[--sp];
            if (!kernel_.valid(h) || !kernel_.visible(h)) continue;
            const SoaLayoutKind kind = kernel_.layout_kind(h);
            if (kind == SoaLayoutKind::List) {
                const WidgetKind widget_kind = kernel_.kind(h);
                const StyleState state = make_state(kernel_, h);
                const ResolvedStyleView view = StyleSheet::instance().lookup(widget_kind, state);
                const int pad = view.metrics ? view.metrics->padding : 0;
                kernel_.apply_list_layout(h, pad);
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
