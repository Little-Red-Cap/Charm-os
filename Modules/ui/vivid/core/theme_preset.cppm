module;
#include <type_traits>
export module charm.core.theme_preset;

export import charm.core.style;
export import charm.core.style_sheet;
export import charm.widgets.button;
export import charm.widgets.checkbox;
export import charm.widgets.label;
export import charm.widgets.list;
export import charm.widgets.list_view;
export import charm.widgets.progress;
export import charm.widgets.radio;
export import charm.widgets.scroll_container;
export import charm.widgets.scrollbar;
export import charm.widgets.slider;
export import charm.widgets.switcher;
export import charm.widgets.text_input;
export import charm.widgets.text_area;
export import charm.widgets.image_box;

export
struct ThemePreset {
    bool has_label{false};
    Style label{};
    bool has_button{false};
    Style button{};
    bool has_checkbox{false};
    Style checkbox{};
    bool has_list_view{false};
    Style list_view{};
    bool has_list_item{false};
    Style list_item{};
    bool has_list{false};
    Style list{};
    bool has_progress{false};
    Style progress{};
    bool has_scroll_container{false};
    Style scroll_container{};
    bool has_scroll_bar{false};
    Style scroll_bar{};
    bool has_slider{false};
    Style slider{};
    bool has_switch{false};
    Style switcher{};
    bool has_radio{false};
    Style radio{};
    bool has_text_input{false};
    Style text_input{};
    bool has_text_area{false};
    Style text_area{};
};

export
inline void apply_theme_preset(const ThemePreset& preset) noexcept {
    auto& theme = Theme::instance();
    if (preset.has_label) theme.set<Label>(preset.label);
    if (preset.has_button) theme.set<Button>(preset.button);
    if (preset.has_checkbox) theme.set<Checkbox>(preset.checkbox);
    if (preset.has_list_view) theme.set<ListView>(preset.list_view);
    if (preset.has_list_item) theme.set<ListItem>(preset.list_item);
    if (preset.has_list) theme.set<List>(preset.list);
    if (preset.has_progress) theme.set<Progress>(preset.progress);
    if (preset.has_scroll_container) theme.set<ScrollContainer>(preset.scroll_container);
    if (preset.has_scroll_bar) theme.set<ScrollBar>(preset.scroll_bar);
    if (preset.has_slider) theme.set<Slider>(preset.slider);
    if (preset.has_switch) theme.set<Switch>(preset.switcher);
    if (preset.has_radio) theme.set<Radio>(preset.radio);
    if (preset.has_text_input) theme.set<TextInput>(preset.text_input);
    if (preset.has_text_area) theme.set<TextArea>(preset.text_area);
}

export
inline void apply_baseline_theme_preset(const Style& base) noexcept {
    auto& theme = Theme::instance();
    auto apply_base = [&](Style& s) {
        s.colors.bg_color = base.colors.bg_color;
        s.colors.bg_hover = base.colors.bg_hover;
        s.colors.bg_pressed = base.colors.bg_pressed;
        s.colors.bg_disabled = base.colors.bg_disabled;
        s.metrics.padding = base.metrics.padding;
        s.metrics.corner_radius = base.metrics.corner_radius;
        s.metrics.border_width = base.metrics.border_width;
        s.colors.border_color = base.colors.border_color;
        s.colors.border_hover = base.colors.border_hover;
        s.colors.border_pressed = base.colors.border_pressed;
        s.colors.border_disabled = base.colors.border_disabled;
        s.colors.border_focus = base.colors.border_focus;
        s.colors.font_color = base.colors.font_color;
        s.colors.font_color_disabled = base.colors.font_color_disabled;
        s.metrics.scrollbar_margin = base.metrics.scrollbar_margin;
        s.metrics.scrollbar_thumb_min = base.metrics.scrollbar_thumb_min;
        s.colors.accent_color = base.colors.accent_color;
        s.colors.accent_hover = base.colors.accent_hover;
        s.colors.accent_pressed = base.colors.accent_pressed;
        s.colors.accent_disabled = base.colors.accent_disabled;
        s.colors.on_accent = base.colors.on_accent;
        if (base.font) {
            s.font = base.font;
        }
    };

    ThemePreset preset{};
    preset.has_label = true;
    preset.label = theme.get<Label>();
    apply_base(preset.label);
    preset.has_button = true;
    preset.button = theme.get<Button>();
    apply_base(preset.button);
    preset.has_checkbox = true;
    preset.checkbox = theme.get<Checkbox>();
    apply_base(preset.checkbox);
    preset.has_list_view = true;
    preset.list_view = theme.get<ListView>();
    apply_base(preset.list_view);
    preset.has_list_item = true;
    preset.list_item = theme.get<ListItem>();
    apply_base(preset.list_item);
    preset.has_list = true;
    preset.list = theme.get<List>();
    apply_base(preset.list);
    preset.has_progress = true;
    preset.progress = theme.get<Progress>();
    apply_base(preset.progress);
    preset.has_scroll_container = true;
    preset.scroll_container = theme.get<ScrollContainer>();
    apply_base(preset.scroll_container);
    preset.has_scroll_bar = true;
    preset.scroll_bar = theme.get<ScrollBar>();
    apply_base(preset.scroll_bar);
    preset.has_slider = true;
    preset.slider = theme.get<Slider>();
    apply_base(preset.slider);
    preset.has_switch = true;
    preset.switcher = theme.get<Switch>();
    apply_base(preset.switcher);
    preset.has_radio = true;
    preset.radio = theme.get<Radio>();
    apply_base(preset.radio);
    preset.has_text_input = true;
    preset.text_input = theme.get<TextInput>();
    apply_base(preset.text_input);
    preset.has_text_area = true;
    preset.text_area = theme.get<TextArea>();
    apply_base(preset.text_area);

    apply_theme_preset(preset);
}

