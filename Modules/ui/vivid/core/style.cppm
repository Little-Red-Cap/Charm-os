module;
#include <cstdint>
#include <type_traits>
export module charm.core.style;

export import charm.gfx.color;
export import charm.font.typography;

export
struct Style {
    rgba      bg_color      = {240,240,240,255};
    rgba      border_color  = {180,180,180,255};
    int       border_width  = 1;
    int       corner_radius = 4;
    int       padding       = 4;
    int       header_padding = 4;
    int       content_padding = 8;
    int       scrollbar_margin = 2;
    int       scrollbar_thumb_min = 12;
    int       glass_highlight_pos = 10;
    int       glass_highlight_alpha = 70;
    int       glass_shadow_alpha = 40;
    int       glass_opacity_min = 40;
    int       glass_opacity_max = 200;

    const Font* font         = nullptr;
    rgba        font_color   = {  0,  0,  0,255};

    rgba      bg_hover      = {230,230,230,255};
    rgba      bg_pressed    = {210,210,210,255};
    rgba      bg_disabled   = {220,220,220,255};
    rgba      border_hover  = {120,120,120,255};
    rgba      border_pressed= { 80, 80, 80,255};
    rgba      border_disabled= {160,160,160,255};
    rgba      border_focus  = { 80,120,200,255};
    rgba      font_color_disabled = {120,120,120,255};

    rgba      accent_color  = { 80,120,200,255};
    rgba      accent_hover  = {  0,  0,  0,  0}; // 0 alpha = derive from accent_color
    rgba      accent_pressed= {  0,  0,  0,  0}; // 0 alpha = derive from accent_color
    rgba      accent_disabled= { 0,  0,  0,  0}; // 0 alpha = derive from accent_color
    rgba      on_accent     = {255,255,255,255};
};

export
struct ThemeTokens {
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

export
struct StylePatch {
    bool has_bg_color{false};
    bool has_border_color{false};
    bool has_border_width{false};
    bool has_corner_radius{false};
    bool has_padding{false};
    bool has_header_padding{false};
    bool has_content_padding{false};
    bool has_scrollbar_margin{false};
    bool has_scrollbar_thumb_min{false};
    bool has_glass_highlight_pos{false};
    bool has_glass_highlight_alpha{false};
    bool has_glass_shadow_alpha{false};
    bool has_glass_opacity_min{false};
    bool has_glass_opacity_max{false};
    bool has_font{false};
    bool has_font_color{false};
    bool has_bg_hover{false};
    bool has_bg_pressed{false};
    bool has_bg_disabled{false};
    bool has_border_hover{false};
    bool has_border_pressed{false};
    bool has_border_disabled{false};
    bool has_border_focus{false};
    bool has_font_color_disabled{false};
    bool has_accent_color{false};
    bool has_accent_hover{false};
    bool has_accent_pressed{false};
    bool has_accent_disabled{false};
    bool has_on_accent{false};

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

