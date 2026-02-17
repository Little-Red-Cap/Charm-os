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
