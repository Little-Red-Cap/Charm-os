module;
#include <type_traits>
export module charm.core.theme_preset;

#include "features.hpp"

export import charm.core.style;
export import charm.core.style_sheet;
#if CHARM_VIVID_ENABLE_WIDGET_Button
export import charm.widgets.button;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Arc
export import charm.widgets.arc;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Bar
export import charm.widgets.bar;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Checkbox
export import charm.widgets.checkbox;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_CodeBlock
export import charm.widgets.code_block;
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
#if CHARM_VIVID_ENABLE_WIDGET_IconList
export import charm.widgets.icon_list;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextList
export import charm.widgets.text_list;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextTrackingList
export import charm.widgets.text_tracking_list;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Dial
export import charm.widgets.dial;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Dropdown
export import charm.widgets.dropdown_popup;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Image
export import charm.widgets.image;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ImageBox
export import charm.widgets.image_box;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Led
export import charm.widgets.led;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_MessageBox
export import charm.widgets.message_box;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ModalDialog
export import charm.widgets.modal_dialog;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Progress
export import charm.widgets.progress;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressBarSimple
export import charm.widgets.progress_bar_simple;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressBarRound
export import charm.widgets.progress_bar_round;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressBarDrill
export import charm.widgets.progress_bar_drill;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressFlowing
export import charm.widgets.progress_bar_flowing;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressFlowing
export import charm.widgets.progress_flowing;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressWheel
export import charm.widgets.progress_wheel;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Spinner
export import charm.widgets.spinner;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ScrollContainer
export import charm.widgets.scroll_container;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ScrollBar
export import charm.widgets.scrollbar;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_SegmentedControl
export import charm.widgets.segmented_control;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Slider
export import charm.widgets.slider;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Switch
export import charm.widgets.switcher;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Radio
export import charm.widgets.radio;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_RadioGroup
export import charm.widgets.radio_group;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextInput
export import charm.widgets.text_input;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextArea
export import charm.widgets.text_area;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextBox
export import charm.widgets.text_box;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_NumberInput
export import charm.widgets.number_input;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_NumberList
export import charm.widgets.number_list;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ToggleGroup
export import charm.widgets.toggle_group;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TableView
export import charm.widgets.table_view;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TreeView
export import charm.widgets.tree_view;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Dropdown
export import charm.widgets.dropdown;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Menu
export import charm.widgets.menu;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_MenuItem
export import charm.widgets.menu_item;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TabView
export import charm.widgets.tabview;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Roller
export import charm.widgets.roller;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_RichText
export import charm.widgets.rich_text;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Stepper
export import charm.widgets.stepper;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Timeline
export import charm.widgets.timeline;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_PerfOverlay
export import charm.widgets.perf_overlay;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_PopupLayer
export import charm.widgets.popup_layer;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_PrimitivesCanvas
export import charm.widgets.primitives_canvas;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_FoldablePanel
export import charm.widgets.foldable_panel;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_CloudyGlass
export import charm.widgets.cloudy_glass;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_DynamicNebula
export import charm.widgets.dynamic_nebula;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_CrtScreen
export import charm.widgets.crt_screen;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_SpectrumView
export import charm.widgets.spectrum_view;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_SpinningWheel
export import charm.widgets.spinning_wheel;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_SpinZoomWidget
export import charm.widgets.spin_zoom_widget;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_MeterPointer
export import charm.widgets.meter_pointer;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_BatteryGauge
export import charm.widgets.battery_gauge;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_BatteryGasGauge
export import charm.widgets.battery_gasgauge;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Histogram
export import charm.widgets.histogram;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_HistogramView
export import charm.widgets.histogram_view;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Chart
export import charm.widgets.chart;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Waveform
export import charm.widgets.waveform;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_WaveformView
export import charm.widgets.waveform_view;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Gauge
export import charm.widgets.gauge;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_RingIndication
export import charm.widgets.ring_indication;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_BusyWheel
export import charm.widgets.busy_wheel;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ConsoleBox
export import charm.widgets.console_box;
#endif

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
#if CHARM_VIVID_ENABLE_WIDGET_IconButton
        set_base(static_cast<Button*>(nullptr), WidgetKind::IconButton);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Checkbox
        set_base(static_cast<Checkbox*>(nullptr), WidgetKind::Checkbox);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ListView
        set_base(static_cast<ListView*>(nullptr), WidgetKind::ListView);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ListItem
        set_base(static_cast<ListItem*>(nullptr), WidgetKind::ListItem);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_List
        set_base(static_cast<List*>(nullptr), WidgetKind::List);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_IconList
        set_base(static_cast<IconList*>(nullptr), WidgetKind::IconList);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextList
        set_base(static_cast<TextList*>(nullptr), WidgetKind::TextList);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextTrackingList
        set_base(static_cast<TextTrackingList*>(nullptr), WidgetKind::TextTrackingList);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Progress
        set_base(static_cast<Progress*>(nullptr), WidgetKind::Progress);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressBarSimple
        set_base(static_cast<ProgressBarSimple*>(nullptr), WidgetKind::ProgressBarSimple);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressBarRound
        set_base(static_cast<ProgressBarRound*>(nullptr), WidgetKind::ProgressBarRound);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressBarDrill
        set_base(static_cast<ProgressBarDrill*>(nullptr), WidgetKind::ProgressBarDrill);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressFlowing
        set_base(static_cast<ProgressBarFlowing*>(nullptr), WidgetKind::ProgressFlowing);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressFlowing
        set_base(static_cast<ProgressFlowing*>(nullptr), WidgetKind::ProgressFlowing);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressWheel
        set_base(static_cast<ProgressWheel*>(nullptr), WidgetKind::ProgressWheel);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Spinner
        set_base(static_cast<Spinner*>(nullptr), WidgetKind::Spinner);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ScrollContainer
        set_base(static_cast<ScrollContainer*>(nullptr), WidgetKind::ScrollContainer);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ScrollBar
        set_base(static_cast<ScrollBar*>(nullptr), WidgetKind::ScrollBar);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_SegmentedControl
        set_base(static_cast<SegmentedControl*>(nullptr), WidgetKind::SegmentedControl);
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
#if CHARM_VIVID_ENABLE_WIDGET_RadioGroup
        set_base(static_cast<RadioGroup*>(nullptr), WidgetKind::RadioGroup);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextInput
        set_base(static_cast<TextInput*>(nullptr), WidgetKind::TextInput);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextArea
        set_base(static_cast<TextArea*>(nullptr), WidgetKind::TextArea);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextBox
        set_base(static_cast<TextBox*>(nullptr), WidgetKind::TextBox);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_NumberInput
        set_base(static_cast<NumberInput*>(nullptr), WidgetKind::NumberInput);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_NumberList
        set_base(static_cast<NumberList*>(nullptr), WidgetKind::NumberList);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ToggleGroup
        set_base(static_cast<ToggleGroup*>(nullptr), WidgetKind::ToggleGroup);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TableView
        set_base(static_cast<TableView*>(nullptr), WidgetKind::TableView);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TreeView
        set_base(static_cast<TreeView*>(nullptr), WidgetKind::TreeView);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Dropdown
        set_base(static_cast<Dropdown*>(nullptr), WidgetKind::Dropdown);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Menu
        set_base(static_cast<Menu*>(nullptr), WidgetKind::Menu);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_MenuItem
        set_base(static_cast<MenuItem*>(nullptr), WidgetKind::MenuItem);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TabView
        set_base(static_cast<TabView*>(nullptr), WidgetKind::TabView);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Roller
        set_base(static_cast<Roller*>(nullptr), WidgetKind::Roller);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Stepper
        set_base(static_cast<Stepper*>(nullptr), WidgetKind::Stepper);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Timeline
        set_base(static_cast<Timeline*>(nullptr), WidgetKind::Timeline);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_PerfOverlay
        set_base(static_cast<PerfOverlay*>(nullptr), WidgetKind::PerfOverlay);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_PopupLayer
        set_base(static_cast<PopupLayer*>(nullptr), WidgetKind::PopupLayer);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ModalDialog
        set_base(static_cast<ModalDialog*>(nullptr), WidgetKind::ModalDialog);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_MessageBox
        set_base(static_cast<MessageBox*>(nullptr), WidgetKind::MessageBox);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_PrimitivesCanvas
        set_base(static_cast<PrimitivesCanvas*>(nullptr), WidgetKind::PrimitivesCanvas);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_FoldablePanel
        set_base(static_cast<FoldablePanel*>(nullptr), WidgetKind::FoldablePanel);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_CloudyGlass
        set_base(static_cast<CloudyGlass*>(nullptr), WidgetKind::CloudyGlass);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_DynamicNebula
        set_base(static_cast<DynamicNebula*>(nullptr), WidgetKind::DynamicNebula);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_CrtScreen
        set_base(static_cast<CrtScreen*>(nullptr), WidgetKind::CrtScreen);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_SpectrumView
        set_base(static_cast<SpectrumView*>(nullptr), WidgetKind::SpectrumView);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_SpinningWheel
        set_base(static_cast<SpinningWheel*>(nullptr), WidgetKind::SpinningWheel);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_SpinZoomWidget
        set_base(static_cast<SpinZoomWidget*>(nullptr), WidgetKind::SpinZoomWidget);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_MeterPointer
        set_base(static_cast<MeterPointer*>(nullptr), WidgetKind::MeterPointer);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Arc
        set_base(static_cast<Arc*>(nullptr), WidgetKind::Arc);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Bar
        set_base(static_cast<Bar*>(nullptr), WidgetKind::Bar);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Dial
        set_base(static_cast<Dial*>(nullptr), WidgetKind::Dial);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Image
        set_base(static_cast<Image*>(nullptr), WidgetKind::Image);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ImageBox
        set_base(static_cast<ImageBox*>(nullptr), WidgetKind::ImageBox);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Led
        set_base(static_cast<Led*>(nullptr), WidgetKind::Led);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_BusyWheel
        set_base(static_cast<BusyWheel*>(nullptr), WidgetKind::BusyWheel);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ConsoleBox
        set_base(static_cast<ConsoleBox*>(nullptr), WidgetKind::ConsoleBox);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_BatteryGauge
        set_base(static_cast<BatteryGauge*>(nullptr), WidgetKind::BatteryGauge);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_BatteryGasGauge
        set_base(static_cast<BatteryGasGauge*>(nullptr), WidgetKind::BatteryGasGauge);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Histogram
        set_base(static_cast<Histogram*>(nullptr), WidgetKind::Histogram);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_HistogramView
        set_base(static_cast<HistogramView*>(nullptr), WidgetKind::HistogramView);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Chart
        set_base(static_cast<Chart*>(nullptr), WidgetKind::Chart);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Waveform
        set_base(static_cast<Waveform*>(nullptr), WidgetKind::Waveform);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_WaveformView
        set_base(static_cast<WaveformView*>(nullptr), WidgetKind::WaveformView);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Gauge
        set_base(static_cast<Gauge*>(nullptr), WidgetKind::Gauge);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_RingIndication
        set_base(static_cast<RingIndication*>(nullptr), WidgetKind::RingIndication);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_RichText
        set_base(static_cast<RichText*>(nullptr), WidgetKind::RichText);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_CodeBlock
        set_base(static_cast<CodeBlock*>(nullptr), WidgetKind::CodeBlock);
