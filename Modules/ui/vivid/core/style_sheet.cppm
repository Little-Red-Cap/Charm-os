module;
#include <array>
#include <cassert>
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

inline constexpr std::size_t kWidgetKindCount =
    static_cast<std::size_t>(WidgetKind::Histogram) + 1;
inline constexpr std::uint8_t kMaxStyleVariants = 4;
inline constexpr std::uint8_t kStyleStateCount = 16;

struct ResolvedColors {
    rgba bg{};
    rgba border{};
    rgba font{};
    rgba accent{};
    rgba on_accent{};
    rgba border_focus{};
};

struct StyleTable {
    std::array<ResolvedColors, kWidgetKindCount * kMaxStyleVariants * kStyleStateCount> colors{};
    std::array<std::uint8_t, kWidgetKindCount * kMaxStyleVariants * kStyleStateCount> matched{};
    std::array<std::uint8_t, kWidgetKindCount> kind_compiled{};
    std::uint32_t tokens_version{std::numeric_limits<std::uint32_t>::max()};
    std::uint32_t stylesheet_version{std::numeric_limits<std::uint32_t>::max()};
    bool valid{false};

    void reset() noexcept {
        colors.fill(ResolvedColors{});
        matched.fill(0);
        kind_compiled.fill(0);
        valid = false;
    }
};

export
struct ResolvedTheme {
    std::uint32_t version{std::numeric_limits<std::uint32_t>::max()};
    RolePalette role_palette{};
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

inline ResolvedTheme build_resolved_theme(const ThemeTokens& t) noexcept {
    ResolvedTheme r{};
    r.version = t.version;
    r.role_palette = build_palette(t);
    return r;
}

inline std::uint8_t clamp_variant(std::uint8_t variant) noexcept {
    return (variant < kMaxStyleVariants) ? variant : static_cast<std::uint8_t>(0);
}

inline std::uint8_t style_state_index(const StyleState& state) noexcept {
    std::uint8_t mask = 0;
    if (state.hovered) mask |= static_cast<std::uint8_t>(StyleStateFlag::Hovered);
    if (state.pressed) mask |= static_cast<std::uint8_t>(StyleStateFlag::Pressed);
    if (state.focused) mask |= static_cast<std::uint8_t>(StyleStateFlag::Focused);
    if (!state.enabled) mask |= static_cast<std::uint8_t>(StyleStateFlag::Disabled);
    return mask;
}

inline StyleState style_state_from_index(std::uint8_t mask, std::uint8_t variant) noexcept {
    const bool hovered = (mask & static_cast<std::uint8_t>(StyleStateFlag::Hovered)) != 0;
    const bool pressed = (mask & static_cast<std::uint8_t>(StyleStateFlag::Pressed)) != 0;
    const bool focused = (mask & static_cast<std::uint8_t>(StyleStateFlag::Focused)) != 0;
    const bool disabled = (mask & static_cast<std::uint8_t>(StyleStateFlag::Disabled)) != 0;
    return make_style_state(!disabled, hovered, pressed, focused, variant);
}

inline ResolvedColors build_resolved_colors(const Style& st, const StyleState& state) noexcept {
    rgba bg{};
    rgba border{};
    rgba font{};
    resolve_colors(st, state, bg, border, font);
    const rgba accent = resolve_accent(st, state);
    return ResolvedColors{bg, border, font, accent, st.colors.on_accent, st.colors.border_focus};
}

inline void apply_resolved_colors(Style& style, const ResolvedColors& colors) noexcept {
    style.colors.bg_color = colors.bg;
    style.colors.bg_hover = colors.bg;
    style.colors.bg_pressed = colors.bg;
    style.colors.bg_disabled = colors.bg;
    style.colors.border_color = colors.border;
    style.colors.border_hover = colors.border;
    style.colors.border_pressed = colors.border;
    style.colors.border_disabled = colors.border;
    style.colors.border_focus = colors.border_focus;
    style.colors.font_color = colors.font;
    style.colors.font_color_disabled = colors.font;
    style.colors.accent_color = colors.accent;
    style.colors.accent_hover = colors.accent;
    style.colors.accent_pressed = colors.accent;
    style.colors.accent_disabled = colors.accent;
    style.colors.on_accent = colors.on_accent;
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
        mark_stylesheet_dirty();
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
        mark_stylesheet_dirty();
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
        mark_stylesheet_dirty();
        return true;
    }

    bool apply(WidgetKind kind, const StyleState& state, Style& style) const noexcept {
        return apply_compiled(kind, state, style);
    }

