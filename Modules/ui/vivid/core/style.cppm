module;
#include <array>
#include <cstdint>
#include <limits>
#include <type_traits>
export module charm.core.style;

export import charm.core.config;
export import charm.gfx.color;
export import charm.font.typography;

export
struct StyleColors {
    rgba bg_color{240, 240, 240, 255};
    rgba border_color{180, 180, 180, 255};
    rgba font_color{0, 0, 0, 255};

    rgba bg_hover{230, 230, 230, 255};
    rgba bg_pressed{210, 210, 210, 255};
    rgba bg_disabled{220, 220, 220, 255};
    rgba border_hover{120, 120, 120, 255};
    rgba border_pressed{80, 80, 80, 255};
    rgba border_disabled{160, 160, 160, 255};
    rgba border_focus{80, 120, 200, 255};
    rgba font_color_disabled{120, 120, 120, 255};

    rgba accent_color{80, 120, 200, 255};
    rgba accent_hover{0, 0, 0, 0}; // 0 alpha = derive from accent_color
    rgba accent_pressed{0, 0, 0, 0}; // 0 alpha = derive from accent_color
    rgba accent_disabled{0, 0, 0, 0}; // 0 alpha = derive from accent_color
    rgba on_accent{255, 255, 255, 255};
};

export
struct StyleMetrics {
    int border_width{1};
    int corner_radius{4};
    int padding{4};
    int header_padding{4};
    int content_padding{8};
    int scrollbar_margin{2};
    int scrollbar_thumb_min{12};
    int glass_highlight_pos{10};
    int glass_highlight_alpha{70};
    int glass_shadow_alpha{40};
    int glass_opacity_min{40};
    int glass_opacity_max{200};
};

export
struct StyleDecoration {
    bool shadow_enabled{false};
    rgba shadow_color{0, 0, 0, 0};
    std::int16_t shadow_offset_x{0};
    std::int16_t shadow_offset_y{0};
    std::int16_t shadow_spread{0};
    std::int16_t shadow_radius{0};

    bool inner_stroke_enabled{false};
    rgba inner_stroke_color{0, 0, 0, 0};
    std::int16_t inner_stroke_width{0};

    bool outline_enabled{false};
    rgba outline_color{0, 0, 0, 0};
    std::int16_t outline_width{0};

    bool gradient_enabled{false};
    rgba gradient_start{0, 0, 0, 0};
    rgba gradient_end{0, 0, 0, 0};
    std::uint8_t gradient_direction{0};
};

export
struct Style {
    StyleColors colors{};
    StyleMetrics metrics{};
    StyleDecoration decoration{};
    const Font* font{nullptr};
    FontId font_role{FontId::Normal};
    FontWeight font_weight{FontWeight::Regular};
};

export
struct ThemeTokens {
    std::uint32_t version{0};
    rgba surface{240, 240, 240, 255};
    rgba surface_variant{255, 255, 255, 255};
    rgba on_surface{0, 0, 0, 255};
    rgba on_surface_muted{120, 120, 120, 255};
    rgba outline{180, 180, 180, 255};
    rgba accent{80, 120, 200, 255};
    rgba on_accent{255, 255, 255, 255};
    rgba danger{200, 60, 60, 255};
    rgba on_danger{255, 255, 255, 255};
    rgba focus_ring{80, 120, 200, 255};
};

export rgba adjust_by_luma(const rgba& c, int delta) noexcept;

