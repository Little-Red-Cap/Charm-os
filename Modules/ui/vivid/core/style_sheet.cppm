module;
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
export module charm.core.style_sheet;

export import charm.core.style;
export import charm.core.handle;
export import charm.core.widget_registry;

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

inline constexpr std::size_t kWidgetKindCount = enabled_widget_kind_count;
inline constexpr std::uint8_t kInvalidKindIndex = invalid_widget_kind_index;
inline constexpr std::uint8_t kMaxStyleVariants = 4;
inline constexpr std::uint8_t kMaxStyleStateBits = 4;
inline constexpr std::uint8_t kMaxStyleStateCount = static_cast<std::uint8_t>(1u << kMaxStyleStateBits);
inline constexpr std::uint8_t kMaxMetricsPool = 64;
inline constexpr std::uint8_t kStyleStateMaskAll =
    static_cast<std::uint8_t>(StyleStateFlag::Hovered)
    | static_cast<std::uint8_t>(StyleStateFlag::Pressed)
    | static_cast<std::uint8_t>(StyleStateFlag::Focused)
    | static_cast<std::uint8_t>(StyleStateFlag::Disabled);
inline constexpr std::uint8_t kStyleStateMaskDefault =
    static_cast<std::uint8_t>(StyleStateFlag::Hovered)
    | static_cast<std::uint8_t>(StyleStateFlag::Pressed)
    | static_cast<std::uint8_t>(StyleStateFlag::Disabled);

constexpr std::uint8_t variant_count_for_kind(WidgetKind) noexcept {
    return 1;
}

constexpr std::array<std::uint8_t, kWidgetKindCount> build_kind_variant_counts() noexcept {
    std::array<std::uint8_t, kWidgetKindCount> counts{};
    for (std::size_t i = 0; i < kWidgetKindCount; ++i) {
        counts[i] = variant_count_for_kind(enabled_widget_kinds[i]);
        if (counts[i] == 0) counts[i] = 1;
        if (counts[i] > kMaxStyleVariants) counts[i] = kMaxStyleVariants;
    }
    return counts;
}

constexpr std::array<std::uint16_t, kWidgetKindCount> build_kind_variant_offsets(
    const std::array<std::uint8_t, kWidgetKindCount>& counts) noexcept {
    std::array<std::uint16_t, kWidgetKindCount> offsets{};
    std::uint16_t sum = 0;
    for (std::size_t i = 0; i < kWidgetKindCount; ++i) {
        offsets[i] = sum;
        sum = static_cast<std::uint16_t>(sum + counts[i]);
    }
    return offsets;
}

constexpr std::size_t count_total_variant_slots(
    const std::array<std::uint8_t, kWidgetKindCount>& counts) noexcept {
    std::size_t sum = 0;
    for (std::uint8_t count : counts) {
        sum += count;
    }
    return sum;
}

inline constexpr auto kKindVariantCounts = build_kind_variant_counts();
inline constexpr auto kKindVariantOffsets = build_kind_variant_offsets(kKindVariantCounts);
inline constexpr std::size_t kTotalVariantSlots = count_total_variant_slots(kKindVariantCounts);