    bool apply(WidgetKind kind,
               const StyleState& state,
               Style& out,
               const Style& base) const noexcept {
        return apply_compiled(kind, state, out, base);
    }

    void set_base_style(WidgetKind kind, const Style& style) noexcept {
        const auto idx = static_cast<std::size_t>(kind);
        if (idx >= kWidgetKindCount) return;
        base_styles_[idx] = style;
        base_style_set_[idx] = 1;
    }

    void notify_base_style_changed() noexcept {
        mark_stylesheet_dirty();
    }

    void rebuild_if_needed() noexcept {
        const auto& tokens = Theme::instance().get_tokens();
        if (tokens.version != resolved_.version) {
            resolved_ = build_resolved_theme(tokens);
#if defined(VIVID_SOA_TRACE_INPUT)
            role_palette_compile_count_ += 1u;
#endif
        }
        if (style_table_.valid &&
            style_table_.tokens_version == tokens.version &&
            style_table_.stylesheet_version == stylesheet_version_) {
            return;
        }
        rebuild_style_table();
        style_table_.tokens_version = tokens.version;
        style_table_.stylesheet_version = stylesheet_version_;
        style_table_.valid = true;
#if defined(VIVID_SOA_TRACE_INPUT)
        style_table_compile_count_ += 1u;
#endif
    }

#if defined(VIVID_SOA_TRACE_INPUT)
    void style_trace_reset() noexcept {
        role_palette_compile_count_ = 0;
        style_table_compile_count_ = 0;
    }

    std::uint32_t role_palette_compile_count() const noexcept {
        return role_palette_compile_count_;
    }

    std::uint32_t style_table_compile_count() const noexcept {
        return style_table_compile_count_;
    }
#endif