#endif

        sheet.notify_base_style_changed();
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
    bool has_icon_list{false};
    Style icon_list{};
    bool has_text_list{false};
    Style text_list{};
    bool has_text_tracking_list{false};
    Style text_tracking_list{};
    bool has_progress{false};
    Style progress{};
    bool has_progress_bar_simple{false};
    Style progress_bar_simple{};
    bool has_progress_bar_round{false};
    Style progress_bar_round{};
    bool has_progress_bar_drill{false};
    Style progress_bar_drill{};
    bool has_progress_flowing{false};
    Style progress_flowing{};
    bool has_progress_wheel{false};
    Style progress_wheel{};
    bool has_spinner{false};
    Style spinner{};
    bool has_scroll_container{false};
    Style scroll_container{};
    bool has_scroll_bar{false};
    Style scroll_bar{};
    bool has_segmented_control{false};
    Style segmented_control{};
    bool has_slider{false};
    Style slider{};
    bool has_switch{false};
    Style switcher{};
    bool has_radio{false};
    Style radio{};
    bool has_radio_group{false};
    Style radio_group{};
    bool has_text_input{false};
    Style text_input{};
    bool has_text_area{false};
    Style text_area{};
    bool has_text_box{false};
    Style text_box{};
    bool has_number_input{false};
    Style number_input{};
    bool has_number_list{false};
    Style number_list{};
    bool has_toggle_group{false};
    Style toggle_group{};
    bool has_table_view{false};
    Style table_view{};
    bool has_tree_view{false};
    Style tree_view{};
    bool has_dropdown{false};
    Style dropdown{};
    bool has_menu{false};
    Style menu{};
    bool has_menu_item{false};
    Style menu_item{};
    bool has_tab_view{false};
    Style tab_view{};
    bool has_roller{false};
    Style roller{};
    bool has_stepper{false};
    Style stepper{};
    bool has_perf_overlay{false};
    Style perf_overlay{};
    bool has_foldable_panel{false};
    Style foldable_panel{};
    bool has_cloudy_glass{false};
    Style cloudy_glass{};
    bool has_dynamic_nebula{false};
    Style dynamic_nebula{};
    bool has_crt_screen{false};
    Style crt_screen{};
    bool has_spectrum_view{false};
    Style spectrum_view{};
    bool has_battery_gasgauge{false};
    Style battery_gasgauge{};
    bool has_histogram{false};
    Style histogram{};
    bool has_histogram_view{false};
    Style histogram_view{};
    bool has_chart{false};
    Style chart{};
    bool has_waveform{false};
    Style waveform{};
    bool has_waveform_view{false};
    Style waveform_view{};
    bool has_gauge{false};
    Style gauge{};
    bool has_ring_indication{false};
    Style ring_indication{};
    bool has_busy_wheel{false};
    Style busy_wheel{};
    bool has_console_box{false};
    Style console_box{};
};