    void apply_to(Style& s) const noexcept {
        if (has_bg_color) s.bg_color = bg_color;
        if (has_border_color) s.border_color = border_color;
        if (has_border_width) s.border_width = border_width;
        if (has_corner_radius) s.corner_radius = corner_radius;
        if (has_padding) s.padding = padding;
        if (has_header_padding) s.header_padding = header_padding;
        if (has_content_padding) s.content_padding = content_padding;
        if (has_scrollbar_margin) s.scrollbar_margin = scrollbar_margin;
        if (has_scrollbar_thumb_min) s.scrollbar_thumb_min = scrollbar_thumb_min;
        if (has_glass_highlight_pos) s.glass_highlight_pos = glass_highlight_pos;
        if (has_glass_highlight_alpha) s.glass_highlight_alpha = glass_highlight_alpha;
        if (has_glass_shadow_alpha) s.glass_shadow_alpha = glass_shadow_alpha;
        if (has_glass_opacity_min) s.glass_opacity_min = glass_opacity_min;
        if (has_glass_opacity_max) s.glass_opacity_max = glass_opacity_max;
        if (has_font) s.font = font;
        if (has_font_color) s.font_color = font_color;
        if (has_bg_hover) s.bg_hover = bg_hover;
        if (has_bg_pressed) s.bg_pressed = bg_pressed;
        if (has_bg_disabled) s.bg_disabled = bg_disabled;
        if (has_border_hover) s.border_hover = border_hover;
        if (has_border_pressed) s.border_pressed = border_pressed;
        if (has_border_disabled) s.border_disabled = border_disabled;
        if (has_border_focus) s.border_focus = border_focus;
        if (has_font_color_disabled) s.font_color_disabled = font_color_disabled;
        if (has_accent_color) s.accent_color = accent_color;
        if (has_accent_hover) s.accent_hover = accent_hover;
        if (has_accent_pressed) s.accent_pressed = accent_pressed;
        if (has_accent_disabled) s.accent_disabled = accent_disabled;
        if (has_on_accent) s.on_accent = on_accent;
    }
};

export
struct StyleState {
    bool enabled{true};
    bool hovered{false};
    bool pressed{false};
    bool focused{false};
    std::uint8_t variant{0};
};

export
inline StyleState make_style_state(bool enabled,
                                   bool hovered,
                                   bool pressed,
                                   bool focused,
                                   std::uint8_t variant = 0) noexcept {
    return StyleState{enabled, hovered, pressed, focused, variant};
}

export
inline const Font& resolve_font(const Style& st) noexcept {
    return st.font ? *st.font : get_font(FontId::Normal);
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
    bg = st.bg_color;
    border = st.border_color;
    font = st.font_color;
    if (!state.enabled) {
        bg = st.bg_disabled;
        border = st.border_disabled;
        font = st.font_color_disabled;
        return;
    }
    if (state.pressed) {
        bg = st.bg_pressed;
        border = st.border_pressed;
    } else if (state.hovered) {
        bg = st.bg_hover;
        border = st.border_hover;
    }
}

export inline rgba resolve_accent(const Style& st, const StyleState& state) noexcept {
    if (!state.enabled) {
        return st.accent_disabled.a ? st.accent_disabled : adjust_by_luma(st.accent_color, 40);
    }
    if (state.pressed) {
        return st.accent_pressed.a ? st.accent_pressed : adjust_by_luma(st.accent_color, 24);
    }
    if (state.hovered) {
        return st.accent_hover.a ? st.accent_hover : adjust_by_luma(st.accent_color, 12);
    }
    return st.accent_color;
}

export
inline void apply_tokens_to_style(Style& s, const ThemeTokens& t) noexcept {
    s.bg_color = t.surface;
    s.bg_hover = adjust_by_luma(t.surface, 8);
    s.bg_pressed = adjust_by_luma(t.surface, 20);
    s.bg_disabled = adjust_by_luma(t.surface, 6);

    s.border_color = t.outline;
    s.border_hover = adjust_by_luma(t.outline, 20);
    s.border_pressed = adjust_by_luma(t.outline, 40);
    s.border_disabled = adjust_by_luma(t.outline, 16);
    s.border_focus = t.focus_ring;

    s.font_color = t.on_surface;
    s.font_color_disabled = t.on_surface_muted;

    s.accent_color = t.accent;
    s.accent_hover = {0, 0, 0, 0};
    s.accent_pressed = {0, 0, 0, 0};
    s.accent_disabled = {0, 0, 0, 0};
    s.on_accent = t.on_accent;
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
        Style copy = s;
        if (!copy.font) {
            copy.font = default_font();
        }
        style_slot<Widget>::value = copy;
    }

    template<typename Child, typename Parent>
    void inherit() noexcept {
        style_slot<Child>::value = style_slot<Parent>::value;
    }

    template<typename Widget>
    void patch(const StylePatch& patch) noexcept {
        patch.apply_to(style_slot<Widget>::value);
    }

    void set_tokens(const ThemeTokens& t) noexcept {
        tokens() = t;
    }

    [[nodiscard]] const ThemeTokens& get_tokens() const noexcept {
        return tokens();
    }

    void set_default_font(const Font& f) noexcept {
        default_font_ptr() = &f;
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
};