    std::uint32_t stylesheet_version() const noexcept {
        return stylesheet_version_;
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

    // Priority model (deterministic):
    // 1) kind specificity (None < concrete kind)
    // 2) variant specificity (Any < concrete variant)
    // 3) state specificity (more bits => more specific)
    // Tie-break by insertion order.
    static std::uint16_t rule_priority(const StyleSelector& sel) noexcept {
        const std::uint16_t kind_score = (sel.kind == WidgetKind::None) ? 0u : 1u;
        const std::uint16_t variant_score = (sel.variant == kStyleVariantAny) ? 0u : 1u;
        const std::uint16_t state_score = static_cast<std::uint16_t>(mask_weight(sel.require_mask));
        return static_cast<std::uint16_t>((kind_score << 8) | (variant_score << 7) | (state_score << 4));
    }

    void insert_rule(const StyleRuleEntry& entry) noexcept {
        // Keep rules ordered by priority; tie-break by insertion order (older first).
        std::size_t pos = count_;
        for (std::size_t i = 0; i < count_; ++i) {
            const auto& cur = rules_[i];
            if (entry.priority < cur.priority ||
                (entry.priority == cur.priority && entry.order < cur.order)) {
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

    void mark_stylesheet_dirty() noexcept {
        stylesheet_version_ += 1u;
        style_table_.valid = false;
    }

    void rebuild_style_table() noexcept {
        style_table_.reset();
        std::array<std::uint8_t, kWidgetKindCount> kind_used{};
        bool has_global_rule = false;
        for (std::size_t i = 0; i < count_; ++i) {
            const auto& rule = rules_[i];
            if (rule.selector.kind == WidgetKind::None) {
                has_global_rule = true;
                continue;
            }
            const auto kind_idx = static_cast<std::size_t>(rule.selector.kind);
            if (kind_idx < kWidgetKindCount) {
                kind_used[kind_idx] = 1;
            }
        }
        if (has_global_rule) {
#ifndef NDEBUG
            assert(false && "StyleSheet compile does not support global rules yet");
#endif
        }
        for (std::size_t kind_idx = 0; kind_idx < kWidgetKindCount; ++kind_idx) {
            if (!has_global_rule && kind_used[kind_idx] == 0) {
                continue;
            }
            style_table_.kind_compiled[kind_idx] = static_cast<std::uint8_t>(1);
            Style base = base_styles_[kind_idx];
#ifndef NDEBUG
            if (base_style_set_[kind_idx] == 0) {
                assert(false && "StyleSheet base style missing");
            }
#endif
            for (std::uint8_t variant = 0; variant < kMaxStyleVariants; ++variant) {
                for (std::uint8_t state_idx = 0; state_idx < kStyleStateCount; ++state_idx) {
                    const StyleState state = style_state_from_index(state_idx, variant);
                    Style scratch{};
                    bool matched = false;
                    for (std::size_t r = 0; r < count_; ++r) {
                        const auto& rule = rules_[r];
                        if (rule.selector.kind != WidgetKind::None &&
                            rule.selector.kind != static_cast<WidgetKind>(kind_idx)) {
                            continue;
                        }
                        if (rule.selector.variant != kStyleVariantAny &&
                            rule.selector.variant >= kMaxStyleVariants) {
#ifndef NDEBUG
                            assert(false && "StyleSheet variant out of range");
#endif
                            continue;
                        }
                        if (rule.selector.variant != kStyleVariantAny &&
                            rule.selector.variant != variant) {
                            continue;
                        }
                        const std::uint8_t mask = state_mask(state);
                        if ((mask & rule.selector.require_mask) != rule.selector.require_mask) {
                            continue;
                        }
                        if (!matched) {
                            scratch = base;
                            matched = true;
                        }
                        if (rule.kind == StyleRuleKind::Patch) {
                            rule.patch.apply_to(scratch);
                        } else {
                            apply_role_patch(scratch, rule.role_patch, resolved_.role_palette);
                        }
                    }
                    const ResolvedColors colors = build_resolved_colors(matched ? scratch : base, state);
                    const std::size_t entry =
                        ((kind_idx * kMaxStyleVariants + variant) * kStyleStateCount + state_idx);
                    style_table_.colors[entry] = colors;
                    style_table_.matched[entry] = matched
                        ? static_cast<std::uint8_t>(1)
                        : static_cast<std::uint8_t>(0);
                }
            }
        }
    }

    bool apply_compiled(WidgetKind kind, const StyleState& state, Style& style) const noexcept {
        if (!style_table_.valid) {
#ifndef NDEBUG
            assert(false && "StyleSheet compiled table is not ready");
#endif
            return false;
        }
        const auto& tokens = Theme::instance().get_tokens();
        if (style_table_.tokens_version != tokens.version ||
            style_table_.stylesheet_version != stylesheet_version_) {
#ifndef NDEBUG
            assert(false && "StyleSheet compiled table out of date");
#endif
            return false;
        }
        const auto kind_idx = static_cast<std::size_t>(kind);
        if (kind_idx >= kWidgetKindCount) return false;
        if (style_table_.kind_compiled[kind_idx] == 0) return false;
        const std::uint8_t variant = clamp_variant(state.variant);
        const std::uint8_t state_idx = style_state_index(state);
        const std::size_t entry =
            ((kind_idx * kMaxStyleVariants + variant) * kStyleStateCount + state_idx);
        if (style_table_.matched[entry] == 0) return false;
        apply_resolved_colors(style, style_table_.colors[entry]);
        return true;
    }

    bool apply_compiled(WidgetKind kind,
                        const StyleState& state,
                        Style& out,
                        const Style& base) const noexcept {
        if (!style_table_.valid) {
#ifndef NDEBUG
            assert(false && "StyleSheet compiled table is not ready");
#endif
            return false;
        }
        const auto& tokens = Theme::instance().get_tokens();
        if (style_table_.tokens_version != tokens.version ||
            style_table_.stylesheet_version != stylesheet_version_) {
#ifndef NDEBUG
            assert(false && "StyleSheet compiled table out of date");
#endif
            return false;
        }
        const auto kind_idx = static_cast<std::size_t>(kind);
        if (kind_idx >= kWidgetKindCount) return false;
        if (style_table_.kind_compiled[kind_idx] == 0) return false;
        const std::uint8_t variant = clamp_variant(state.variant);
        const std::uint8_t state_idx = style_state_index(state);
        const std::size_t entry =
            ((kind_idx * kMaxStyleVariants + variant) * kStyleStateCount + state_idx);
        if (style_table_.matched[entry] == 0) return false;
        out = base;
        apply_resolved_colors(out, style_table_.colors[entry]);
        return true;
    }

    std::array<StyleRuleEntry, 32> rules_{};
    std::size_t count_{0};
    std::uint16_t order_{0};
    mutable ResolvedTheme resolved_{};
    std::array<Style, kWidgetKindCount> base_styles_{};
    std::array<std::uint8_t, kWidgetKindCount> base_style_set_{};
    std::uint32_t stylesheet_version_{0};
    mutable StyleTable style_table_{};
#if defined(VIVID_SOA_TRACE_INPUT)
    std::uint32_t role_palette_compile_count_{0};
    std::uint32_t style_table_compile_count_{0};
#endif
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