export
struct StylePatch {
    // Every SoA node reserves a StylePatch, so presence flags must stay packed.
    bool has_bg_color : 1 {false};
    bool has_border_color : 1 {false};
    bool has_border_width : 1 {false};
    bool has_corner_radius : 1 {false};
    bool has_padding : 1 {false};
    bool has_header_padding : 1 {false};
    bool has_content_padding : 1 {false};
    bool has_scrollbar_margin : 1 {false};
    bool has_scrollbar_thumb_min : 1 {false};
    bool has_glass_highlight_pos : 1 {false};
    bool has_glass_highlight_alpha : 1 {false};
    bool has_glass_shadow_alpha : 1 {false};
    bool has_glass_opacity_min : 1 {false};
    bool has_glass_opacity_max : 1 {false};
    bool has_font : 1 {false};
    bool has_font_role : 1 {false};
    bool has_font_weight : 1 {false};
    bool has_font_color : 1 {false};
    bool has_bg_hover : 1 {false};
    bool has_bg_pressed : 1 {false};
    bool has_bg_disabled : 1 {false};
    bool has_border_hover : 1 {false};
    bool has_border_pressed : 1 {false};
    bool has_border_disabled : 1 {false};
    bool has_border_focus : 1 {false};
    bool has_font_color_disabled : 1 {false};
    bool has_accent_color : 1 {false};
    bool has_accent_hover : 1 {false};
    bool has_accent_pressed : 1 {false};
    bool has_accent_disabled : 1 {false};
    bool has_on_accent : 1 {false};
    bool has_shadow_enabled : 1 {false};
    bool has_shadow_color : 1 {false};
    bool has_shadow_offset_x : 1 {false};
    bool has_shadow_offset_y : 1 {false};
    bool has_shadow_spread : 1 {false};
    bool has_shadow_radius : 1 {false};
    bool has_inner_stroke_enabled : 1 {false};
    bool has_inner_stroke_color : 1 {false};
    bool has_inner_stroke_width : 1 {false};
    bool has_outline_enabled : 1 {false};
    bool has_outline_color : 1 {false};
    bool has_outline_width : 1 {false};
    bool has_gradient_enabled : 1 {false};
    bool has_gradient_start : 1 {false};
    bool has_gradient_end : 1 {false};
    bool has_gradient_direction : 1 {false};

    rgba bg_color{};
    rgba border_color{};
    int  border_width{0};
    int  corner_radius{0};
    int  padding{0};
    int  header_padding{0};
    int  content_padding{0};
    int  scrollbar_margin{0};
    int  scrollbar_thumb_min{0};
    int  glass_highlight_pos{0};
    int  glass_highlight_alpha{0};
    int  glass_shadow_alpha{0};
    int  glass_opacity_min{0};
    int  glass_opacity_max{0};
    const Font* font{nullptr};
    FontId font_role{FontId::Normal};
    FontWeight font_weight{FontWeight::Regular};
    rgba font_color{};
    rgba bg_hover{};
    rgba bg_pressed{};
    rgba bg_disabled{};
    rgba border_hover{};
    rgba border_pressed{};
    rgba border_disabled{};
    rgba border_focus{};
    rgba font_color_disabled{};
    rgba accent_color{};
    rgba accent_hover{};
    rgba accent_pressed{};
    rgba accent_disabled{};
    rgba on_accent{};
    bool shadow_enabled{false};
    rgba shadow_color{};
    std::int16_t shadow_offset_x{0};
    std::int16_t shadow_offset_y{0};
    std::int16_t shadow_spread{0};
    std::int16_t shadow_radius{0};
    bool inner_stroke_enabled{false};
    rgba inner_stroke_color{};
    std::int16_t inner_stroke_width{0};
    bool outline_enabled{false};
    rgba outline_color{};
    std::int16_t outline_width{0};
    bool gradient_enabled{false};
    rgba gradient_start{};
    rgba gradient_end{};
    std::uint8_t gradient_direction{0};

