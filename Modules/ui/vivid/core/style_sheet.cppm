module;
#include <array>
#include <cstddef>
#include <cstdint>
export module charm.core.style_sheet;

export import charm.core.style;
export import charm.core.handle;

export
enum class StyleStateFlag : std::uint8_t {
    Hovered = 1 << 0,
    Pressed = 1 << 1,
    Focused = 1 << 2,
    Disabled = 1 << 3
};

export
struct StyleSelector {
    WidgetKind kind{WidgetKind::None};
    std::uint8_t require_mask{0};
};

export
struct StyleRule {
    StyleSelector selector{};
    StylePatch patch{};
};

export
class StyleSheet {
public:
    static StyleSheet& instance() {
        static StyleSheet inst;
        return inst;
    }

    void clear() noexcept { count_ = 0; }

    bool add_rule(const StyleSelector& sel, const StylePatch& patch) noexcept {
        if (count_ >= rules_.size()) return false;
        rules_[count_++] = StyleRule{sel, patch};
        return true;
    }

    void apply(WidgetKind kind, const StyleState& state, Style& style) const noexcept {
        const std::uint8_t mask = state_mask(state);
        for (std::size_t i = 0; i < count_; ++i) {
            const auto& rule = rules_[i];
            if (rule.selector.kind != WidgetKind::None && rule.selector.kind != kind) {
                continue;
            }
            if ((mask & rule.selector.require_mask) != rule.selector.require_mask) {
                continue;
            }
            rule.patch.apply_to(style);
        }
    }

private:
    static std::uint8_t state_mask(const StyleState& state) noexcept {
        std::uint8_t mask = 0;
        if (state.hovered) mask |= static_cast<std::uint8_t>(StyleStateFlag::Hovered);
        if (state.pressed) mask |= static_cast<std::uint8_t>(StyleStateFlag::Pressed);
        if (state.focused) mask |= static_cast<std::uint8_t>(StyleStateFlag::Focused);
        if (!state.enabled) mask |= static_cast<std::uint8_t>(StyleStateFlag::Disabled);
        return mask;
    }

    std::array<StyleRule, 32> rules_{};
    std::size_t count_{0};
};

export
inline void apply_style_sheet(WidgetKind kind, const StyleState& state, Style& style) noexcept {
    StyleSheet::instance().apply(kind, state, style);
}
