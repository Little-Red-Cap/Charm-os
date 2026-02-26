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
constexpr std::uint8_t kStyleVariantAny = 0xFF;

export
struct StyleSelector {
    WidgetKind kind{WidgetKind::None};
    std::uint8_t require_mask{0};
    std::uint8_t variant{kStyleVariantAny};
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
    SurfaceHover,
    SurfacePressed,
    OnSurface,
    OnSurfaceMuted,
    Outline,
    OutlineHover,
    OutlinePressed,
    Accent,
    AccentHover,
    AccentPressed,
    OnAccent,
    Danger,
    OnDanger,
    FocusRing
};

export
struct StyleRolePatch {
    bool has_bg_color{false};
    bool has_bg_hover{false};
    bool has_bg_pressed{false};
    bool has_border_color{false};
    bool has_border_hover{false};
    bool has_border_pressed{false};
    bool has_font_color{false};
    bool has_accent_color{false};
    bool has_on_accent{false};
    bool has_border_focus{false};

    StyleRole bg_color{StyleRole::Surface};
    StyleRole bg_hover{StyleRole::SurfaceHover};
    StyleRole bg_pressed{StyleRole::SurfacePressed};
    StyleRole border_color{StyleRole::Outline};
    StyleRole border_hover{StyleRole::OutlineHover};
    StyleRole border_pressed{StyleRole::OutlinePressed};
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
    std::uint16_t priority{0};
    std::uint16_t order{0};
};

inline rgba role_color(StyleRole role, const ThemeTokens& t) noexcept {
    switch (role) {
    case StyleRole::Surface: return t.surface;
    case StyleRole::SurfaceVariant: return t.surface_variant;
    case StyleRole::SurfaceHover: return adjust_by_luma(t.surface, 8);
    case StyleRole::SurfacePressed: return adjust_by_luma(t.surface, 20);
    case StyleRole::OnSurface: return t.on_surface;
    case StyleRole::OnSurfaceMuted: return t.on_surface_muted;
    case StyleRole::Outline: return t.outline;
    case StyleRole::OutlineHover: return adjust_by_luma(t.outline, 20);
    case StyleRole::OutlinePressed: return adjust_by_luma(t.outline, 40);
    case StyleRole::Accent: return t.accent;
    case StyleRole::AccentHover: return adjust_by_luma(t.accent, 12);
    case StyleRole::AccentPressed: return adjust_by_luma(t.accent, 24);
    case StyleRole::OnAccent: return t.on_accent;
    case StyleRole::Danger: return t.danger;
    case StyleRole::OnDanger: return t.on_danger;
    case StyleRole::FocusRing: return t.focus_ring;
    default: return t.surface;
    }
}

inline void apply_role_patch(Style& style, const StyleRolePatch& patch, const ThemeTokens& tokens) noexcept {
    if (patch.has_bg_color) style.bg_color = role_color(patch.bg_color, tokens);
    if (patch.has_bg_hover) style.bg_hover = role_color(patch.bg_hover, tokens);
    if (patch.has_bg_pressed) style.bg_pressed = role_color(patch.bg_pressed, tokens);
    if (patch.has_border_color) style.border_color = role_color(patch.border_color, tokens);
    if (patch.has_border_hover) style.border_hover = role_color(patch.border_hover, tokens);
    if (patch.has_border_pressed) style.border_pressed = role_color(patch.border_pressed, tokens);
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

    void clear() noexcept {
        count_ = 0;
        order_ = 0;
    }

    bool add_rule(const StyleSelector& sel, const StylePatch& patch) noexcept {
        if (count_ >= rules_.size()) return false;
        StyleRuleEntry entry{};
        entry.selector = sel;
        entry.kind = StyleRuleKind::Patch;
        entry.patch = patch;
        entry.priority = rule_priority(sel);
        entry.order = order_++;
        insert_rule(entry);
        return true;
    }

    bool add_role_rule(const StyleSelector& sel, const StyleRolePatch& patch) noexcept {
        if (count_ >= rules_.size()) return false;
        StyleRuleEntry entry{};
        entry.selector = sel;
        entry.kind = StyleRuleKind::RolePatch;
        entry.role_patch = patch;
        entry.priority = rule_priority(sel);
        entry.order = order_++;
        insert_rule(entry);
        return true;
    }

    void apply(WidgetKind kind, const StyleState& state, Style& style) const noexcept {
        const std::uint8_t mask = state_mask(state);
        for (std::size_t i = 0; i < count_; ++i) {
            const auto& rule = rules_[i];
            if (rule.selector.kind != WidgetKind::None && rule.selector.kind != kind) {
                continue;
            }
            if (rule.selector.variant != kStyleVariantAny && rule.selector.variant != state.variant) {
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
    static int mask_weight(std::uint8_t mask) noexcept {
        int count = 0;
        while (mask) {
            count += (mask & 1u) ? 1 : 0;
            mask >>= 1u;
        }
        return count;
    }

    static std::uint16_t rule_priority(const StyleSelector& sel) noexcept {
        const std::uint16_t kind_score = (sel.kind == WidgetKind::None) ? 0u : 1u;
        const std::uint16_t variant_score = (sel.variant == kStyleVariantAny) ? 0u : 1u;
        const std::uint16_t state_score = static_cast<std::uint16_t>(mask_weight(sel.require_mask));
        return static_cast<std::uint16_t>((kind_score << 8) | (variant_score << 7) | (state_score << 4));
    }

    void insert_rule(const StyleRuleEntry& entry) noexcept {
        std::size_t pos = count_;
        for (std::size_t i = 0; i < count_; ++i) {
            const auto& cur = rules_[i];
            if (entry.priority < cur.priority) {
                pos = i;
                break;
            }
        }
        if (pos < count_) {
            for (std::size_t i = count_; i > pos; --i) {
                rules_[i] = rules_[i - 1];
            }
        }
        rules_[pos] = entry;
        ++count_;
    }

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
    std::uint16_t order_{0};
};

export
inline void apply_style_sheet(WidgetKind kind, const StyleState& state, Style& style) noexcept {
    StyleSheet::instance().apply(kind, state, style);
}
