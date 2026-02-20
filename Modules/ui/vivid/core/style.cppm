module;
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

    rgba bg_color{};
    rgba border_color{};
    int  border_width{0};
    int  corner_radius{0};
    int  padding{0};
    int  header_padding{0};
    int  content_padding{0};
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

    void apply_to(Style& s) const noexcept {
        if (has_bg_color) s.bg_color = bg_color;
        if (has_border_color) s.border_color = border_color;
        if (has_border_width) s.border_width = border_width;
        if (has_corner_radius) s.corner_radius = corner_radius;
        if (has_padding) s.padding = padding;
        if (has_header_padding) s.header_padding = header_padding;
        if (has_content_padding) s.content_padding = content_padding;
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
    }
};

export
struct StyleState {
    bool enabled{true};
    bool hovered{false};
    bool pressed{false};
    bool focused{false};
};

export
inline const Font& resolve_font(const Style& st) noexcept {
    return st.font ? *st.font : get_font(FontId::Normal);
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
    } else if (state.focused) {
        border = st.border_focus;
    }
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

    void set_default_font(const Font& f) noexcept {
        default_font_ptr() = &f;
    }

private:
    Theme() {
        default_font_ptr() = &get_font(FontId::Normal);
    }

    static const Font*& default_font_ptr() {
        static const Font* f = nullptr;
        return f;
    }

    static const Font* default_font() {
        return default_font_ptr();
    }
};