export
inline void apply_tokens_to_all_widgets(const ThemeTokens& tokens) noexcept {
    auto& theme = Theme::instance();
    theme.set_tokens_unsafe(tokens);

    auto apply_widget = [&](auto* tag) {
        using Widget = std::remove_pointer_t<decltype(tag)>;
        Style s = theme.get<Widget>();
        apply_tokens_to_style(s, tokens);
        theme.set<Widget>(s);
    };

    apply_widget(static_cast<Label*>(nullptr));
    apply_widget(static_cast<Button*>(nullptr));
    apply_widget(static_cast<Checkbox*>(nullptr));
    apply_widget(static_cast<ListView*>(nullptr));
    apply_widget(static_cast<ListItem*>(nullptr));
    apply_widget(static_cast<List*>(nullptr));
    apply_widget(static_cast<Progress*>(nullptr));
    apply_widget(static_cast<ScrollContainer*>(nullptr));
    apply_widget(static_cast<ScrollBar*>(nullptr));
    apply_widget(static_cast<Slider*>(nullptr));
    apply_widget(static_cast<Switch*>(nullptr));
    apply_widget(static_cast<Radio*>(nullptr));
    apply_widget(static_cast<TextInput*>(nullptr));
    apply_widget(static_cast<TextArea*>(nullptr));
    apply_widget(static_cast<ImageBox*>(nullptr));
}

export
inline void apply_theme(const ThemeTokens& tokens) noexcept {
    apply_tokens_to_all_widgets(tokens);
}

export
inline void apply_theme_tokens(const ThemeTokens& tokens) noexcept {
    apply_theme(tokens);
}

export
inline void apply_ios_light_preset() noexcept {
    constexpr rgba kSystemBlue{0, 122, 255, 255};
    constexpr rgba kSurface{242, 242, 247, 255};
    constexpr rgba kSurfaceElevated{255, 255, 255, 255};
    constexpr rgba kBorder{199, 199, 204, 255};
    constexpr rgba kText{28, 28, 30, 255};

    ThemeTokens tokens{};
    tokens.surface = kSurface;
    tokens.surface_variant = kSurfaceElevated;
    tokens.on_surface = kText;
    tokens.on_surface_muted = kBorder;
    tokens.outline = kBorder;
    tokens.accent = kSystemBlue;
    tokens.on_accent = kSurfaceElevated;
    tokens.focus_ring = kSystemBlue;
    apply_theme_tokens(tokens);
}

export
inline void apply_material3_light_preset() noexcept {
    constexpr rgba kPrimary{103, 80, 164, 255};
    constexpr rgba kOnPrimary{255, 255, 255, 255};
    constexpr rgba kSurface{255, 251, 254, 255};
    constexpr rgba kSurfaceVariant{231, 224, 236, 255};
    constexpr rgba kOutline{121, 116, 126, 255};
    constexpr rgba kOnSurface{28, 27, 31, 255};

    ThemeTokens tokens{};
    tokens.surface = kSurface;
    tokens.surface_variant = kSurfaceVariant;
    tokens.on_surface = kOnSurface;
    tokens.on_surface_muted = kOutline;
    tokens.outline = kOutline;
    tokens.accent = kPrimary;
    tokens.on_accent = kOnPrimary;
    tokens.focus_ring = kPrimary;
    apply_theme_tokens(tokens);
}

export
inline void apply_lvgl_default_preset() noexcept {
    constexpr rgba kLvBg{240, 240, 240, 255};
    constexpr rgba kLvBorder{160, 160, 160, 255};
    constexpr rgba kLvBorderFocus{80, 120, 200, 255};
    constexpr rgba kLvText{40, 40, 40, 255};
    constexpr rgba kLvAccent{0, 136, 255, 255};

    ThemeTokens tokens{};
    tokens.surface = kLvBg;
    tokens.surface_variant = kLvBg;
    tokens.on_surface = kLvText;
    tokens.on_surface_muted = kLvBorder;
    tokens.outline = kLvBorder;
    tokens.accent = kLvAccent;
    tokens.on_accent = kLvText;
    tokens.focus_ring = kLvBorderFocus;
    apply_theme_tokens(tokens);
}

export
inline void apply_arm2d_demo_preset() noexcept {
    constexpr rgba kPanel{24, 28, 36, 255};
    constexpr rgba kPanelBorder{58, 72, 92, 255};
    constexpr rgba kPanelHover{32, 38, 50, 255};
    constexpr rgba kAccent{48, 198, 255, 255};
    constexpr rgba kText{210, 220, 235, 255};

    ThemeTokens tokens{};
    tokens.surface = kPanel;
    tokens.surface_variant = kPanelHover;
    tokens.on_surface = kText;
    tokens.on_surface_muted = kPanelBorder;
    tokens.outline = kPanelBorder;
    tokens.accent = kAccent;
    tokens.on_accent = kText;
    tokens.focus_ring = kAccent;
    apply_theme_tokens(tokens);
}