    void apply_to(Style& s) const noexcept {
        if (has_bg_color) s.colors.bg_color = bg_color;
        if (has_border_color) s.colors.border_color = border_color;
        if (has_border_width) s.metrics.border_width = border_width;
        if (has_corner_radius) s.metrics.corner_radius = corner_radius;
        if (has_padding) s.metrics.padding = padding;
        if (has_header_padding) s.metrics.header_padding = header_padding;
        if (has_content_padding) s.metrics.content_padding = content_padding;
        if (has_scrollbar_margin) s.metrics.scrollbar_margin = scrollbar_margin;
        if (has_scrollbar_thumb_min) s.metrics.scrollbar_thumb_min = scrollbar_thumb_min;
        if (has_glass_highlight_pos) s.metrics.glass_highlight_pos = glass_highlight_pos;
        if (has_glass_highlight_alpha) s.metrics.glass_highlight_alpha = glass_highlight_alpha;
        if (has_glass_shadow_alpha) s.metrics.glass_shadow_alpha = glass_shadow_alpha;
        if (has_glass_opacity_min) s.metrics.glass_opacity_min = glass_opacity_min;
        if (has_glass_opacity_max) s.metrics.glass_opacity_max = glass_opacity_max;
        if (has_font) s.font = font;
        if (has_font_role) {
            s.font_role = font_role;
            if (!has_font) {
                s.font = nullptr;
            }
        }
        if (has_font_weight) s.font_weight = font_weight;
        if (has_font_color) s.colors.font_color = font_color;
        if (has_bg_hover) s.colors.bg_hover = bg_hover;
        if (has_bg_pressed) s.colors.bg_pressed = bg_pressed;
        if (has_bg_disabled) s.colors.bg_disabled = bg_disabled;
        if (has_border_hover) s.colors.border_hover = border_hover;
        if (has_border_pressed) s.colors.border_pressed = border_pressed;
        if (has_border_disabled) s.colors.border_disabled = border_disabled;
        if (has_border_focus) s.colors.border_focus = border_focus;
        if (has_font_color_disabled) s.colors.font_color_disabled = font_color_disabled;
    if (has_accent_color) {
        s.colors.accent_color = accent_color;
        if (!has_accent_hover) {
            s.colors.accent_hover = adjust_by_luma(accent_color, 12);
        }
        if (!has_accent_pressed) {
            s.colors.accent_pressed = adjust_by_luma(accent_color, 24);
        }
        if (!has_accent_disabled) {
            s.colors.accent_disabled = adjust_by_luma(accent_color, 40);
        }
    }
        if (has_accent_hover) s.colors.accent_hover = accent_hover;
        if (has_accent_pressed) s.colors.accent_pressed = accent_pressed;
        if (has_accent_disabled) s.colors.accent_disabled = accent_disabled;
        if (has_on_accent) s.colors.on_accent = on_accent;
        if (has_shadow_enabled) s.decoration.shadow_enabled = shadow_enabled;
        if (has_shadow_color) s.decoration.shadow_color = shadow_color;
        if (has_shadow_offset_x) s.decoration.shadow_offset_x = shadow_offset_x;
        if (has_shadow_offset_y) s.decoration.shadow_offset_y = shadow_offset_y;
        if (has_shadow_spread) s.decoration.shadow_spread = shadow_spread;
        if (has_shadow_radius) s.decoration.shadow_radius = shadow_radius;
        if (has_inner_stroke_enabled) s.decoration.inner_stroke_enabled = inner_stroke_enabled;
        if (has_inner_stroke_color) s.decoration.inner_stroke_color = inner_stroke_color;
        if (has_inner_stroke_width) s.decoration.inner_stroke_width = inner_stroke_width;
        if (has_outline_enabled) s.decoration.outline_enabled = outline_enabled;
        if (has_outline_color) s.decoration.outline_color = outline_color;
        if (has_outline_width) s.decoration.outline_width = outline_width;
        if (has_gradient_enabled) s.decoration.gradient_enabled = gradient_enabled;
        if (has_gradient_start) s.decoration.gradient_start = gradient_start;
        if (has_gradient_end) s.decoration.gradient_end = gradient_end;
        if (has_gradient_direction) s.decoration.gradient_direction = gradient_direction;
    }
};

static_assert(std::is_trivially_copyable_v<StylePatch>,
              "StylePatch must remain a fixed-cost value type");
static_assert(sizeof(StylePatch) <= 184,
              "StylePatch presence flags must remain packed");
static_assert([] {
    StylePatch patch{};
    patch.has_bg_color = true;
    patch.has_scrollbar_thumb_min = true;
    patch.has_shadow_enabled = true;
    patch.has_gradient_direction = true;
    return patch.has_bg_color
        && patch.has_scrollbar_thumb_min
        && patch.has_shadow_enabled
        && patch.has_gradient_direction
        && !patch.has_border_color
        && !patch.has_shadow_color
        && !patch.has_gradient_end;
}(), "StylePatch packed presence flags must remain independent");

export
enum class StylePatchKind : std::uint8_t {
    None = 0,
    Adjust = 1,
    Override = 2
};

export
struct StyleToken {
    StylePatch patch{};
};

export
using StyleClassId = std::uint8_t;

export
inline constexpr StyleClassId kStyleClassInvalid = 0;

export
inline constexpr std::size_t kStyleClassMax = style_class_max;
static_assert(kStyleClassMax > 0);
static_assert(kStyleClassMax
              <= static_cast<std::size_t>(std::numeric_limits<StyleClassId>::max()) + 1u);
static_assert(sizeof(StyleClassId) == 1);

