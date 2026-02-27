module;
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
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
    AccentDisabled,
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

inline constexpr std::size_t role_index(StyleRole role) noexcept {
    return static_cast<std::size_t>(role);
}

inline constexpr std::size_t kRoleCount = role_index(StyleRole::FocusRing) + 1;

struct RolePalette {
    std::array<rgba, kRoleCount> values{};
};

inline RolePalette build_palette(const ThemeTokens& t) noexcept {
    RolePalette p{};
    p.values[role_index(StyleRole::Surface)] = t.surface;
    p.values[role_index(StyleRole::SurfaceVariant)] = t.surface_variant;
    p.values[role_index(StyleRole::SurfaceHover)] = adjust_by_luma(t.surface, 8);
    p.values[role_index(StyleRole::SurfacePressed)] = adjust_by_luma(t.surface, 20);
    p.values[role_index(StyleRole::OnSurface)] = t.on_surface;
    p.values[role_index(StyleRole::OnSurfaceMuted)] = t.on_surface_muted;
    p.values[role_index(StyleRole::Outline)] = t.outline;
    p.values[role_index(StyleRole::OutlineHover)] = adjust_by_luma(t.outline, 20);
    p.values[role_index(StyleRole::OutlinePressed)] = adjust_by_luma(t.outline, 40);
    p.values[role_index(StyleRole::Accent)] = t.accent;
    p.values[role_index(StyleRole::AccentHover)] = adjust_by_luma(t.accent, 12);
    p.values[role_index(StyleRole::AccentPressed)] = adjust_by_luma(t.accent, 24);
    p.values[role_index(StyleRole::AccentDisabled)] = adjust_by_luma(t.accent, 40);
    p.values[role_index(StyleRole::OnAccent)] = t.on_accent;
    p.values[role_index(StyleRole::Danger)] = t.danger;
    p.values[role_index(StyleRole::OnDanger)] = t.on_danger;
    p.values[role_index(StyleRole::FocusRing)] = t.focus_ring;
    return p;
}

inline rgba role_color(const RolePalette& palette, StyleRole role) noexcept {
    const auto idx = role_index(role);
    return (idx < palette.values.size()) ? palette.values[idx] : rgba{};
}

inline void apply_role_patch(Style& style, const StyleRolePatch& patch, const RolePalette& palette) noexcept {
    if (patch.has_bg_color) style.colors.bg_color = role_color(palette, patch.bg_color);
    if (patch.has_bg_hover) style.colors.bg_hover = role_color(palette, patch.bg_hover);
    if (patch.has_bg_pressed) style.colors.bg_pressed = role_color(palette, patch.bg_pressed);
    if (patch.has_border_color) style.colors.border_color = role_color(palette, patch.border_color);
    if (patch.has_border_hover) style.colors.border_hover = role_color(palette, patch.border_hover);
    if (patch.has_border_pressed) style.colors.border_pressed = role_color(palette, patch.border_pressed);
    if (patch.has_font_color) style.colors.font_color = role_color(palette, patch.font_color);
    if (patch.has_accent_color) {
        const rgba accent = role_color(palette, patch.accent_color);
        style.colors.accent_color = accent;
        style.colors.accent_hover = role_color(palette, StyleRole::AccentHover);
        style.colors.accent_pressed = role_color(palette, StyleRole::AccentPressed);
        style.colors.accent_disabled = role_color(palette, StyleRole::AccentDisabled);
    }
    if (patch.has_on_accent) style.colors.on_accent = role_color(palette, patch.on_accent);
    if (patch.has_border_focus) style.colors.border_focus = role_color(palette, patch.border_focus);
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

    bool apply(WidgetKind kind, const StyleState& state, Style& style) const noexcept {
        const std::uint8_t mask = state_mask(state);
        bool matched = false;
        ensure_palette();
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
            matched = true;
            if (rule.kind == StyleRuleKind::Patch) {
                rule.patch.apply_to(style);
            } else {
                apply_role_patch(style, rule.role_patch, palette_);
            }
        }
        return matched;
    }

    bool apply(WidgetKind kind,
               const StyleState& state,
               Style& out,
               const Style& base) const noexcept {
        const std::uint8_t mask = state_mask(state);
        bool matched = false;
        ensure_palette();
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
            if (!matched) {
                out = base;
                matched = true;
            }
            if (rule.kind == StyleRuleKind::Patch) {
                rule.patch.apply_to(out);
            } else {
                apply_role_patch(out, rule.role_patch, palette_);
            }
        }
        return matched;
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

    void ensure_palette() const noexcept {
        const auto& tokens = Theme::instance().get_tokens();
        if (tokens.version == palette_version_) {
            return;
        }
        palette_ = build_palette(tokens);
        palette_version_ = tokens.version;
    }

    std::array<StyleRuleEntry, 32> rules_{};
    std::size_t count_{0};
    std::uint16_t order_{0};
    mutable RolePalette palette_{};
    mutable std::uint32_t palette_version_{std::numeric_limits<std::uint32_t>::max()};
};

export
inline bool apply_style_sheet(WidgetKind kind, const StyleState& state, Style& style) noexcept {
    return StyleSheet::instance().apply(kind, state, style);
}

export
inline const Style& resolve_style(WidgetKind kind,
                                  const StyleState& state,
                                  const Style& base,
                                  Style& scratch) noexcept {
    if (StyleSheet::instance().apply(kind, state, scratch, base)) {
        return scratch;
    }
    return base;
}