constexpr std::uint8_t style_state_mask_for_kind(WidgetKind kind) noexcept {
    const std::uint8_t interactive = kStyleStateMaskDefault;
    const std::uint8_t readonly = static_cast<std::uint8_t>(StyleStateFlag::Disabled);
    switch (kind) {
        case WidgetKind::None:
        case WidgetKind::Container:
        case WidgetKind::Dial:
        case WidgetKind::Arc:
        case WidgetKind::Image:
        case WidgetKind::Label:
        case WidgetKind::Led:
        case WidgetKind::Progress:
        case WidgetKind::ModalDialog:
        case WidgetKind::ProgressBarSimple:
        case WidgetKind::DynamicNebula:
        case WidgetKind::CrtScreen:
        case WidgetKind::Bar:
        case WidgetKind::PopupLayer:
        case WidgetKind::MessageBox:
        case WidgetKind::RadioGroup:
        case WidgetKind::Chart:
        case WidgetKind::Waveform:
        case WidgetKind::Gauge:
        case WidgetKind::PrimitivesCanvas:
        case WidgetKind::PerfOverlay:
        case WidgetKind::Timeline:
        case WidgetKind::RichText:
        case WidgetKind::CodeBlock:
        case WidgetKind::ProgressWheel:
        case WidgetKind::WaveformView:
        case WidgetKind::BatteryGauge:
        case WidgetKind::HistogramView:
        case WidgetKind::RingIndication:
        case WidgetKind::TextBox:
        case WidgetKind::FoldablePanel:
        case WidgetKind::ProgressFlowing:
        case WidgetKind::CloudyGlass:
        case WidgetKind::ProgressBarRound:
        case WidgetKind::SpinningWheel:
        case WidgetKind::ImageBox:
        case WidgetKind::MeterPointer:
        case WidgetKind::ProgressBarDrill:
        case WidgetKind::SpectrumView:
        case WidgetKind::BusyWheel:
        case WidgetKind::ConsoleBox:
        case WidgetKind::BatteryGasGauge:
        case WidgetKind::Histogram:
            return readonly;
        case WidgetKind::Button:
        case WidgetKind::Checkbox:
        case WidgetKind::Radio:
        case WidgetKind::Switch:
        case WidgetKind::Slider:
        case WidgetKind::ScrollBar:
        case WidgetKind::SegmentedControl:
        case WidgetKind::Dropdown:
        case WidgetKind::TabView:
        case WidgetKind::Stepper:
        case WidgetKind::Menu:
        case WidgetKind::MenuItem:
        case WidgetKind::TextInput:
        case WidgetKind::TextArea:
        case WidgetKind::NumberInput:
        case WidgetKind::ToggleGroup:
        case WidgetKind::ListItem:
        case WidgetKind::List:
        case WidgetKind::ListView:
        case WidgetKind::IconList:
        case WidgetKind::TextTrackingList:
        case WidgetKind::TextList:
        case WidgetKind::TableView:
        case WidgetKind::TreeView:
        case WidgetKind::ScrollContainer:
        case WidgetKind::Roller:
        case WidgetKind::Spinner:
        case WidgetKind::NumberList:
        case WidgetKind::SpinZoomWidget:
            return interactive;
    }
    return readonly;
}

constexpr std::array<std::uint8_t, kWidgetKindCount> build_kind_state_masks() noexcept {
    std::array<std::uint8_t, kWidgetKindCount> masks{};
    for (std::size_t i = 0; i < kWidgetKindCount; ++i) {
        const std::uint8_t mask = style_state_mask_for_kind(enabled_widget_kinds[i]) & kStyleStateMaskAll;
        masks[i] = mask;
    }
    return masks;
}

constexpr std::uint8_t popcount4(std::uint8_t mask) noexcept {
    mask = static_cast<std::uint8_t>(mask & 0x0F);
    return static_cast<std::uint8_t>(((mask >> 0) & 1u)
        + ((mask >> 1) & 1u)
        + ((mask >> 2) & 1u)
        + ((mask >> 3) & 1u));
}

constexpr std::array<std::uint8_t, kWidgetKindCount> build_kind_state_counts(
    const std::array<std::uint8_t, kWidgetKindCount>& masks) noexcept {
    std::array<std::uint8_t, kWidgetKindCount> counts{};
    for (std::size_t i = 0; i < kWidgetKindCount; ++i) {
        const std::uint8_t bits = popcount4(masks[i]);
        counts[i] = static_cast<std::uint8_t>(1u << bits);
        if (counts[i] == 0) counts[i] = 1;
    }
    return counts;
}

constexpr std::array<std::uint16_t, kWidgetKindCount> build_kind_state_offsets(
    const std::array<std::uint8_t, kWidgetKindCount>& variant_counts,
    const std::array<std::uint8_t, kWidgetKindCount>& state_counts) noexcept {
    std::array<std::uint16_t, kWidgetKindCount> offsets{};
    std::uint16_t sum = 0;
    for (std::size_t i = 0; i < kWidgetKindCount; ++i) {
        offsets[i] = sum;
        sum = static_cast<std::uint16_t>(sum + variant_counts[i] * state_counts[i]);
    }
    return offsets;
}

constexpr std::size_t count_total_style_slots(
    const std::array<std::uint8_t, kWidgetKindCount>& variant_counts,
    const std::array<std::uint8_t, kWidgetKindCount>& state_counts) noexcept {
    std::size_t sum = 0;
    for (std::size_t i = 0; i < kWidgetKindCount; ++i) {
        sum += static_cast<std::size_t>(variant_counts[i]) * state_counts[i];
    }
    return sum;
}