export
inline void merge_style_patch(StylePatch& dst, const StylePatch& src) noexcept {
    if (src.has_bg_color) { dst.has_bg_color = true; dst.bg_color = src.bg_color; }
    if (src.has_border_color) { dst.has_border_color = true; dst.border_color = src.border_color; }
    if (src.has_border_width) { dst.has_border_width = true; dst.border_width = src.border_width; }
    if (src.has_corner_radius) { dst.has_corner_radius = true; dst.corner_radius = src.corner_radius; }
    if (src.has_padding) { dst.has_padding = true; dst.padding = src.padding; }
    if (src.has_header_padding) { dst.has_header_padding = true; dst.header_padding = src.header_padding; }
    if (src.has_content_padding) { dst.has_content_padding = true; dst.content_padding = src.content_padding; }
    if (src.has_scrollbar_margin) { dst.has_scrollbar_margin = true; dst.scrollbar_margin = src.scrollbar_margin; }
    if (src.has_scrollbar_thumb_min) { dst.has_scrollbar_thumb_min = true; dst.scrollbar_thumb_min = src.scrollbar_thumb_min; }
    if (src.has_glass_highlight_pos) { dst.has_glass_highlight_pos = true; dst.glass_highlight_pos = src.glass_highlight_pos; }
    if (src.has_glass_highlight_alpha) { dst.has_glass_highlight_alpha = true; dst.glass_highlight_alpha = src.glass_highlight_alpha; }
    if (src.has_glass_shadow_alpha) { dst.has_glass_shadow_alpha = true; dst.glass_shadow_alpha = src.glass_shadow_alpha; }
    if (src.has_glass_opacity_min) { dst.has_glass_opacity_min = true; dst.glass_opacity_min = src.glass_opacity_min; }
    if (src.has_glass_opacity_max) { dst.has_glass_opacity_max = true; dst.glass_opacity_max = src.glass_opacity_max; }
    if (src.has_font) { dst.has_font = true; dst.font = src.font; }
    if (src.has_font_role) { dst.has_font_role = true; dst.font_role = src.font_role; }
    if (src.has_font_weight) { dst.has_font_weight = true; dst.font_weight = src.font_weight; }
    if (src.has_font_color) { dst.has_font_color = true; dst.font_color = src.font_color; }
    if (src.has_bg_hover) { dst.has_bg_hover = true; dst.bg_hover = src.bg_hover; }
    if (src.has_bg_pressed) { dst.has_bg_pressed = true; dst.bg_pressed = src.bg_pressed; }
    if (src.has_bg_disabled) { dst.has_bg_disabled = true; dst.bg_disabled = src.bg_disabled; }
    if (src.has_border_hover) { dst.has_border_hover = true; dst.border_hover = src.border_hover; }
    if (src.has_border_pressed) { dst.has_border_pressed = true; dst.border_pressed = src.border_pressed; }
    if (src.has_border_disabled) { dst.has_border_disabled = true; dst.border_disabled = src.border_disabled; }
    if (src.has_border_focus) { dst.has_border_focus = true; dst.border_focus = src.border_focus; }
    if (src.has_font_color_disabled) { dst.has_font_color_disabled = true; dst.font_color_disabled = src.font_color_disabled; }
    if (src.has_accent_color) { dst.has_accent_color = true; dst.accent_color = src.accent_color; }
    if (src.has_accent_hover) { dst.has_accent_hover = true; dst.accent_hover = src.accent_hover; }
    if (src.has_accent_pressed) { dst.has_accent_pressed = true; dst.accent_pressed = src.accent_pressed; }
    if (src.has_accent_disabled) { dst.has_accent_disabled = true; dst.accent_disabled = src.accent_disabled; }
    if (src.has_on_accent) { dst.has_on_accent = true; dst.on_accent = src.on_accent; }
    if (src.has_shadow_enabled) { dst.has_shadow_enabled = true; dst.shadow_enabled = src.shadow_enabled; }
    if (src.has_shadow_color) { dst.has_shadow_color = true; dst.shadow_color = src.shadow_color; }
    if (src.has_shadow_offset_x) { dst.has_shadow_offset_x = true; dst.shadow_offset_x = src.shadow_offset_x; }
    if (src.has_shadow_offset_y) { dst.has_shadow_offset_y = true; dst.shadow_offset_y = src.shadow_offset_y; }
    if (src.has_shadow_spread) { dst.has_shadow_spread = true; dst.shadow_spread = src.shadow_spread; }
    if (src.has_shadow_radius) { dst.has_shadow_radius = true; dst.shadow_radius = src.shadow_radius; }
    if (src.has_inner_stroke_enabled) { dst.has_inner_stroke_enabled = true; dst.inner_stroke_enabled = src.inner_stroke_enabled; }
    if (src.has_inner_stroke_color) { dst.has_inner_stroke_color = true; dst.inner_stroke_color = src.inner_stroke_color; }
    if (src.has_inner_stroke_width) { dst.has_inner_stroke_width = true; dst.inner_stroke_width = src.inner_stroke_width; }
    if (src.has_outline_enabled) { dst.has_outline_enabled = true; dst.outline_enabled = src.outline_enabled; }
    if (src.has_outline_color) { dst.has_outline_color = true; dst.outline_color = src.outline_color; }
    if (src.has_outline_width) { dst.has_outline_width = true; dst.outline_width = src.outline_width; }
    if (src.has_gradient_enabled) { dst.has_gradient_enabled = true; dst.gradient_enabled = src.gradient_enabled; }
    if (src.has_gradient_start) { dst.has_gradient_start = true; dst.gradient_start = src.gradient_start; }
    if (src.has_gradient_end) { dst.has_gradient_end = true; dst.gradient_end = src.gradient_end; }
    if (src.has_gradient_direction) { dst.has_gradient_direction = true; dst.gradient_direction = src.gradient_direction; }
  }

