module;
#include <type_traits>

#include "features.hpp"
export module charm.core.theme_preset;

export import charm.core.style;
export import charm.core.style_sheet;
export import charm.core.widget_registry;
export import charm.font.typography;

inline void sync_style_sheet_bases() noexcept {
    auto& sheet = StyleSheet::instance();
    sheet.notify_base_style_changed();
    sheet.rebuild_if_needed();
}

inline void ensure_style_font(Style& s) noexcept {
    if (!s.font) {
        s.font = &get_font(FontId::Normal);
    }
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
        ensure_style_font(s);
        sheet.set_base_style(kind, s);
    };
#if CHARM_VIVID_ENABLE_WIDGET_Label
    if (preset.has_label) set_base(preset.label, WidgetKind::Label);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Button
    if (preset.has_button) set_base(preset.button, WidgetKind::Button);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_IconButton
    if (preset.has_button) set_base(preset.button, WidgetKind::IconButton);
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
}

export
inline void apply_baseline_theme_preset(const Style& base) noexcept {
    auto& sheet = StyleSheet::instance();
    Style base_copy = base;
    ensure_style_font(base_copy);
    for (const auto kind : enabled_widget_kinds) {
        sheet.set_base_style(kind, base_copy);
    }
    sync_style_sheet_bases();
}