inline constexpr auto kKindStateMasks = build_kind_state_masks();
inline constexpr auto kKindStateCounts = build_kind_state_counts(kKindStateMasks);
inline constexpr auto kKindStateOffsets = build_kind_state_offsets(kKindVariantCounts, kKindStateCounts);
inline constexpr std::size_t kTotalStyleSlots = count_total_style_slots(kKindVariantCounts, kKindStateCounts);

export
struct ResolvedColors {
    rgba bg{};
    rgba border{};
    rgba font{};
    rgba accent{};
    rgba on_accent{};
    rgba border_focus{};
};

export
struct ResolvedMetrics {
    const Font* font{nullptr};
    std::int16_t border_width{0};
    std::int16_t corner_radius{0};
    std::int16_t padding{0};
    std::int16_t header_padding{0};
    std::int16_t content_padding{0};
    std::int16_t scrollbar_margin{0};
    std::int16_t scrollbar_thumb_min{0};
};

export
struct ResolvedStyleView {
    const ResolvedColors* colors{nullptr};
    const ResolvedMetrics* metrics{nullptr};
};

#if defined(VIVID_SOA_TRACE_INPUT)
export
struct StyleStats {
    std::size_t style_colors_bytes{0};
    std::size_t style_metrics_id_bytes{0};
    std::size_t metrics_pool_bytes{0};
    std::size_t style_table_total_bytes{0};
    std::uint32_t metrics_pool_size{0};
    std::uint32_t style_lookup_count{0};
    std::uint32_t theme_recompile_count{0};
    bool metrics_overflowed{false};
};
#endif

static_assert(std::is_trivially_copyable_v<ResolvedColors>);
static_assert(std::is_trivially_copyable_v<ResolvedMetrics>);
static_assert(std::is_trivially_copyable_v<ResolvedStyleView>);
static_assert(sizeof(ResolvedColors) <= 32);
static_assert(sizeof(ResolvedMetrics) <= 32);
static_assert(sizeof(ResolvedStyleView) <= 24);
static_assert(kMaxStyleStateBits <= 6);

struct StyleTable {
    std::array<ResolvedColors, kTotalStyleSlots> colors{};
    std::array<std::uint8_t, kTotalStyleSlots> matched{};
    std::array<std::uint8_t, kWidgetKindCount> kind_compiled{};
    std::array<ResolvedMetrics, kMaxMetricsPool> metrics_pool{};
    std::array<std::uint8_t, kTotalVariantSlots> metrics_id{};
    std::uint8_t metrics_count{0};
    bool metrics_overflowed{false};
    std::uint32_t tokens_version{std::numeric_limits<std::uint32_t>::max()};
    std::uint32_t stylesheet_version{std::numeric_limits<std::uint32_t>::max()};
    bool valid{false};