export
struct StyleState {
    bool enabled{true};
    bool hovered{false};
    bool pressed{false};
    bool focused{false};
};

export
inline StyleState make_style_state(bool enabled,
                                   bool hovered,
                                   bool pressed,
                                   bool focused) noexcept {
    return StyleState{enabled, hovered, pressed, focused};
}

static_assert(sizeof(StyleState) == 4);

export
inline const Font& resolve_font(const Style& st) noexcept {
    return st.font ? *st.font : get_font_weighted(st.font_role, st.font_weight);
}

inline int luma(const rgba& c) noexcept {
    return (static_cast<int>(c.r) * 30 + static_cast<int>(c.g) * 59 + static_cast<int>(c.b) * 11) / 100;
}

inline rgba adjust_color(rgba c, int delta) noexcept {
    auto clamp = [](int v) noexcept { return (v < 0) ? 0 : (v > 255 ? 255 : v); };
    c.r = static_cast<std::uint8_t>(clamp(static_cast<int>(c.r) + delta));
    c.g = static_cast<std::uint8_t>(clamp(static_cast<int>(c.g) + delta));
    c.b = static_cast<std::uint8_t>(clamp(static_cast<int>(c.b) + delta));
    return c;
}

export inline rgba adjust_by_luma(const rgba& c, int delta) noexcept {
    return (luma(c) < 128) ? adjust_color(c, delta) : adjust_color(c, -delta);
}

export
inline void resolve_colors(const Style& st, const StyleState& state,
                           rgba& bg, rgba& border, rgba& font) noexcept {
    bg = st.colors.bg_color;
    border = st.colors.border_color;
    font = st.colors.font_color;
    if (!state.enabled) {
        bg = st.colors.bg_disabled;
        border = st.colors.border_disabled;
        font = st.colors.font_color_disabled;
        return;
    }
    if (state.pressed) {
        bg = st.colors.bg_pressed;
        border = st.colors.border_pressed;
    } else if (state.hovered) {
        bg = st.colors.bg_hover;
        border = st.colors.border_hover;
    }
}

export inline rgba resolve_accent(const Style& st, const StyleState& state) noexcept {
    if (!state.enabled) {
        return st.colors.accent_disabled.a ? st.colors.accent_disabled : st.colors.accent_color;
    }
    if (state.pressed) {
        return st.colors.accent_pressed.a ? st.colors.accent_pressed : st.colors.accent_color;
    }
    if (state.hovered) {
        return st.colors.accent_hover.a ? st.colors.accent_hover : st.colors.accent_color;
    }
    return st.colors.accent_color;
}

export
inline void apply_tokens_to_style(Style& s, const ThemeTokens& t) noexcept {
    s.colors.bg_color = t.surface;
    s.colors.bg_hover = adjust_by_luma(t.surface, 8);
    s.colors.bg_pressed = adjust_by_luma(t.surface, 20);
    s.colors.bg_disabled = adjust_by_luma(t.surface, 6);

    s.colors.border_color = t.outline;
    s.colors.border_hover = adjust_by_luma(t.outline, 20);
    s.colors.border_pressed = adjust_by_luma(t.outline, 40);
    s.colors.border_disabled = adjust_by_luma(t.outline, 16);
    s.colors.border_focus = t.focus_ring;

    s.colors.font_color = t.on_surface;
    s.colors.font_color_disabled = t.on_surface_muted;

    s.colors.accent_color = t.accent;
    s.colors.accent_hover = adjust_by_luma(t.accent, 12);
    s.colors.accent_pressed = adjust_by_luma(t.accent, 24);
    s.colors.accent_disabled = adjust_by_luma(t.accent, 40);
    s.colors.on_accent = t.on_accent;
}

