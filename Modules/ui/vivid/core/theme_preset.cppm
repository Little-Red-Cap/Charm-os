module;
#include <type_traits>
export module charm.core.theme_preset;

export import charm.core.style;
export import charm.core.style_sheet;
export import charm.core.widget_registry;
export import charm.font.typography;

inline void sync_style_sheet_bases() noexcept {
    auto& sheet = StyleSheet::instance();
    sheet.rebuild_if_needed();
}

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
    auto& sheet = StyleSheet::instance();
    auto set_base = [&](Style s, WidgetKind kind) {
        sheet.set_base_style(kind, s);
    };
    if constexpr (widget_kind_enabled(WidgetKind::Label)) {
        if (preset.has_label) set_base(preset.label, WidgetKind::Label);
    }
    if constexpr (widget_kind_enabled(WidgetKind::Button)) {
        if (preset.has_button) set_base(preset.button, WidgetKind::Button);
    }
    if constexpr (widget_kind_enabled(WidgetKind::IconButton)) {
        if (preset.has_button) set_base(preset.button, WidgetKind::IconButton);
    }
    if constexpr (widget_kind_enabled(WidgetKind::Checkbox)) {
        if (preset.has_checkbox) set_base(preset.checkbox, WidgetKind::Checkbox);
    }
    if constexpr (widget_kind_enabled(WidgetKind::ListView)) {
        if (preset.has_list_view) set_base(preset.list_view, WidgetKind::ListView);
    }
    if constexpr (widget_kind_enabled(WidgetKind::ListItem) && widget_kind_enabled(WidgetKind::List)) {
        if (preset.has_list_item) set_base(preset.list_item, WidgetKind::ListItem);
    }
    if constexpr (widget_kind_enabled(WidgetKind::List)) {
        if (preset.has_list) set_base(preset.list, WidgetKind::List);
    }
    if constexpr (widget_kind_enabled(WidgetKind::Progress)) {
        if (preset.has_progress) set_base(preset.progress, WidgetKind::Progress);
    }
    if constexpr (widget_kind_enabled(WidgetKind::ScrollContainer)) {
        if (preset.has_scroll_container) set_base(preset.scroll_container, WidgetKind::ScrollContainer);
    }
    if constexpr (widget_kind_enabled(WidgetKind::ScrollBar)) {
        if (preset.has_scroll_bar) set_base(preset.scroll_bar, WidgetKind::ScrollBar);
    }
    if constexpr (widget_kind_enabled(WidgetKind::Slider)) {
        if (preset.has_slider) set_base(preset.slider, WidgetKind::Slider);
    }
    if constexpr (widget_kind_enabled(WidgetKind::Switch)) {
        if (preset.has_switch) set_base(preset.switcher, WidgetKind::Switch);
    }
    if constexpr (widget_kind_enabled(WidgetKind::Radio)) {
        if (preset.has_radio) set_base(preset.radio, WidgetKind::Radio);
    }
    if constexpr (widget_kind_enabled(WidgetKind::TextInput)) {
        if (preset.has_text_input) set_base(preset.text_input, WidgetKind::TextInput);
    }
    if constexpr (widget_kind_enabled(WidgetKind::TextArea)) {
        if (preset.has_text_area) set_base(preset.text_area, WidgetKind::TextArea);
    }
    sync_style_sheet_bases();
}

export
inline void apply_baseline_theme_preset(const Style& base) noexcept {
    auto& sheet = StyleSheet::instance();
    for (const auto kind : enabled_widget_kinds) {
        sheet.set_base_style(kind, base);
    }
    sync_style_sheet_bases();
}
