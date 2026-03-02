module;
#include <type_traits>

#include "features.hpp"
export module charm.core.theme_preset;

export import charm.core.style;
export import charm.core.style_sheet;
#if CHARM_VIVID_SOA_ONLY
export import charm.core.widget_registry;
export import charm.font.typography;
#else
#if CHARM_VIVID_ENABLE_WIDGET_Button
export import charm.widgets.button;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Checkbox
export import charm.widgets.checkbox;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Label
export import charm.widgets.label;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_List
export import charm.widgets.list;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ListView
export import charm.widgets.list_view;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Progress
export import charm.widgets.progress;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Radio
export import charm.widgets.radio;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ScrollContainer
export import charm.widgets.scroll_container;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ScrollBar
export import charm.widgets.scrollbar;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Slider
export import charm.widgets.slider;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Switch
export import charm.widgets.switcher;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextInput
export import charm.widgets.text_input;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextArea
export import charm.widgets.text_area;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ImageBox
export import charm.widgets.image_box;
#endif
#endif

#if CHARM_VIVID_SOA_ONLY
inline void sync_style_sheet_bases() noexcept {
    auto& sheet = StyleSheet::instance();
    sheet.notify_base_style_changed();
    sheet.rebuild_if_needed();
}
#else
inline void sync_style_sheet_bases() noexcept {
    auto& theme = Theme::instance();
    auto& sheet = StyleSheet::instance();

    auto set_base = [&](auto* tag, WidgetKind kind) {
        using Widget = std::remove_pointer_t<decltype(tag)>;
        sheet.set_base_style(kind, theme.get<Widget>());
    };

#if CHARM_VIVID_ENABLE_WIDGET_Label
    set_base(static_cast<Label*>(nullptr), WidgetKind::Label);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Button
    set_base(static_cast<Button*>(nullptr), WidgetKind::Button);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Checkbox
    set_base(static_cast<Checkbox*>(nullptr), WidgetKind::Checkbox);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ListView
    set_base(static_cast<ListView*>(nullptr), WidgetKind::ListView);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ListItem && CHARM_VIVID_ENABLE_WIDGET_List
    set_base(static_cast<ListItem*>(nullptr), WidgetKind::ListItem);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_List
    set_base(static_cast<List*>(nullptr), WidgetKind::List);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Progress
    set_base(static_cast<Progress*>(nullptr), WidgetKind::Progress);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ScrollContainer
    set_base(static_cast<ScrollContainer*>(nullptr), WidgetKind::ScrollContainer);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ScrollBar
    set_base(static_cast<ScrollBar*>(nullptr), WidgetKind::ScrollBar);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Slider
    set_base(static_cast<Slider*>(nullptr), WidgetKind::Slider);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Switch
    set_base(static_cast<Switch*>(nullptr), WidgetKind::Switch);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Radio
    set_base(static_cast<Radio*>(nullptr), WidgetKind::Radio);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextInput
    set_base(static_cast<TextInput*>(nullptr), WidgetKind::TextInput);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextArea
    set_base(static_cast<TextArea*>(nullptr), WidgetKind::TextArea);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ImageBox
    set_base(static_cast<ImageBox*>(nullptr), WidgetKind::ImageBox);
#endif

    sheet.notify_base_style_changed();
    sheet.rebuild_if_needed();
}
#endif

#if CHARM_VIVID_SOA_ONLY
inline void ensure_style_font(Style& s) noexcept {
    if (!s.font) {
        s.font = &get_font(FontId::Normal);
    }
}
#endif

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
#if CHARM_VIVID_SOA_ONLY
    auto& sheet = StyleSheet::instance();
    auto set_base = [&](Style s, WidgetKind kind) {
        ensure_style_font(s);
        sheet.set_base_style(kind, s);
    };
#if CHARM_VIVID_ENABLE_WIDGET_Label
    if (preset.has_label) set_base(preset.label, WidgetKind::Label);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Button
    if (preset.has_button) set_base(preset.button, WidgetKind::Button);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Checkbox
    if (preset.has_checkbox) set_base(preset.checkbox, WidgetKind::Checkbox);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ListView
    if (preset.has_list_view) set_base(preset.list_view, WidgetKind::ListView);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ListItem && CHARM_VIVID_ENABLE_WIDGET_List
    if (preset.has_list_item) set_base(preset.list_item, WidgetKind::ListItem);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_List
    if (preset.has_list) set_base(preset.list, WidgetKind::List);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Progress
    if (preset.has_progress) set_base(preset.progress, WidgetKind::Progress);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ScrollContainer
    if (preset.has_scroll_container) set_base(preset.scroll_container, WidgetKind::ScrollContainer);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ScrollBar
    if (preset.has_scroll_bar) set_base(preset.scroll_bar, WidgetKind::ScrollBar);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Slider
    if (preset.has_slider) set_base(preset.slider, WidgetKind::Slider);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Switch
    if (preset.has_switch) set_base(preset.switcher, WidgetKind::Switch);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Radio
    if (preset.has_radio) set_base(preset.radio, WidgetKind::Radio);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextInput
    if (preset.has_text_input) set_base(preset.text_input, WidgetKind::TextInput);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextArea
    if (preset.has_text_area) set_base(preset.text_area, WidgetKind::TextArea);
#endif
    sync_style_sheet_bases();
#else
    auto& theme = Theme::instance();