export
inline Style make_style_from_tokens(const ThemeTokens& t) noexcept {
    Style s{};
    apply_tokens_to_style(s, t);
    return s;
}

export
template<typename Widget>
struct style_slot {
    static inline Style value{};
};

export
class Theme {
public:
    static Theme& instance() {
        static Theme inst;
        return inst;
    }

    template<typename Widget>
    [[nodiscard]] const Style& get() const noexcept {
        return style_slot<Widget>::value;
    }

    template<typename Widget>
    void set(const Style& s) noexcept {
        style_slot<Widget>::value = s;
    }

    template<typename Child, typename Parent>
    void inherit() noexcept {
        style_slot<Child>::value = style_slot<Parent>::value;
    }

    template<typename Widget>
    void patch(const StylePatch& patch) noexcept {
        patch.apply_to(style_slot<Widget>::value);
    }

    // Raw setter; prefer apply_theme_tokens (tokens + style slots).
    void set_tokens_unsafe(const ThemeTokens& t) noexcept {
        ThemeTokens next = t;
        next.version = tokens().version + 1;
        tokens() = next;
    }

    [[nodiscard]] const ThemeTokens& get_tokens() const noexcept {
        return tokens();
    }

    void set_default_font(const Font& f) noexcept {
        default_font_ptr() = &f;
    }

    void set_style_class(StyleClassId id, const StylePatch& patch) noexcept {
        if (id == kStyleClassInvalid || id >= kStyleClassMax) return;
        class_patch_[id] = patch;
        class_on_[id] = 1;
        class_version_++;
    }

    void clear_style_class(StyleClassId id) noexcept {
        if (id == kStyleClassInvalid || id >= kStyleClassMax) return;
        if (class_on_[id] == 0) return;
        class_patch_[id] = StylePatch{};
        class_on_[id] = 0;
        class_version_++;
    }

    [[nodiscard]] const StylePatch* style_class(StyleClassId id) const noexcept {
        if (id == kStyleClassInvalid || id >= kStyleClassMax) return nullptr;
        if (class_on_[id] == 0) return nullptr;
        return &class_patch_[id];
    }

    [[nodiscard]] std::uint32_t style_class_version() const noexcept {
        return class_version_;
    }

private:
    Theme() {
        default_font_ptr() = nullptr;
        tokens() = ThemeTokens{};
    }

    static const Font*& default_font_ptr() {
        static const Font* f = nullptr;
        return f;
    }

    static ThemeTokens& tokens() {
        static ThemeTokens t{};
        return t;
    }

    static const Font* default_font() {
        if (auto* f = default_font_ptr()) {
            return f;
        }
        return &get_font(FontId::Normal);
    }

    std::array<StylePatch, kStyleClassMax> class_patch_{};
    std::array<std::uint8_t, kStyleClassMax> class_on_{};
    std::uint32_t class_version_{0};
};

export
struct ThemeMemoryProfile {
    std::size_t style_class_max{0};
    std::size_t class_patch_bytes{0};
    std::size_t class_on_bytes{0};
    std::size_t theme_size_bytes{0};
};

export
inline constexpr ThemeMemoryProfile theme_memory_profile() noexcept {
    return ThemeMemoryProfile{
        kStyleClassMax,
        kStyleClassMax * sizeof(StylePatch),
        kStyleClassMax * sizeof(std::uint8_t),
        sizeof(Theme),
    };
}

#if CHARM_VIVID_MEMORY_PROFILE_SYMBOLS && defined(__GNUC__)
extern "C" [[gnu::used]] void charm_theme_memory_profile_symbols() noexcept {
    constexpr auto profile = theme_memory_profile();
    asm volatile(
        ".global charm_theme_profile_style_class_max\n"
        ".set charm_theme_profile_style_class_max, %c0\n"
        ".global charm_theme_profile_class_patch_bytes\n"
        ".set charm_theme_profile_class_patch_bytes, %c1\n"
        ".global charm_theme_profile_class_on_bytes\n"
        ".set charm_theme_profile_class_on_bytes, %c2\n"
        ".global charm_theme_profile_theme_size_bytes\n"
        ".set charm_theme_profile_theme_size_bytes, %c3\n"
        :
        : "i"(profile.style_class_max),
          "i"(profile.class_patch_bytes),
          "i"(profile.class_on_bytes),
          "i"(profile.theme_size_bytes));
}
#endif