export
inline void apply_theme_preset(const ThemePreset& preset) noexcept {
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
#if CHARM_VIVID_ENABLE_WIDGET_ListItem
    if (preset.has_list_item) theme.set<ListItem>(preset.list_item);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_List
    if (preset.has_list) theme.set<List>(preset.list);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_IconList
    if (preset.has_icon_list) theme.set<IconList>(preset.icon_list);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextList
    if (preset.has_text_list) theme.set<TextList>(preset.text_list);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextTrackingList
    if (preset.has_text_tracking_list) theme.set<TextTrackingList>(preset.text_tracking_list);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Progress
    if (preset.has_progress) theme.set<Progress>(preset.progress);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressBarSimple
    if (preset.has_progress_bar_simple) theme.set<ProgressBarSimple>(preset.progress_bar_simple);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressBarRound
    if (preset.has_progress_bar_round) theme.set<ProgressBarRound>(preset.progress_bar_round);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressBarDrill
    if (preset.has_progress_bar_drill) theme.set<ProgressBarDrill>(preset.progress_bar_drill);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressFlowing
    if (preset.has_progress_flowing) theme.set<ProgressFlowing>(preset.progress_flowing);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressWheel
    if (preset.has_progress_wheel) theme.set<ProgressWheel>(preset.progress_wheel);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Spinner
    if (preset.has_spinner) theme.set<Spinner>(preset.spinner);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ScrollContainer
    if (preset.has_scroll_container) theme.set<ScrollContainer>(preset.scroll_container);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ScrollBar
    if (preset.has_scroll_bar) theme.set<ScrollBar>(preset.scroll_bar);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_SegmentedControl
    if (preset.has_segmented_control) theme.set<SegmentedControl>(preset.segmented_control);
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
#if CHARM_VIVID_ENABLE_WIDGET_RadioGroup
    if (preset.has_radio_group) theme.set<RadioGroup>(preset.radio_group);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextInput
    if (preset.has_text_input) theme.set<TextInput>(preset.text_input);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextArea
    if (preset.has_text_area) theme.set<TextArea>(preset.text_area);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextBox
    if (preset.has_text_box) theme.set<TextBox>(preset.text_box);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_NumberInput
    if (preset.has_number_input) theme.set<NumberInput>(preset.number_input);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_NumberList
    if (preset.has_number_list) theme.set<NumberList>(preset.number_list);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ToggleGroup
    if (preset.has_toggle_group) theme.set<ToggleGroup>(preset.toggle_group);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TableView
    if (preset.has_table_view) theme.set<TableView>(preset.table_view);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TreeView
    if (preset.has_tree_view) theme.set<TreeView>(preset.tree_view);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Dropdown
    if (preset.has_dropdown) theme.set<Dropdown>(preset.dropdown);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Menu
    if (preset.has_menu) theme.set<Menu>(preset.menu);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_MenuItem
    if (preset.has_menu_item) theme.set<MenuItem>(preset.menu_item);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TabView
    if (preset.has_tab_view) theme.set<TabView>(preset.tab_view);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Roller
    if (preset.has_roller) theme.set<Roller>(preset.roller);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Stepper
    if (preset.has_stepper) theme.set<Stepper>(preset.stepper);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_PerfOverlay
    if (preset.has_perf_overlay) theme.set<PerfOverlay>(preset.perf_overlay);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_FoldablePanel
    if (preset.has_foldable_panel) theme.set<FoldablePanel>(preset.foldable_panel);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_CloudyGlass
    if (preset.has_cloudy_glass) theme.set<CloudyGlass>(preset.cloudy_glass);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_DynamicNebula
    if (preset.has_dynamic_nebula) theme.set<DynamicNebula>(preset.dynamic_nebula);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_CrtScreen
    if (preset.has_crt_screen) theme.set<CrtScreen>(preset.crt_screen);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_SpectrumView
    if (preset.has_spectrum_view) theme.set<SpectrumView>(preset.spectrum_view);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_BatteryGasGauge
    if (preset.has_battery_gasgauge) theme.set<BatteryGasGauge>(preset.battery_gasgauge);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Histogram
    if (preset.has_histogram) theme.set<Histogram>(preset.histogram);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_HistogramView
    if (preset.has_histogram_view) theme.set<HistogramView>(preset.histogram_view);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Chart
    if (preset.has_chart) theme.set<Chart>(preset.chart);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Waveform
    if (preset.has_waveform) theme.set<Waveform>(preset.waveform);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_WaveformView
    if (preset.has_waveform_view) theme.set<WaveformView>(preset.waveform_view);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Gauge
    if (preset.has_gauge) theme.set<Gauge>(preset.gauge);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_RingIndication
    if (preset.has_ring_indication) theme.set<RingIndication>(preset.ring_indication);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_BusyWheel
    if (preset.has_busy_wheel) theme.set<BusyWheel>(preset.busy_wheel);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ConsoleBox
    if (preset.has_console_box) theme.set<ConsoleBox>(preset.console_box);
#endif
    sync_style_sheet_bases();
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
#if CHARM_VIVID_ENABLE_WIDGET_Label
    preset.label = theme.get<Label>();
#endif
    apply_base(preset.label);
    preset.has_button = true;
#if CHARM_VIVID_ENABLE_WIDGET_Button
    preset.button = theme.get<Button>();
#endif
    apply_base(preset.button);
    preset.has_checkbox = true;
#if CHARM_VIVID_ENABLE_WIDGET_Checkbox
    preset.checkbox = theme.get<Checkbox>();
#endif
    apply_base(preset.checkbox);
    preset.has_list_view = true;
#if CHARM_VIVID_ENABLE_WIDGET_ListView
    preset.list_view = theme.get<ListView>();
#endif
    apply_base(preset.list_view);
    preset.has_list_item = true;
#if CHARM_VIVID_ENABLE_WIDGET_ListItem
    preset.list_item = theme.get<ListItem>();
#endif
    apply_base(preset.list_item);
    preset.has_list = true;
#if CHARM_VIVID_ENABLE_WIDGET_List
    preset.list = theme.get<List>();
#endif
    apply_base(preset.list);
    preset.has_icon_list = true;
#if CHARM_VIVID_ENABLE_WIDGET_IconList
    preset.icon_list = theme.get<IconList>();
#endif
    apply_base(preset.icon_list);
    preset.has_text_list = true;
#if CHARM_VIVID_ENABLE_WIDGET_TextList
    preset.text_list = theme.get<TextList>();
#endif
    apply_base(preset.text_list);
    preset.has_text_tracking_list = true;
#if CHARM_VIVID_ENABLE_WIDGET_TextTrackingList
    preset.text_tracking_list = theme.get<TextTrackingList>();
#endif
    apply_base(preset.text_tracking_list);
    preset.has_progress = true;
#if CHARM_VIVID_ENABLE_WIDGET_Progress
    preset.progress = theme.get<Progress>();
#endif
    apply_base(preset.progress);
    preset.has_progress_bar_simple = true;
#if CHARM_VIVID_ENABLE_WIDGET_ProgressBarSimple
    preset.progress_bar_simple = theme.get<ProgressBarSimple>();
#endif
    apply_base(preset.progress_bar_simple);
    preset.has_progress_bar_round = true;
#if CHARM_VIVID_ENABLE_WIDGET_ProgressBarRound
    preset.progress_bar_round = theme.get<ProgressBarRound>();
#endif
    apply_base(preset.progress_bar_round);
    preset.has_progress_bar_drill = true;
#if CHARM_VIVID_ENABLE_WIDGET_ProgressBarDrill
    preset.progress_bar_drill = theme.get<ProgressBarDrill>();
#endif
    apply_base(preset.progress_bar_drill);
    preset.has_progress_flowing = true;
#if CHARM_VIVID_ENABLE_WIDGET_ProgressFlowing
    preset.progress_flowing = theme.get<ProgressFlowing>();
#endif
    apply_base(preset.progress_flowing);
    preset.has_progress_wheel = true;
#if CHARM_VIVID_ENABLE_WIDGET_ProgressWheel
    preset.progress_wheel = theme.get<ProgressWheel>();
#endif
    apply_base(preset.progress_wheel);
    preset.has_spinner = true;
#if CHARM_VIVID_ENABLE_WIDGET_Spinner
    preset.spinner = theme.get<Spinner>();
#endif
    apply_base(preset.spinner);
    preset.has_scroll_container = true;
#if CHARM_VIVID_ENABLE_WIDGET_ScrollContainer
    preset.scroll_container = theme.get<ScrollContainer>();
#endif
    apply_base(preset.scroll_container);
    preset.has_scroll_bar = true;
#if CHARM_VIVID_ENABLE_WIDGET_ScrollBar
    preset.scroll_bar = theme.get<ScrollBar>();
#endif
    apply_base(preset.scroll_bar);
    preset.has_segmented_control = true;
#if CHARM_VIVID_ENABLE_WIDGET_SegmentedControl
    preset.segmented_control = theme.get<SegmentedControl>();
#endif
    apply_base(preset.segmented_control);
    preset.has_slider = true;
#if CHARM_VIVID_ENABLE_WIDGET_Slider
    preset.slider = theme.get<Slider>();
#endif
    apply_base(preset.slider);
    preset.has_switch = true;
#if CHARM_VIVID_ENABLE_WIDGET_Switch
    preset.switcher = theme.get<Switch>();
#endif
    apply_base(preset.switcher);
    preset.has_radio = true;
#if CHARM_VIVID_ENABLE_WIDGET_Radio
    preset.radio = theme.get<Radio>();
#endif
    apply_base(preset.radio);
    preset.has_radio_group = true;
#if CHARM_VIVID_ENABLE_WIDGET_RadioGroup
    preset.radio_group = theme.get<RadioGroup>();
#endif
    apply_base(preset.radio_group);
    preset.has_text_input = true;
#if CHARM_VIVID_ENABLE_WIDGET_TextInput
    preset.text_input = theme.get<TextInput>();
#endif
    apply_base(preset.text_input);
    preset.has_text_area = true;
#if CHARM_VIVID_ENABLE_WIDGET_TextArea
    preset.text_area = theme.get<TextArea>();
#endif
    apply_base(preset.text_area);
    preset.has_text_box = true;
#if CHARM_VIVID_ENABLE_WIDGET_TextBox
    preset.text_box = theme.get<TextBox>();
#endif
    apply_base(preset.text_box);
    preset.has_number_input = true;
#if CHARM_VIVID_ENABLE_WIDGET_NumberInput
    preset.number_input = theme.get<NumberInput>();
#endif
    apply_base(preset.number_input);
    preset.has_number_list = true;
#if CHARM_VIVID_ENABLE_WIDGET_NumberList
    preset.number_list = theme.get<NumberList>();
#endif
    apply_base(preset.number_list);
    preset.has_toggle_group = true;
#if CHARM_VIVID_ENABLE_WIDGET_ToggleGroup
    preset.toggle_group = theme.get<ToggleGroup>();
#endif
    apply_base(preset.toggle_group);
    preset.has_table_view = true;
#if CHARM_VIVID_ENABLE_WIDGET_TableView
    preset.table_view = theme.get<TableView>();
#endif
    apply_base(preset.table_view);
    preset.has_tree_view = true;
#if CHARM_VIVID_ENABLE_WIDGET_TreeView
    preset.tree_view = theme.get<TreeView>();
#endif
    apply_base(preset.tree_view);
    preset.has_dropdown = true;
#if CHARM_VIVID_ENABLE_WIDGET_Dropdown
    preset.dropdown = theme.get<Dropdown>();
#endif
    apply_base(preset.dropdown);
    preset.has_menu = true;
#if CHARM_VIVID_ENABLE_WIDGET_Menu
    preset.menu = theme.get<Menu>();
#endif
    apply_base(preset.menu);
    preset.has_menu_item = true;
#if CHARM_VIVID_ENABLE_WIDGET_MenuItem
    preset.menu_item = theme.get<MenuItem>();
#endif
    apply_base(preset.menu_item);
    preset.has_tab_view = true;
#if CHARM_VIVID_ENABLE_WIDGET_TabView
    preset.tab_view = theme.get<TabView>();
#endif
    apply_base(preset.tab_view);
    preset.has_roller = true;
#if CHARM_VIVID_ENABLE_WIDGET_Roller
    preset.roller = theme.get<Roller>();
#endif
    apply_base(preset.roller);
    preset.has_stepper = true;
#if CHARM_VIVID_ENABLE_WIDGET_Stepper
    preset.stepper = theme.get<Stepper>();
#endif
    apply_base(preset.stepper);
    preset.has_perf_overlay = true;
#if CHARM_VIVID_ENABLE_WIDGET_PerfOverlay
    preset.perf_overlay = theme.get<PerfOverlay>();
#endif
    apply_base(preset.perf_overlay);
    preset.has_foldable_panel = true;
#if CHARM_VIVID_ENABLE_WIDGET_FoldablePanel
    preset.foldable_panel = theme.get<FoldablePanel>();
#endif
    apply_base(preset.foldable_panel);
    preset.has_cloudy_glass = true;
#if CHARM_VIVID_ENABLE_WIDGET_CloudyGlass
    preset.cloudy_glass = theme.get<CloudyGlass>();
#endif
    apply_base(preset.cloudy_glass);
    preset.has_dynamic_nebula = true;
#if CHARM_VIVID_ENABLE_WIDGET_DynamicNebula
    preset.dynamic_nebula = theme.get<DynamicNebula>();
#endif
    apply_base(preset.dynamic_nebula);
    preset.has_crt_screen = true;
#if CHARM_VIVID_ENABLE_WIDGET_CrtScreen
    preset.crt_screen = theme.get<CrtScreen>();
#endif
    apply_base(preset.crt_screen);
    preset.has_spectrum_view = true;
#if CHARM_VIVID_ENABLE_WIDGET_SpectrumView
    preset.spectrum_view = theme.get<SpectrumView>();
#endif
    apply_base(preset.spectrum_view);
    preset.has_battery_gasgauge = true;
#if CHARM_VIVID_ENABLE_WIDGET_BatteryGasGauge
    preset.battery_gasgauge = theme.get<BatteryGasGauge>();
#endif
    apply_base(preset.battery_gasgauge);
    preset.has_histogram = true;
#if CHARM_VIVID_ENABLE_WIDGET_Histogram
    preset.histogram = theme.get<Histogram>();
#endif
    apply_base(preset.histogram);
    preset.has_histogram_view = true;
#if CHARM_VIVID_ENABLE_WIDGET_HistogramView
    preset.histogram_view = theme.get<HistogramView>();
#endif
    apply_base(preset.histogram_view);
    preset.has_chart = true;
#if CHARM_VIVID_ENABLE_WIDGET_Chart
    preset.chart = theme.get<Chart>();
#endif
    apply_base(preset.chart);
    preset.has_waveform = true;
#if CHARM_VIVID_ENABLE_WIDGET_Waveform
    preset.waveform = theme.get<Waveform>();
#endif
    apply_base(preset.waveform);
    preset.has_waveform_view = true;
#if CHARM_VIVID_ENABLE_WIDGET_WaveformView
    preset.waveform_view = theme.get<WaveformView>();
#endif
    apply_base(preset.waveform_view);
    preset.has_gauge = true;
#if CHARM_VIVID_ENABLE_WIDGET_Gauge
    preset.gauge = theme.get<Gauge>();
#endif
    apply_base(preset.gauge);
    preset.has_ring_indication = true;
#if CHARM_VIVID_ENABLE_WIDGET_RingIndication
    preset.ring_indication = theme.get<RingIndication>();
#endif
    apply_base(preset.ring_indication);
    preset.has_busy_wheel = true;
#if CHARM_VIVID_ENABLE_WIDGET_BusyWheel
    preset.busy_wheel = theme.get<BusyWheel>();
#endif
    apply_base(preset.busy_wheel);
    preset.has_console_box = true;
#if CHARM_VIVID_ENABLE_WIDGET_ConsoleBox
    preset.console_box = theme.get<ConsoleBox>();
#endif
    apply_base(preset.console_box);

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
#if CHARM_VIVID_ENABLE_WIDGET_ListItem
    apply_widget(static_cast<ListItem*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_List
    apply_widget(static_cast<List*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_IconList
    apply_widget(static_cast<IconList*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextList
    apply_widget(static_cast<TextList*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextTrackingList
    apply_widget(static_cast<TextTrackingList*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Progress
    apply_widget(static_cast<Progress*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressBarSimple
    apply_widget(static_cast<ProgressBarSimple*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressBarRound
    apply_widget(static_cast<ProgressBarRound*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressBarDrill
    apply_widget(static_cast<ProgressBarDrill*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressFlowing
    apply_widget(static_cast<ProgressBarFlowing*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressFlowing
    apply_widget(static_cast<ProgressFlowing*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressWheel
    apply_widget(static_cast<ProgressWheel*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Spinner
    apply_widget(static_cast<Spinner*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ScrollContainer
    apply_widget(static_cast<ScrollContainer*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ScrollBar
    apply_widget(static_cast<ScrollBar*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_SegmentedControl
    apply_widget(static_cast<SegmentedControl*>(nullptr));
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
#if CHARM_VIVID_ENABLE_WIDGET_RadioGroup
    apply_widget(static_cast<RadioGroup*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextInput
    apply_widget(static_cast<TextInput*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextArea
    apply_widget(static_cast<TextArea*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextBox
    apply_widget(static_cast<TextBox*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_NumberInput
    apply_widget(static_cast<NumberInput*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_NumberList
    apply_widget(static_cast<NumberList*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ToggleGroup
    apply_widget(static_cast<ToggleGroup*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TableView
    apply_widget(static_cast<TableView*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TreeView
    apply_widget(static_cast<TreeView*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Dropdown
    apply_widget(static_cast<Dropdown*>(nullptr));
#endif
    apply_widget(static_cast<DropdownPopup*>(nullptr));
#if CHARM_VIVID_ENABLE_WIDGET_Menu
    apply_widget(static_cast<Menu*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_MenuItem
    apply_widget(static_cast<MenuItem*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TabView
    apply_widget(static_cast<TabView*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Roller
    apply_widget(static_cast<Roller*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Stepper
    apply_widget(static_cast<Stepper*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Timeline
    apply_widget(static_cast<Timeline*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_PerfOverlay
    apply_widget(static_cast<PerfOverlay*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_PopupLayer
    apply_widget(static_cast<PopupLayer*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ModalDialog
    apply_widget(static_cast<ModalDialog*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_MessageBox
    apply_widget(static_cast<MessageBox*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_PrimitivesCanvas
    apply_widget(static_cast<PrimitivesCanvas*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_FoldablePanel
    apply_widget(static_cast<FoldablePanel*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_CloudyGlass
    apply_widget(static_cast<CloudyGlass*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_DynamicNebula
    apply_widget(static_cast<DynamicNebula*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_CrtScreen
    apply_widget(static_cast<CrtScreen*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_SpectrumView
    apply_widget(static_cast<SpectrumView*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_SpinningWheel
    apply_widget(static_cast<SpinningWheel*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_SpinZoomWidget
    apply_widget(static_cast<SpinZoomWidget*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_MeterPointer
    apply_widget(static_cast<MeterPointer*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Arc
    apply_widget(static_cast<Arc*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Bar
    apply_widget(static_cast<Bar*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Dial
    apply_widget(static_cast<Dial*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Image
    apply_widget(static_cast<Image*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ImageBox
    apply_widget(static_cast<ImageBox*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Led
    apply_widget(static_cast<Led*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_BusyWheel
    apply_widget(static_cast<BusyWheel*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ConsoleBox
    apply_widget(static_cast<ConsoleBox*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_BatteryGauge
    apply_widget(static_cast<BatteryGauge*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_BatteryGasGauge
    apply_widget(static_cast<BatteryGasGauge*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Histogram
    apply_widget(static_cast<Histogram*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_HistogramView
    apply_widget(static_cast<HistogramView*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Chart
    apply_widget(static_cast<Chart*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Waveform
    apply_widget(static_cast<Waveform*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_WaveformView
    apply_widget(static_cast<WaveformView*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Gauge
    apply_widget(static_cast<Gauge*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_RingIndication
    apply_widget(static_cast<RingIndication*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_RichText
    apply_widget(static_cast<RichText*>(nullptr));
#endif
#if CHARM_VIVID_ENABLE_WIDGET_CodeBlock
    apply_widget(static_cast<CodeBlock*>(nullptr));
#endif
    sync_style_sheet_bases();
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
    auto& theme = Theme::instance();

    constexpr rgba kSystemBlue{0, 122, 255, 255};
    constexpr rgba kSystemBlueHover{0, 112, 245, 255};
    constexpr rgba kSystemBluePressed{0, 102, 235, 255};
    constexpr rgba kSystemGreen{52, 199, 89, 255};
    constexpr rgba kSurface{242, 242, 247, 255};
    constexpr rgba kSurfaceElevated{255, 255, 255, 255};
    constexpr rgba kBorder{199, 199, 204, 255};
    constexpr rgba kText{28, 28, 30, 255};
    constexpr rgba kTrack{209, 209, 214, 255};

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

    ThemePreset preset{};
    preset.has_label = true;
#if CHARM_VIVID_ENABLE_WIDGET_Label
    preset.label = theme.get<Label>();
#endif
    preset.label.colors.font_color = kText;

    preset.has_button = true;
#if CHARM_VIVID_ENABLE_WIDGET_Button
    preset.button = theme.get<Button>();
#endif
    preset.button.colors.bg_color = kSystemBlue;
    preset.button.colors.bg_hover = kSystemBlueHover;
    preset.button.colors.bg_pressed = kSystemBluePressed;
    preset.button.colors.border_color = kSystemBlue;
    preset.button.colors.border_hover = kSystemBlueHover;
    preset.button.colors.border_pressed = kSystemBluePressed;
    preset.button.colors.border_focus = kSystemBlue;
    preset.button.colors.font_color = kSurfaceElevated;
    preset.button.metrics.corner_radius = 10;
    preset.button.metrics.padding = 8;

    preset.has_segmented_control = true;
#if CHARM_VIVID_ENABLE_WIDGET_SegmentedControl
    preset.segmented_control = theme.get<SegmentedControl>();
#endif
    preset.segmented_control.colors.bg_color = kSurface;
    preset.segmented_control.colors.border_color = kBorder;
    preset.segmented_control.colors.bg_pressed = kSurfaceElevated;
    preset.segmented_control.colors.border_pressed = kBorder;
    preset.segmented_control.colors.font_color = kText;
    preset.segmented_control.metrics.corner_radius = 10;
    preset.segmented_control.metrics.padding = 4;

    preset.has_slider = true;
#if CHARM_VIVID_ENABLE_WIDGET_Slider
    preset.slider = theme.get<Slider>();
#endif
    preset.slider.colors.bg_color = kSystemBlue;
    preset.slider.colors.bg_hover = kSystemBlueHover;
    preset.slider.colors.bg_pressed = kSystemBluePressed;
    preset.slider.colors.border_color = kTrack;
    preset.slider.colors.border_hover = kTrack;
    preset.slider.colors.border_pressed = kTrack;
    preset.slider.colors.font_color = kSurfaceElevated;
    preset.slider.colors.accent_color = kSystemBlue;
    preset.slider.colors.accent_hover = kSystemBlueHover;
    preset.slider.colors.accent_pressed = kSystemBluePressed;
    preset.slider.colors.on_accent = kSurfaceElevated;
    preset.slider.colors.border_focus = kSystemBlue;
    preset.slider.metrics.padding = 8;

    preset.has_switch = true;
#if CHARM_VIVID_ENABLE_WIDGET_Switch
    preset.switcher = theme.get<Switch>();
#endif
    preset.switcher.colors.bg_color = kSurface;
    preset.switcher.colors.border_color = kBorder;
    preset.switcher.colors.bg_pressed = kSystemGreen;
    preset.switcher.colors.border_pressed = kSystemGreen;
    preset.switcher.colors.accent_color = kSystemGreen;
    preset.switcher.colors.on_accent = kSurfaceElevated;
    preset.switcher.colors.border_focus = kSystemBlue;
    preset.switcher.metrics.corner_radius = 10;
    preset.switcher.metrics.padding = 4;

    preset.has_list_view = true;
#if CHARM_VIVID_ENABLE_WIDGET_ListView
    preset.list_view = theme.get<ListView>();
#endif
    preset.list_view.metrics.scrollbar_margin = 2;
    preset.list_view.metrics.scrollbar_thumb_min = 12;

    preset.has_scroll_container = true;
#if CHARM_VIVID_ENABLE_WIDGET_ScrollContainer
    preset.scroll_container = theme.get<ScrollContainer>();
#endif
    preset.scroll_container.metrics.scrollbar_margin = 2;
    preset.scroll_container.metrics.scrollbar_thumb_min = 12;

    apply_theme_preset(preset);
}

export
inline void apply_material3_light_preset() noexcept {
    auto& theme = Theme::instance();

    constexpr rgba kPrimary{103, 80, 164, 255};
    constexpr rgba kPrimaryHover{96, 74, 154, 255};
    constexpr rgba kPrimaryPressed{88, 68, 142, 255};
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

    ThemePreset preset{};
    preset.has_label = true;
#if CHARM_VIVID_ENABLE_WIDGET_Label
    preset.label = theme.get<Label>();
#endif
    preset.label.colors.font_color = kOnSurface;

    preset.has_button = true;
#if CHARM_VIVID_ENABLE_WIDGET_Button
    preset.button = theme.get<Button>();
#endif
    preset.button.colors.bg_color = kPrimary;
    preset.button.colors.bg_hover = kPrimaryHover;
    preset.button.colors.bg_pressed = kPrimaryPressed;
    preset.button.colors.border_color = kPrimary;
    preset.button.colors.border_hover = kPrimaryHover;
    preset.button.colors.border_pressed = kPrimaryPressed;
    preset.button.colors.border_focus = kPrimary;
    preset.button.colors.font_color = kOnPrimary;
    preset.button.metrics.corner_radius = 12;
    preset.button.metrics.padding = 10;

    preset.has_segmented_control = true;
#if CHARM_VIVID_ENABLE_WIDGET_SegmentedControl
    preset.segmented_control = theme.get<SegmentedControl>();
#endif
    preset.segmented_control.colors.bg_color = kSurfaceVariant;
    preset.segmented_control.colors.border_color = kOutline;
    preset.segmented_control.colors.bg_pressed = kSurface;
    preset.segmented_control.colors.border_pressed = kOutline;
    preset.segmented_control.colors.font_color = kOnSurface;
    preset.segmented_control.metrics.corner_radius = 12;
    preset.segmented_control.metrics.padding = 4;

    preset.has_slider = true;
#if CHARM_VIVID_ENABLE_WIDGET_Slider
    preset.slider = theme.get<Slider>();
#endif
    preset.slider.colors.bg_color = kPrimary;
    preset.slider.colors.bg_hover = kPrimaryHover;
    preset.slider.colors.bg_pressed = kPrimaryPressed;
    preset.slider.colors.border_color = kOutline;
    preset.slider.colors.border_hover = kOutline;
    preset.slider.colors.border_pressed = kOutline;
    preset.slider.colors.font_color = kOnPrimary;
    preset.slider.colors.accent_color = kPrimary;
    preset.slider.colors.accent_hover = kPrimaryHover;
    preset.slider.colors.accent_pressed = kPrimaryPressed;
    preset.slider.colors.on_accent = kOnPrimary;
    preset.slider.colors.border_focus = kPrimary;
    preset.slider.metrics.padding = 8;

    preset.has_switch = true;
#if CHARM_VIVID_ENABLE_WIDGET_Switch
    preset.switcher = theme.get<Switch>();
#endif
    preset.switcher.colors.bg_color = kSurfaceVariant;
    preset.switcher.colors.border_color = kOutline;
    preset.switcher.colors.bg_pressed = kPrimary;
    preset.switcher.colors.border_pressed = kPrimary;
    preset.switcher.colors.accent_color = kPrimary;
    preset.switcher.colors.on_accent = kOnPrimary;
    preset.switcher.colors.border_focus = kPrimary;
    preset.switcher.colors.font_color = kOnPrimary;
    preset.switcher.metrics.corner_radius = 12;
    preset.switcher.metrics.padding = 4;

    preset.has_list_view = true;
#if CHARM_VIVID_ENABLE_WIDGET_ListView
    preset.list_view = theme.get<ListView>();
#endif
    preset.list_view.metrics.scrollbar_margin = 2;
    preset.list_view.metrics.scrollbar_thumb_min = 12;

    preset.has_scroll_container = true;
#if CHARM_VIVID_ENABLE_WIDGET_ScrollContainer
    preset.scroll_container = theme.get<ScrollContainer>();
#endif
    preset.scroll_container.metrics.scrollbar_margin = 2;
    preset.scroll_container.metrics.scrollbar_thumb_min = 12;

    // Tonal button feel via hover/pressed roles.
    StyleSheet::instance().add_role_rule(
        StyleSelector{WidgetKind::Button, 0},
        StyleRolePatch{
            .has_bg_hover = true,
            .has_bg_pressed = true,
            .bg_hover = StyleRole::SurfaceVariant,
            .bg_pressed = StyleRole::AccentPressed,
        });

    apply_theme_preset(preset);
}

export
inline void apply_lvgl_default_preset() noexcept {
    auto& theme = Theme::instance();

    constexpr rgba kLvBg{240, 240, 240, 255};
    constexpr rgba kLvBgHover{224, 224, 224, 255};
    constexpr rgba kLvBgPressed{200, 200, 200, 255};
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

    ThemePreset preset{};
    preset.has_label = true;
#if CHARM_VIVID_ENABLE_WIDGET_Label
    preset.label = theme.get<Label>();
#endif
    preset.label.colors.font_color = kLvText;

    preset.has_button = true;
#if CHARM_VIVID_ENABLE_WIDGET_Button
    preset.button = theme.get<Button>();
#endif
    preset.button.colors.bg_color = kLvBg;
    preset.button.colors.bg_hover = kLvBgHover;
    preset.button.colors.bg_pressed = kLvBgPressed;
    preset.button.colors.border_color = kLvBorder;
    preset.button.colors.border_hover = kLvBorder;
    preset.button.colors.border_pressed = kLvBorder;
    preset.button.colors.border_focus = kLvBorderFocus;
    preset.button.colors.font_color = kLvText;
    preset.button.metrics.corner_radius = 6;
    preset.button.metrics.padding = 6;

    preset.has_segmented_control = true;
#if CHARM_VIVID_ENABLE_WIDGET_SegmentedControl
    preset.segmented_control = theme.get<SegmentedControl>();
#endif
    preset.segmented_control.colors.bg_color = kLvBg;
    preset.segmented_control.colors.border_color = kLvBorder;
    preset.segmented_control.colors.bg_pressed = kLvBgPressed;
    preset.segmented_control.colors.border_pressed = kLvBorder;
    preset.segmented_control.colors.font_color = kLvText;
    preset.segmented_control.metrics.corner_radius = 6;
    preset.segmented_control.metrics.padding = 4;

    preset.has_slider = true;
#if CHARM_VIVID_ENABLE_WIDGET_Slider
    preset.slider = theme.get<Slider>();
#endif
    preset.slider.colors.bg_color = kLvAccent;
    preset.slider.colors.bg_hover = kLvAccent;
    preset.slider.colors.bg_pressed = kLvAccent;
    preset.slider.colors.border_color = kLvBorder;
    preset.slider.colors.border_hover = kLvBorder;
    preset.slider.colors.border_pressed = kLvBorder;
    preset.slider.colors.font_color = kLvText;
    preset.slider.colors.accent_color = kLvAccent;
    preset.slider.colors.on_accent = kLvText;
    preset.slider.colors.border_focus = kLvBorderFocus;
    preset.slider.metrics.padding = 6;

    preset.has_switch = true;
#if CHARM_VIVID_ENABLE_WIDGET_Switch
    preset.switcher = theme.get<Switch>();
#endif
    preset.switcher.colors.bg_color = kLvBg;
    preset.switcher.colors.border_color = kLvBorder;
    preset.switcher.colors.bg_pressed = kLvAccent;
    preset.switcher.colors.border_pressed = kLvAccent;
    preset.switcher.colors.accent_color = kLvAccent;
    preset.switcher.colors.on_accent = kLvText;
    preset.switcher.colors.border_focus = kLvBorderFocus;
    preset.switcher.metrics.corner_radius = 6;
    preset.switcher.metrics.padding = 4;

    preset.has_list_view = true;
#if CHARM_VIVID_ENABLE_WIDGET_ListView
    preset.list_view = theme.get<ListView>();
#endif
    preset.list_view.metrics.scrollbar_margin = 2;
    preset.list_view.metrics.scrollbar_thumb_min = 12;

    preset.has_scroll_container = true;
#if CHARM_VIVID_ENABLE_WIDGET_ScrollContainer
    preset.scroll_container = theme.get<ScrollContainer>();
#endif
    preset.scroll_container.metrics.scrollbar_margin = 2;
    preset.scroll_container.metrics.scrollbar_thumb_min = 12;

    apply_theme_preset(preset);
}

export
inline void apply_arm2d_demo_preset() noexcept {
    auto& theme = Theme::instance();

    constexpr rgba kPanel{24, 28, 36, 255};
    constexpr rgba kPanelBorder{58, 72, 92, 255};
    constexpr rgba kPanelHover{32, 38, 50, 255};
    constexpr rgba kPanelPressed{36, 44, 58, 255};
    constexpr rgba kAccent{48, 198, 255, 255};
    constexpr rgba kAccentSoft{90, 220, 255, 255};
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

    ThemePreset preset{};
    preset.has_label = true;
#if CHARM_VIVID_ENABLE_WIDGET_Label
    preset.label = theme.get<Label>();
#endif
    preset.label.colors.font_color = kText;

    preset.has_button = true;
#if CHARM_VIVID_ENABLE_WIDGET_Button
    preset.button = theme.get<Button>();
#endif
    preset.button.colors.bg_color = kPanel;
    preset.button.colors.bg_hover = kPanelHover;
    preset.button.colors.bg_pressed = kPanelPressed;
    preset.button.colors.border_color = kPanelBorder;
    preset.button.colors.border_hover = kPanelBorder;
    preset.button.colors.border_pressed = kAccent;
    preset.button.colors.border_focus = kAccent;
    preset.button.colors.font_color = kText;
    preset.button.metrics.corner_radius = 8;
    preset.button.metrics.padding = 6;

    preset.has_progress = true;
#if CHARM_VIVID_ENABLE_WIDGET_Progress
    preset.progress = theme.get<Progress>();
#endif
    preset.progress.colors.bg_color = kPanel;
    preset.progress.colors.border_color = kPanelBorder;
    preset.progress.colors.bg_pressed = kAccent;
    preset.progress.colors.accent_color = kAccent;
    preset.progress.colors.on_accent = kText;
    preset.progress.colors.border_focus = kAccent;
    preset.progress.metrics.corner_radius = 6;
    preset.progress.metrics.padding = 4;

    preset.has_progress_bar_simple = true;
#if CHARM_VIVID_ENABLE_WIDGET_ProgressBarSimple
    preset.progress_bar_simple = theme.get<ProgressBarSimple>();
#endif
    preset.progress_bar_simple.colors.bg_color = kPanel;
    preset.progress_bar_simple.colors.border_color = kPanelBorder;
    preset.progress_bar_simple.colors.bg_pressed = kAccent;
    preset.progress_bar_simple.colors.accent_color = kAccent;
    preset.progress_bar_simple.colors.on_accent = kText;
    preset.progress_bar_simple.colors.border_focus = kAccent;
    preset.progress_bar_simple.metrics.corner_radius = 6;
    preset.progress_bar_simple.metrics.padding = 4;

    preset.has_cloudy_glass = true;
#if CHARM_VIVID_ENABLE_WIDGET_CloudyGlass
    preset.cloudy_glass = theme.get<CloudyGlass>();
#endif
    preset.cloudy_glass.colors.bg_color = kPanel;
    preset.cloudy_glass.colors.border_color = kPanelBorder;
    preset.cloudy_glass.metrics.corner_radius = 10;
    preset.cloudy_glass.metrics.glass_highlight_pos = 18;
    preset.cloudy_glass.metrics.glass_highlight_alpha = 90;
    preset.cloudy_glass.metrics.glass_shadow_alpha = 50;
    preset.cloudy_glass.metrics.glass_opacity_min = 40;
    preset.cloudy_glass.metrics.glass_opacity_max = 190;

    preset.has_dynamic_nebula = true;
#if CHARM_VIVID_ENABLE_WIDGET_DynamicNebula
    preset.dynamic_nebula = theme.get<DynamicNebula>();
#endif
    preset.dynamic_nebula.colors.font_color = kAccentSoft;

    preset.has_busy_wheel = true;
#if CHARM_VIVID_ENABLE_WIDGET_BusyWheel
    preset.busy_wheel = theme.get<BusyWheel>();
#endif
    preset.busy_wheel.colors.font_color = kAccent;

    apply_theme_preset(preset);
}