#if CHARM_VIVID_ENABLE_WIDGET_Label
    if (preset.has_label) theme.set<Label>(preset.label);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Button
    if (preset.has_button) theme.set<Button>(preset.button);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Checkbox
    if (preset.has_checkbox) theme.set<Checkbox>(preset.checkbox);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ListView
    if (preset.has_list_view) theme.set<ListView>(preset.list_view);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ListItem && CHARM_VIVID_ENABLE_WIDGET_List
    if (preset.has_list_item) theme.set<ListItem>(preset.list_item);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_List
    if (preset.has_list) theme.set<List>(preset.list);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Progress
    if (preset.has_progress) theme.set<Progress>(preset.progress);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ScrollContainer
    if (preset.has_scroll_container) theme.set<ScrollContainer>(preset.scroll_container);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ScrollBar
    if (preset.has_scroll_bar) theme.set<ScrollBar>(preset.scroll_bar);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Slider
    if (preset.has_slider) theme.set<Slider>(preset.slider);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Switch
    if (preset.has_switch) theme.set<Switch>(preset.switcher);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Radio
    if (preset.has_radio) theme.set<Radio>(preset.radio);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextInput
    if (preset.has_text_input) theme.set<TextInput>(preset.text_input);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextArea
    if (preset.has_text_area) theme.set<TextArea>(preset.text_area);
#endif
    sync_style_sheet_bases();
#endif
}

export
inline void apply_baseline_theme_preset(const Style& base) noexcept {
#if CHARM_VIVID_SOA_ONLY
    auto& sheet = StyleSheet::instance();
    Style base_copy = base;
    ensure_style_font(base_copy);
    for (const auto kind : enabled_widget_kinds) {
        sheet.set_base_style(kind, base_copy);
    }
    sync_style_sheet_bases();
#else
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
#if CHARM_VIVID_ENABLE_WIDGET_Label
    preset.has_label = true;
    preset.label = theme.get<Label>();
    apply_base(preset.label);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Button
    preset.has_button = true;
    preset.button = theme.get<Button>();
    apply_base(preset.button);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Checkbox
    preset.has_checkbox = true;
    preset.checkbox = theme.get<Checkbox>();
    apply_base(preset.checkbox);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ListView
    preset.has_list_view = true;
    preset.list_view = theme.get<ListView>();
    apply_base(preset.list_view);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ListItem && CHARM_VIVID_ENABLE_WIDGET_List
    preset.has_list_item = true;
    preset.list_item = theme.get<ListItem>();
    apply_base(preset.list_item);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_List
    preset.has_list = true;
    preset.list = theme.get<List>();
    apply_base(preset.list);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Progress
    preset.has_progress = true;
    preset.progress = theme.get<Progress>();
    apply_base(preset.progress);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ScrollContainer
    preset.has_scroll_container = true;
    preset.scroll_container = theme.get<ScrollContainer>();
    apply_base(preset.scroll_container);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ScrollBar
    preset.has_scroll_bar = true;
    preset.scroll_bar = theme.get<ScrollBar>();
    apply_base(preset.scroll_bar);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Slider
    preset.has_slider = true;
    preset.slider = theme.get<Slider>();
    apply_base(preset.slider);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Switch
    preset.has_switch = true;
    preset.switcher = theme.get<Switch>();
    apply_base(preset.switcher);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Radio
    preset.has_radio = true;
    preset.radio = theme.get<Radio>();
    apply_base(preset.radio);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextInput
    preset.has_text_input = true;
    preset.text_input = theme.get<TextInput>();
    apply_base(preset.text_input);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextArea
    preset.has_text_area = true;
    preset.text_area = theme.get<TextArea>();
    apply_base(preset.text_area);
#endif

    apply_theme_preset(preset);
#endif
}

export
inline void apply_tokens_to_all_widgets(const ThemeTokens& tokens) noexcept {
#if CHARM_VIVID_SOA_ONLY
    auto& theme = Theme::instance();
    theme.set_tokens_unsafe(tokens);
    auto& sheet = StyleSheet::instance();
    Style base = make_style_from_tokens(tokens);
    ensure_style_font(base);
    for (const auto kind : enabled_widget_kinds) {
        sheet.set_base_style(kind, base);
    }
    sync_style_sheet_bases();
#else
    auto& theme = Theme::instance();
    theme.set_tokens_unsafe(tokens);

    auto apply_widget = [&](auto* tag) {
        using Widget = std::remove_pointer_t<decltype(tag)>;
        Style s = theme.get<Widget>();
        apply_tokens_to_style(s, tokens);
        theme.set<Widget>(s);
    };

#if CHARM_VIVID_ENABLE_WIDGET_Label
    apply_widget(static_cast<Label*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Button
    apply_widget(static_cast<Button*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Checkbox
    apply_widget(static_cast<Checkbox*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ListView
    apply_widget(static_cast<ListView*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ListItem && CHARM_VIVID_ENABLE_WIDGET_List
    apply_widget(static_cast<ListItem*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_List
    apply_widget(static_cast<List*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Progress
    apply_widget(static_cast<Progress*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ScrollContainer
    apply_widget(static_cast<ScrollContainer*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ScrollBar
    apply_widget(static_cast<ScrollBar*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Slider
    apply_widget(static_cast<Slider*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Switch
    apply_widget(static_cast<Switch*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Radio
    apply_widget(static_cast<Radio*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextInput
    apply_widget(static_cast<TextInput*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextArea
    apply_widget(static_cast<TextArea*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ImageBox
    apply_widget(static_cast<ImageBox*>(nullptr));
#endif
    sync_style_sheet_bases();
#endif
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