    void reset() noexcept {
        colors.fill(ResolvedColors{});
        matched.fill(0);
        kind_compiled.fill(0);
        metrics_pool.fill(ResolvedMetrics{});
        metrics_id.fill(0);
        metrics_count = 0;
        metrics_overflowed = false;
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

inline std::uint8_t clamp_variant(std::uint8_t variant, std::uint8_t variant_count) noexcept {
    if (variant_count == 0) return 0;
    if (variant >= variant_count) return 0;
    return variant;
}

inline std::uint8_t compress_state_mask(std::uint8_t masked, std::uint8_t mask) noexcept {
    std::uint8_t idx = 0;
    std::uint8_t bit = 0;
    const std::uint8_t hovered = static_cast<std::uint8_t>(StyleStateFlag::Hovered);
    const std::uint8_t pressed = static_cast<std::uint8_t>(StyleStateFlag::Pressed);
    const std::uint8_t focused = static_cast<std::uint8_t>(StyleStateFlag::Focused);
    const std::uint8_t disabled = static_cast<std::uint8_t>(StyleStateFlag::Disabled);
    if ((mask & hovered) != 0) {
        if ((masked & hovered) != 0) idx = static_cast<std::uint8_t>(idx | (1u << bit));
        ++bit;
    }
    if ((mask & pressed) != 0) {
        if ((masked & pressed) != 0) idx = static_cast<std::uint8_t>(idx | (1u << bit));
        ++bit;
    }
    if ((mask & focused) != 0) {
        if ((masked & focused) != 0) idx = static_cast<std::uint8_t>(idx | (1u << bit));
        ++bit;
    }
    if ((mask & disabled) != 0) {
        if ((masked & disabled) != 0) idx = static_cast<std::uint8_t>(idx | (1u << bit));
    }
    return idx;
}

inline std::uint8_t style_state_index(const StyleState& state, std::uint8_t mask) noexcept {
    std::uint8_t raw = 0;
    if (state.hovered) raw |= static_cast<std::uint8_t>(StyleStateFlag::Hovered);
    if (state.pressed) raw |= static_cast<std::uint8_t>(StyleStateFlag::Pressed);
    if (state.focused) raw |= static_cast<std::uint8_t>(StyleStateFlag::Focused);
    if (!state.enabled) raw |= static_cast<std::uint8_t>(StyleStateFlag::Disabled);
    const std::uint8_t masked = static_cast<std::uint8_t>(raw & mask);
    return compress_state_mask(masked, mask);
}

inline StyleState style_state_from_index(std::uint8_t idx,
                                         std::uint8_t variant,
                                         std::uint8_t mask) noexcept {
    std::uint8_t bit = 0;
    bool hovered = false;
    bool pressed = false;
    bool focused = false;
    bool disabled = false;
    const std::uint8_t hovered_bit = static_cast<std::uint8_t>(StyleStateFlag::Hovered);
    const std::uint8_t pressed_bit = static_cast<std::uint8_t>(StyleStateFlag::Pressed);
    const std::uint8_t focused_bit = static_cast<std::uint8_t>(StyleStateFlag::Focused);
    const std::uint8_t disabled_bit = static_cast<std::uint8_t>(StyleStateFlag::Disabled);
    if ((mask & hovered_bit) != 0) {
        hovered = ((idx >> bit) & 1u) != 0;
        ++bit;
    }
    if ((mask & pressed_bit) != 0) {
        pressed = ((idx >> bit) & 1u) != 0;
        ++bit;
    }
    if ((mask & focused_bit) != 0) {
        focused = ((idx >> bit) & 1u) != 0;
        ++bit;
    }
    if ((mask & disabled_bit) != 0) {
        disabled = ((idx >> bit) & 1u) != 0;
    }
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

inline std::int16_t clamp_i16(int v) noexcept {
    if (v > std::numeric_limits<std::int16_t>::max()) return std::numeric_limits<std::int16_t>::max();
    if (v < std::numeric_limits<std::int16_t>::min()) return std::numeric_limits<std::int16_t>::min();
    return static_cast<std::int16_t>(v);
}

inline ResolvedMetrics build_resolved_metrics(const Style& st) noexcept {
    ResolvedMetrics m{};
    m.font = st.font;
    m.border_width = clamp_i16(st.metrics.border_width);
    m.corner_radius = clamp_i16(st.metrics.corner_radius);
    m.padding = clamp_i16(st.metrics.padding);
    m.header_padding = clamp_i16(st.metrics.header_padding);
    m.content_padding = clamp_i16(st.metrics.content_padding);
    m.scrollbar_margin = clamp_i16(st.metrics.scrollbar_margin);
    m.scrollbar_thumb_min = clamp_i16(st.metrics.scrollbar_thumb_min);
    return m;
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

inline bool patch_has_metrics(const StylePatch& patch) noexcept {
    return patch.has_border_width ||
        patch.has_corner_radius ||
        patch.has_padding ||
        patch.has_header_padding ||
        patch.has_content_padding ||
        patch.has_scrollbar_margin ||
        patch.has_scrollbar_thumb_min ||
        patch.has_glass_highlight_pos ||
        patch.has_glass_highlight_alpha ||
        patch.has_glass_shadow_alpha ||
        patch.has_glass_opacity_min ||
        patch.has_glass_opacity_max ||
        patch.has_font;
}

inline bool metrics_equal(const ResolvedMetrics& a, const ResolvedMetrics& b) noexcept {
    return a.font == b.font &&
        a.border_width == b.border_width &&
        a.corner_radius == b.corner_radius &&
        a.padding == b.padding &&
        a.header_padding == b.header_padding &&
        a.content_padding == b.content_padding &&
        a.scrollbar_margin == b.scrollbar_margin &&
        a.scrollbar_thumb_min == b.scrollbar_thumb_min;
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

    ResolvedStyleView lookup(WidgetKind kind, const StyleState& state) const noexcept {
#if defined(VIVID_SOA_TRACE_INPUT)
        style_lookup_count_ += 1u;
#endif
        if (!style_table_.valid) {
#ifndef NDEBUG
            assert(false && "StyleSheet compiled table is not ready");
#endif
            return ResolvedStyleView{&fallback_colors_, &fallback_metrics_};
        }
        const auto& tokens = Theme::instance().get_tokens();
        if (style_table_.tokens_version != tokens.version ||
            style_table_.stylesheet_version != stylesheet_version_) {
#ifndef NDEBUG
            assert(false && "StyleSheet compiled table out of date");
#endif
            return ResolvedStyleView{&fallback_colors_, &fallback_metrics_};
        }
        const auto kind_idx = widget_kind_index[static_cast<std::size_t>(kind)];
        if (kind_idx == kInvalidKindIndex || style_table_.kind_compiled[kind_idx] == 0) {
            return ResolvedStyleView{&fallback_colors_, &fallback_metrics_};
        }
        const std::uint8_t variant = clamp_variant(state.variant, kKindVariantCounts[kind_idx]);
        const std::uint8_t state_count = kKindStateCounts[kind_idx];
        const std::uint8_t state_idx = style_state_index(state, kKindStateMasks[kind_idx]);
#ifndef NDEBUG
        if (state_idx >= state_count) assert(false && "StyleSheet state index out of range");
#endif
        const std::size_t entry =
            static_cast<std::size_t>(kKindStateOffsets[kind_idx]) + variant * state_count + state_idx;
        const std::size_t color_entry = entry;
        const std::size_t metrics_entry =
            static_cast<std::size_t>(kKindVariantOffsets[kind_idx]) + variant;
        return ResolvedStyleView{
            &style_table_.colors[color_entry],
            &style_table_.metrics_pool[style_table_.metrics_id[metrics_entry]]
        };
    }

    void set_base_style(WidgetKind kind, const Style& style) noexcept {
        const auto idx = widget_kind_index[static_cast<std::size_t>(kind)];
        if (idx == kInvalidKindIndex) return;
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

    StyleStats style_stats() const noexcept {
        StyleStats s{};
        s.style_colors_bytes = style_table_.colors.size() * sizeof(ResolvedColors);
        s.style_metrics_id_bytes = style_table_.metrics_id.size() * sizeof(std::uint8_t);
        s.metrics_pool_bytes = static_cast<std::size_t>(style_table_.metrics_count) * sizeof(ResolvedMetrics);
        s.style_table_total_bytes = s.style_colors_bytes + s.style_metrics_id_bytes + s.metrics_pool_bytes;
        s.metrics_pool_size = style_table_.metrics_count;
        s.style_lookup_count = style_lookup_count_;
        s.theme_recompile_count = style_table_compile_count_;
        s.metrics_overflowed = style_table_.metrics_overflowed;
        return s;
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
        bool has_global_rule = false;
        for (std::size_t i = 0; i < count_; ++i) {
            const auto& rule = rules_[i];
            if (rule.selector.kind == WidgetKind::None) {
                has_global_rule = true;
            }
        }
        if (has_global_rule) {
#ifndef NDEBUG
            assert(false && "StyleSheet compile does not support global rules yet");
#endif
        }
        for (std::size_t kind_idx = 0; kind_idx < kWidgetKindCount; ++kind_idx) {
            if (base_style_set_[kind_idx] == 0) continue;
            const WidgetKind kind = enabled_widget_kinds[kind_idx];
            style_table_.kind_compiled[kind_idx] = static_cast<std::uint8_t>(1);
            Style base = base_styles_[kind_idx];
#ifndef NDEBUG
            if (base_style_set_[kind_idx] == 0) assert(false && "StyleSheet base style missing");
#endif
            const std::uint8_t variant_count = kKindVariantCounts[kind_idx];
            const std::uint8_t state_mask_bits = kKindStateMasks[kind_idx];
            const std::uint8_t state_count = kKindStateCounts[kind_idx];
            for (std::uint8_t variant = 0; variant < variant_count; ++variant) {
                const std::size_t metrics_entry =
                    static_cast<std::size_t>(kKindVariantOffsets[kind_idx]) + variant;
                const ResolvedMetrics metrics = build_resolved_metrics(base);
                std::uint8_t metrics_slot = 0;
                bool found = false;
                for (std::uint8_t i = 0; i < style_table_.metrics_count; ++i) {
                    if (metrics_equal(style_table_.metrics_pool[i], metrics)) {
                        metrics_slot = i;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    if (style_table_.metrics_count < kMaxMetricsPool) {
                        metrics_slot = style_table_.metrics_count;
                        style_table_.metrics_pool[metrics_slot] = metrics;
                        style_table_.metrics_count = static_cast<std::uint8_t>(style_table_.metrics_count + 1);
                    } else {
#ifndef NDEBUG
                        assert(false && "StyleSheet metrics pool overflow");
#endif
                        style_table_.metrics_overflowed = true;
                        metrics_slot = 0;
                    }
                }
                style_table_.metrics_id[metrics_entry] = metrics_slot;
                for (std::uint8_t state_idx = 0; state_idx < state_count; ++state_idx) {
                    const StyleState state = style_state_from_index(state_idx, variant, state_mask_bits);
                    Style scratch{};
                    bool matched = false;
                    for (std::size_t r = 0; r < count_; ++r) {
                        const auto& rule = rules_[r];
                        if (rule.selector.kind != WidgetKind::None &&
                            rule.selector.kind != kind) {
                            continue;
                        }
                        if (rule.kind == StyleRuleKind::Patch && patch_has_metrics(rule.patch)) {
#ifndef NDEBUG
                            assert(false && "StyleSheet metrics patch not supported in compiled table");
#endif
                        }
                        if (rule.selector.variant != kStyleVariantAny &&
                            rule.selector.variant >= variant_count) {
#ifndef NDEBUG
                            assert(false && "StyleSheet variant out of range");
#endif
                            continue;
                        }
                        if (rule.selector.variant != kStyleVariantAny &&
                            rule.selector.variant != variant) {
                            continue;
                        }
                        if ((rule.selector.require_mask & ~state_mask_bits) != 0) {
#ifndef NDEBUG
                            assert(false && "StyleSheet require_mask outside kind state mask");
#endif
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
                        static_cast<std::size_t>(kKindStateOffsets[kind_idx]) + variant * state_count + state_idx;
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
        const auto kind_idx = widget_kind_index[static_cast<std::size_t>(kind)];
        if (kind_idx == kInvalidKindIndex) return false;
        if (style_table_.kind_compiled[kind_idx] == 0) return false;
        const std::uint8_t variant = clamp_variant(state.variant, kKindVariantCounts[kind_idx]);
        const std::uint8_t state_count = kKindStateCounts[kind_idx];
        const std::uint8_t state_idx = style_state_index(state, kKindStateMasks[kind_idx]);
#ifndef NDEBUG
        if (state_idx >= state_count) assert(false && "StyleSheet state index out of range");
#endif
        const std::size_t entry =
            static_cast<std::size_t>(kKindStateOffsets[kind_idx]) + variant * state_count + state_idx;
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
        const auto kind_idx = widget_kind_index[static_cast<std::size_t>(kind)];
        if (kind_idx == kInvalidKindIndex) return false;
        if (style_table_.kind_compiled[kind_idx] == 0) return false;
        const std::uint8_t variant = clamp_variant(state.variant, kKindVariantCounts[kind_idx]);
        const std::uint8_t state_count = kKindStateCounts[kind_idx];
        const std::uint8_t state_idx = style_state_index(state, kKindStateMasks[kind_idx]);
#ifndef NDEBUG
        if (state_idx >= state_count) assert(false && "StyleSheet state index out of range");
#endif
        const std::size_t entry =
            static_cast<std::size_t>(kKindStateOffsets[kind_idx]) + variant * state_count + state_idx;
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
    ResolvedColors fallback_colors_{};
    ResolvedMetrics fallback_metrics_{};
#if defined(VIVID_SOA_TRACE_INPUT)
    std::uint32_t role_palette_compile_count_{0};
    std::uint32_t style_table_compile_count_{0};
    std::uint32_t style_lookup_count_{0};
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
