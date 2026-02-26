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
enum class StyleRole : std::uint8_t {
    Surface,
    SurfaceVariant,
    OnSurface,
    OnSurfaceMuted,
    Outline,
    Accent,
    OnAccent,
    Danger,
    OnDanger,
    FocusRing
};

export
struct StyleRolePatch {
    bool has_bg_color{false};
    bool has_border_color{false};
    bool has_font_color{false};
    bool has_accent_color{false};
    bool has_on_accent{false};
    bool has_border_focus{false};

    StyleRole bg_color{StyleRole::Surface};
    StyleRole border_color{StyleRole::Outline};
    StyleRole font_color{StyleRole::OnSurface};
    StyleRole accent_color{StyleRole::Accent};
    StyleRole on_accent{StyleRole::OnAccent};
    StyleRole border_focus{StyleRole::FocusRing};
};

enum class StyleRuleKind : std::uint8_t {
    Patch,
    RolePatch
};

struct StyleRuleEntry {
    StyleSelector selector{};
    StyleRuleKind kind{StyleRuleKind::Patch};
    StylePatch patch{};
    StyleRolePatch role_patch{};
};

inline rgba role_color(StyleRole role, const ThemeTokens& t) noexcept {
    switch (role) {
    case StyleRole::Surface: return t.surface;
    case StyleRole::SurfaceVariant: return t.surface_variant;
    case StyleRole::OnSurface: return t.on_surface;
    case StyleRole::OnSurfaceMuted: return t.on_surface_muted;
    case StyleRole::Outline: return t.outline;
    case StyleRole::Accent: return t.accent;
    case StyleRole::OnAccent: return t.on_accent;
    case StyleRole::Danger: return t.danger;
    case StyleRole::OnDanger: return t.on_danger;
    case StyleRole::FocusRing: return t.focus_ring;
    default: return t.surface;
    }
}

inline void apply_role_patch(Style& style, const StyleRolePatch& patch, const ThemeTokens& tokens) noexcept {
    if (patch.has_bg_color) style.bg_color = role_color(patch.bg_color, tokens);
    if (patch.has_border_color) style.border_color = role_color(patch.border_color, tokens);
    if (patch.has_font_color) style.font_color = role_color(patch.font_color, tokens);
    if (patch.has_accent_color) style.accent_color = role_color(patch.accent_color, tokens);
    if (patch.has_on_accent) style.on_accent = role_color(patch.on_accent, tokens);
    if (patch.has_border_focus) style.border_focus = role_color(patch.border_focus, tokens);
}

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
        rules_[count_++] = StyleRuleEntry{sel, StyleRuleKind::Patch, patch, {}};
        return true;
    }

    bool add_role_rule(const StyleSelector& sel, const StyleRolePatch& patch) noexcept {
        if (count_ >= rules_.size()) return false;
        StyleRuleEntry entry{};
        entry.selector = sel;
        entry.kind = StyleRuleKind::RolePatch;
        entry.role_patch = patch;
        rules_[count_++] = entry;
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
            if (rule.kind == StyleRuleKind::Patch) {
                rule.patch.apply_to(style);
            } else {
                apply_role_patch(style, rule.role_patch, Theme::instance().get_tokens());
            }
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

    std::array<StyleRuleEntry, 32> rules_{};
    std::size_t count_{0};
};

export
inline void apply_style_sheet(WidgetKind kind, const StyleState& state, Style& style) noexcept {
    StyleSheet::instance().apply(kind, state, style);
}
