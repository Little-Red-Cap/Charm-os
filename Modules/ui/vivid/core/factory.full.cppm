module;
#include <cstdint>
#include <cstddef>
#include <optional>
#include <type_traits>
#include <utility>
#include <new>
export module charm.core.factory.full;

#include "features.hpp"

import charm.core.handle;
import charm.core.object;
import charm.core.container;
import service.handle_pool;
#if CHARM_VIVID_ENABLE_WIDGET_ScrollContainer
import charm.widgets.scroll_container;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Dial
import charm.widgets.dial;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Arc
import charm.widgets.arc;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Image
import charm.widgets.image;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ImageBox
import charm.widgets.image_box;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_BusyWheel
import charm.widgets.busy_wheel;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ConsoleBox
import charm.widgets.console_box;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_BatteryGasGauge
import charm.widgets.battery_gasgauge;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Histogram
import charm.widgets.histogram;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressFlowing
import charm.widgets.progress_bar_flowing;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Label
import charm.widgets.label;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Button
import charm.widgets.button;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Checkbox
import charm.widgets.checkbox;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Slider
import charm.widgets.slider;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Switch
import charm.widgets.switcher;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Progress
import charm.widgets.progress;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_List
import charm.widgets.list;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ListView
import charm.widgets.list_view;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_IconList
import charm.widgets.icon_list;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextTrackingList
import charm.widgets.text_tracking_list;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextList
import charm.widgets.text_list;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ModalDialog
import charm.widgets.modal_dialog;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ScrollBar
import charm.widgets.scrollbar;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_SegmentedControl
import charm.widgets.segmented_control;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextArea
import charm.widgets.text_area;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextInput
import charm.widgets.text_input;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_NumberInput
import charm.widgets.number_input;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_NumberList
import charm.widgets.number_list;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ToggleGroup
import charm.widgets.toggle_group;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TableView
import charm.widgets.table_view;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TreeView
import charm.widgets.tree_view;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Dropdown
import charm.widgets.dropdown;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TabView
import charm.widgets.tabview;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Roller
import charm.widgets.roller;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Spinner
import charm.widgets.spinner;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Bar
import charm.widgets.bar;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressBarRound
import charm.widgets.progress_bar_round;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressBarSimple
import charm.widgets.progress_bar_simple;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressBarDrill
import charm.widgets.progress_bar_drill;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_PopupLayer
import charm.widgets.popup_layer;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_PopupLayer
import charm.widgets.popup_layer;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Menu
import charm.widgets.menu;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_MenuItem
import charm.widgets.menu_item;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Radio
import charm.widgets.radio;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_RadioGroup
import charm.widgets.radio_group;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Chart
import charm.widgets.chart;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Waveform
import charm.widgets.waveform;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Gauge
import charm.widgets.gauge;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_MeterPointer
import charm.widgets.meter_pointer;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_PrimitivesCanvas
import charm.widgets.primitives_canvas;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_PerfOverlay
import charm.widgets.perf_overlay;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Stepper
import charm.widgets.stepper;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Timeline
import charm.widgets.timeline;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_RichText
import charm.widgets.rich_text;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_CodeBlock
import charm.widgets.code_block;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressWheel
import charm.widgets.progress_wheel;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_WaveformView
import charm.widgets.waveform_view;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_BatteryGauge
import charm.widgets.battery_gauge;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_HistogramView
import charm.widgets.histogram_view;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_RingIndication
import charm.widgets.ring_indication;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextBox
import charm.widgets.text_box;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_FoldablePanel
import charm.widgets.foldable_panel;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressFlowing
import charm.widgets.progress_flowing;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_CloudyGlass
import charm.widgets.cloudy_glass;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_SpinZoomWidget
import charm.widgets.spin_zoom_widget;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_SpinningWheel
import charm.widgets.spinning_wheel;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_DynamicNebula
import charm.widgets.dynamic_nebula;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_CrtScreen
import charm.widgets.crt_screen;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_SpectrumView
import charm.widgets.spectrum_view;
#endif

template <typename T, std::size_t N>
using HandlePool = service::HandlePool<T, N>;

export
class UiFactory {
public:
    WidgetHandle create_container() noexcept { return make_handle(containers_.create(), WidgetKind::Container); }
#if CHARM_VIVID_ENABLE_WIDGET_ScrollContainer
    WidgetHandle create_scroll_container() noexcept { return make_handle(scrolls_.create(), WidgetKind::ScrollContainer); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Dial
    WidgetHandle create_dial() noexcept { return make_handle(dials_.create(), WidgetKind::Dial); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Arc
    WidgetHandle create_arc() noexcept { return make_handle(arcs_.create(), WidgetKind::Arc); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Image
    WidgetHandle create_image() noexcept { return make_handle(images_.create(), WidgetKind::Image); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ImageBox
    WidgetHandle create_image_box() noexcept { return make_handle(image_boxes_.create(), WidgetKind::ImageBox); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_BusyWheel
    WidgetHandle create_busy_wheel() noexcept { return make_handle(busy_wheels_.create(), WidgetKind::BusyWheel); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ConsoleBox
    WidgetHandle create_console_box() noexcept { return make_handle(console_boxes_.create(), WidgetKind::ConsoleBox); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_BatteryGasGauge
    WidgetHandle create_battery_gasgauge() noexcept { return make_handle(battery_gasgauge_.create(), WidgetKind::BatteryGasGauge); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Histogram
    WidgetHandle create_histogram() noexcept { return make_handle(histogram_.create(), WidgetKind::Histogram); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Label
    WidgetHandle create_label(const char* text) noexcept { return make_handle(labels_.create(text), WidgetKind::Label); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Button
    WidgetHandle create_button(const char* text) noexcept { return make_handle(buttons_.create(text), WidgetKind::Button); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Checkbox
    WidgetHandle create_checkbox(const char* text) noexcept { return make_handle(checkboxes_.create(text), WidgetKind::Checkbox); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Slider
    WidgetHandle create_slider() noexcept { return make_handle(sliders_.create(), WidgetKind::Slider); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Switch
    WidgetHandle create_switch() noexcept { return make_handle(switches_.create(), WidgetKind::Switch); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Progress
    WidgetHandle create_progress() noexcept { return make_handle(progresses_.create(), WidgetKind::Progress); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressBarRound
    WidgetHandle create_progress_bar_round() noexcept { return make_handle(progress_round_.create(), WidgetKind::ProgressBarRound); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressBarSimple
    WidgetHandle create_progress_bar_simple() noexcept { return make_handle(progress_simple_.create(), WidgetKind::ProgressBarSimple); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressBarDrill
    WidgetHandle create_progress_bar_drill() noexcept { return make_handle(progress_drill_.create(), WidgetKind::ProgressBarDrill); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_List
    WidgetHandle create_list() noexcept { return make_handle(lists_.create(), WidgetKind::List); }
#endif
    WidgetHandle create_list_item(const char* text) noexcept { return make_handle(list_items_.create(text), WidgetKind::ListItem); }
#if CHARM_VIVID_ENABLE_WIDGET_ListView
    WidgetHandle create_list_view() noexcept { return make_handle(list_views_.create(), WidgetKind::ListView); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_IconList
    WidgetHandle create_icon_list() noexcept { return make_handle(icon_lists_.create(), WidgetKind::IconList); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextTrackingList
    WidgetHandle create_text_tracking_list() noexcept { return make_handle(text_tracking_.create(), WidgetKind::TextTrackingList); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextList
    WidgetHandle create_text_list() noexcept { return make_handle(text_lists_.create(), WidgetKind::TextList); }
#endif
    WidgetHandle create_modal_dialog() noexcept { return make_handle(modals_.create(), WidgetKind::ModalDialog); }
    WidgetHandle create_scroll_bar() noexcept { return make_handle(scroll_bars_.create(), WidgetKind::ScrollBar); }
    WidgetHandle create_segmented_control() noexcept { return make_handle(segments_.create(), WidgetKind::SegmentedControl); }
    WidgetHandle create_text_area(const char* text) noexcept { return make_handle(text_areas_.create(text), WidgetKind::TextArea); }
    WidgetHandle create_text_input() noexcept { return make_handle(text_inputs_.create(), WidgetKind::TextInput); }
    WidgetHandle create_number_input() noexcept { return make_handle(number_inputs_.create(), WidgetKind::NumberInput); }
    WidgetHandle create_number_list() noexcept { return make_handle(number_lists_.create(), WidgetKind::NumberList); }
    WidgetHandle create_toggle_group() noexcept { return make_handle(toggles_.create(), WidgetKind::ToggleGroup); }
    WidgetHandle create_table_view() noexcept { return make_handle(tables_.create(), WidgetKind::TableView); }
    WidgetHandle create_tree_view() noexcept { return make_handle(trees_.create(), WidgetKind::TreeView); }
#if CHARM_VIVID_ENABLE_WIDGET_Dropdown
    WidgetHandle create_dropdown() noexcept { return make_handle(dropdowns_.create(), WidgetKind::Dropdown); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TabView
    WidgetHandle create_tabview() noexcept { return make_handle(tabviews_.create(), WidgetKind::TabView); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Roller
    WidgetHandle create_roller() noexcept { return make_handle(rollers_.create(), WidgetKind::Roller); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Spinner
    WidgetHandle create_spinner() noexcept { return make_handle(spinners_.create(), WidgetKind::Spinner); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Bar
    WidgetHandle create_bar() noexcept { return make_handle(bars_.create(), WidgetKind::Bar); }
#endif
    WidgetHandle create_popup_layer() noexcept { return make_handle(popups_.create(), WidgetKind::PopupLayer); }
#if CHARM_VIVID_ENABLE_WIDGET_Menu
    WidgetHandle create_menu() noexcept { return make_handle(menus_.create(), WidgetKind::Menu); }
#endif
    WidgetHandle create_menu_item(const char* text) noexcept { return make_handle(menu_items_.create(text), WidgetKind::MenuItem); }
#if CHARM_VIVID_ENABLE_WIDGET_Radio
    WidgetHandle create_radio(const char* text) noexcept { return make_handle(radios_.create(text), WidgetKind::Radio); }
#endif
    WidgetHandle create_radio_group() noexcept { return make_handle(radio_groups_.create(), WidgetKind::RadioGroup); }
#if CHARM_VIVID_ENABLE_WIDGET_Chart
    WidgetHandle create_chart() noexcept { return make_handle(charts_.create(), WidgetKind::Chart); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Waveform
    WidgetHandle create_waveform() noexcept { return make_handle(waveforms_.create(), WidgetKind::Waveform); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Gauge
    WidgetHandle create_gauge() noexcept { return make_handle(gauges_.create(), WidgetKind::Gauge); }
#endif
    WidgetHandle create_meter_pointer() noexcept { return make_handle(meters_.create(), WidgetKind::MeterPointer); }
    WidgetHandle create_primitives_canvas() noexcept { return make_handle(prim_canvas_.create(), WidgetKind::PrimitivesCanvas); }
    WidgetHandle create_perf_overlay() noexcept { return make_handle(perf_overlays_.create(), WidgetKind::PerfOverlay); }
#if CHARM_VIVID_ENABLE_WIDGET_Stepper
    WidgetHandle create_stepper() noexcept { return make_handle(steppers_.create(), WidgetKind::Stepper); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Timeline
    WidgetHandle create_timeline() noexcept { return make_handle(timelines_.create(), WidgetKind::Timeline); }
#endif
    WidgetHandle create_rich_text() noexcept { return make_handle(rich_texts_.create(), WidgetKind::RichText); }
    WidgetHandle create_code_block() noexcept { return make_handle(code_blocks_.create(), WidgetKind::CodeBlock); }
#if CHARM_VIVID_ENABLE_WIDGET_ProgressWheel
    WidgetHandle create_progress_wheel() noexcept { return make_handle(progress_wheels_.create(), WidgetKind::ProgressWheel); }
#endif
    WidgetHandle create_waveform_view() noexcept { return make_handle(waveform_views_.create(), WidgetKind::WaveformView); }
    WidgetHandle create_battery_gauge() noexcept { return make_handle(batteries_.create(), WidgetKind::BatteryGauge); }
#if CHARM_VIVID_ENABLE_WIDGET_HistogramView
    WidgetHandle create_histogram_view() noexcept { return make_handle(histograms_.create(), WidgetKind::HistogramView); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_RingIndication
    WidgetHandle create_ring_indication() noexcept { return make_handle(rings_.create(), WidgetKind::RingIndication); }
#endif
    WidgetHandle create_text_box(const char* text) noexcept { return make_handle(text_boxes_.create(text), WidgetKind::TextBox); }
#if CHARM_VIVID_ENABLE_WIDGET_FoldablePanel
    WidgetHandle create_foldable_panel(const char* title) noexcept { return make_handle(fold_panels_.create(title), WidgetKind::FoldablePanel); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressFlowing
    WidgetHandle create_progress_flowing() noexcept { return make_handle(progress_flow_.create(), WidgetKind::ProgressFlowing); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_CloudyGlass
    WidgetHandle create_cloudy_glass() noexcept { return make_handle(glass_.create(), WidgetKind::CloudyGlass); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_SpinZoomWidget
    WidgetHandle create_spin_zoom_widget() noexcept { return make_handle(spin_zoom_.create(), WidgetKind::SpinZoomWidget); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_SpinningWheel
    WidgetHandle create_spinning_wheel() noexcept { return make_handle(spinning_wheel_.create(), WidgetKind::SpinningWheel); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_SpectrumView
    WidgetHandle create_spectrum_view() noexcept { return make_handle(spectrum_views_.create(), WidgetKind::SpectrumView); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_DynamicNebula
    WidgetHandle create_dynamic_nebula() noexcept { return make_handle(nebula_.create(), WidgetKind::DynamicNebula); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_CrtScreen
    WidgetHandle create_crt_screen() noexcept { return make_handle(crt_.create(), WidgetKind::CrtScreen); }
#endif
    void set_overlay(WidgetHandle h) noexcept { overlay_ = h; }
    void clear_overlay(WidgetHandle h) noexcept { if (overlay_ == h) overlay_ = {}; }
    WidgetHandle overlay() const noexcept { return overlay_; }

    Container* get_container(const WidgetHandle& h) noexcept { return get_from(containers_, h, WidgetKind::Container); }
#if CHARM_VIVID_ENABLE_WIDGET_ScrollContainer
    ScrollContainer* get_scroll_container(const WidgetHandle& h) noexcept { return get_from(scrolls_, h, WidgetKind::ScrollContainer); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Dial
    Dial* get_dial(const WidgetHandle& h) noexcept { return get_from(dials_, h, WidgetKind::Dial); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Arc
    Arc* get_arc(const WidgetHandle& h) noexcept { return get_from(arcs_, h, WidgetKind::Arc); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Image
    Image* get_image(const WidgetHandle& h) noexcept { return get_from(images_, h, WidgetKind::Image); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ImageBox
    ImageBox* get_image_box(const WidgetHandle& h) noexcept { return get_from(image_boxes_, h, WidgetKind::ImageBox); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_BusyWheel
    BusyWheel* get_busy_wheel(const WidgetHandle& h) noexcept { return get_from(busy_wheels_, h, WidgetKind::BusyWheel); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ConsoleBox
    ConsoleBox* get_console_box(const WidgetHandle& h) noexcept { return get_from(console_boxes_, h, WidgetKind::ConsoleBox); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_BatteryGasGauge
    BatteryGasGauge* get_battery_gasgauge(const WidgetHandle& h) noexcept { return get_from(battery_gasgauge_, h, WidgetKind::BatteryGasGauge); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Histogram
    Histogram* get_histogram(const WidgetHandle& h) noexcept { return get_from(histogram_, h, WidgetKind::Histogram); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Label
    Label* get_label(const WidgetHandle& h) noexcept { return get_from(labels_, h, WidgetKind::Label); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Button
    Button* get_button(const WidgetHandle& h) noexcept { return get_from(buttons_, h, WidgetKind::Button); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Checkbox
    Checkbox* get_checkbox(const WidgetHandle& h) noexcept { return get_from(checkboxes_, h, WidgetKind::Checkbox); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Slider
    Slider* get_slider(const WidgetHandle& h) noexcept { return get_from(sliders_, h, WidgetKind::Slider); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Switch
    Switch* get_switch(const WidgetHandle& h) noexcept { return get_from(switches_, h, WidgetKind::Switch); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Progress
    Progress* get_progress(const WidgetHandle& h) noexcept { return get_from(progresses_, h, WidgetKind::Progress); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressBarRound
    ProgressBarRound* get_progress_bar_round(const WidgetHandle& h) noexcept { return get_from(progress_round_, h, WidgetKind::ProgressBarRound); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressBarSimple
    ProgressBarSimple* get_progress_bar_simple(const WidgetHandle& h) noexcept { return get_from(progress_simple_, h, WidgetKind::ProgressBarSimple); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressBarDrill
    ProgressBarDrill* get_progress_bar_drill(const WidgetHandle& h) noexcept { return get_from(progress_drill_, h, WidgetKind::ProgressBarDrill); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_List
    List* get_list(const WidgetHandle& h) noexcept { return get_from(lists_, h, WidgetKind::List); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ListItem
    ListItem* get_list_item(const WidgetHandle& h) noexcept { return get_from(list_items_, h, WidgetKind::ListItem); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ListView
    ListView* get_list_view(const WidgetHandle& h) noexcept { return get_from(list_views_, h, WidgetKind::ListView); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_IconList
    IconList* get_icon_list(const WidgetHandle& h) noexcept { return get_from(icon_lists_, h, WidgetKind::IconList); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextTrackingList
    TextTrackingList* get_text_tracking_list(const WidgetHandle& h) noexcept { return get_from(text_tracking_, h, WidgetKind::TextTrackingList); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextList
    TextList* get_text_list(const WidgetHandle& h) noexcept { return get_from(text_lists_, h, WidgetKind::TextList); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ModalDialog
    ModalDialog* get_modal_dialog(const WidgetHandle& h) noexcept { return get_from(modals_, h, WidgetKind::ModalDialog); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ScrollBar
    ScrollBar* get_scroll_bar(const WidgetHandle& h) noexcept { return get_from(scroll_bars_, h, WidgetKind::ScrollBar); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_SegmentedControl
    SegmentedControl* get_segmented_control(const WidgetHandle& h) noexcept { return get_from(segments_, h, WidgetKind::SegmentedControl); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextArea
    TextArea* get_text_area(const WidgetHandle& h) noexcept { return get_from(text_areas_, h, WidgetKind::TextArea); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextInput
    TextInput* get_text_input(const WidgetHandle& h) noexcept { return get_from(text_inputs_, h, WidgetKind::TextInput); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_NumberInput
    NumberInput* get_number_input(const WidgetHandle& h) noexcept { return get_from(number_inputs_, h, WidgetKind::NumberInput); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_NumberList
    NumberList* get_number_list(const WidgetHandle& h) noexcept { return get_from(number_lists_, h, WidgetKind::NumberList); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ToggleGroup
    ToggleGroup* get_toggle_group(const WidgetHandle& h) noexcept { return get_from(toggles_, h, WidgetKind::ToggleGroup); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TableView
    TableView* get_table_view(const WidgetHandle& h) noexcept { return get_from(tables_, h, WidgetKind::TableView); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TreeView
    TreeView* get_tree_view(const WidgetHandle& h) noexcept { return get_from(trees_, h, WidgetKind::TreeView); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Dropdown
    Dropdown* get_dropdown(const WidgetHandle& h) noexcept { return get_from(dropdowns_, h, WidgetKind::Dropdown); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TabView
    TabView* get_tabview(const WidgetHandle& h) noexcept { return get_from(tabviews_, h, WidgetKind::TabView); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Roller
    Roller* get_roller(const WidgetHandle& h) noexcept { return get_from(rollers_, h, WidgetKind::Roller); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Spinner
    Spinner* get_spinner(const WidgetHandle& h) noexcept { return get_from(spinners_, h, WidgetKind::Spinner); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Bar
    Bar* get_bar(const WidgetHandle& h) noexcept { return get_from(bars_, h, WidgetKind::Bar); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Menu
    Menu* get_menu(const WidgetHandle& h) noexcept { return get_from(menus_, h, WidgetKind::Menu); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_MenuItem
    MenuItem* get_menu_item(const WidgetHandle& h) noexcept { return get_from(menu_items_, h, WidgetKind::MenuItem); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Radio
    Radio* get_radio(const WidgetHandle& h) noexcept { return get_from(radios_, h, WidgetKind::Radio); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_RadioGroup
    RadioGroup* get_radio_group(const WidgetHandle& h) noexcept { return get_from(radio_groups_, h, WidgetKind::RadioGroup); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Chart
    Chart* get_chart(const WidgetHandle& h) noexcept { return get_from(charts_, h, WidgetKind::Chart); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Waveform
    Waveform* get_waveform(const WidgetHandle& h) noexcept { return get_from(waveforms_, h, WidgetKind::Waveform); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Gauge
    Gauge* get_gauge(const WidgetHandle& h) noexcept { return get_from(gauges_, h, WidgetKind::Gauge); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_MeterPointer
    MeterPointer* get_meter_pointer(const WidgetHandle& h) noexcept { return get_from(meters_, h, WidgetKind::MeterPointer); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_PrimitivesCanvas
    PrimitivesCanvas* get_primitives_canvas(const WidgetHandle& h) noexcept { return get_from(prim_canvas_, h, WidgetKind::PrimitivesCanvas); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_PerfOverlay
    PerfOverlay* get_perf_overlay(const WidgetHandle& h) noexcept { return get_from(perf_overlays_, h, WidgetKind::PerfOverlay); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Stepper
    Stepper* get_stepper(const WidgetHandle& h) noexcept { return get_from(steppers_, h, WidgetKind::Stepper); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Timeline
    Timeline* get_timeline(const WidgetHandle& h) noexcept { return get_from(timelines_, h, WidgetKind::Timeline); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_RichText
    RichText* get_rich_text(const WidgetHandle& h) noexcept { return get_from(rich_texts_, h, WidgetKind::RichText); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_CodeBlock
    CodeBlock* get_code_block(const WidgetHandle& h) noexcept { return get_from(code_blocks_, h, WidgetKind::CodeBlock); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressWheel
    ProgressWheel* get_progress_wheel(const WidgetHandle& h) noexcept { return get_from(progress_wheels_, h, WidgetKind::ProgressWheel); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_WaveformView
    WaveformView* get_waveform_view(const WidgetHandle& h) noexcept { return get_from(waveform_views_, h, WidgetKind::WaveformView); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_BatteryGauge
    BatteryGauge* get_battery_gauge(const WidgetHandle& h) noexcept { return get_from(batteries_, h, WidgetKind::BatteryGauge); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_HistogramView
    HistogramView* get_histogram_view(const WidgetHandle& h) noexcept { return get_from(histograms_, h, WidgetKind::HistogramView); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_RingIndication
    RingIndication* get_ring_indication(const WidgetHandle& h) noexcept { return get_from(rings_, h, WidgetKind::RingIndication); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextBox
    TextBox* get_text_box(const WidgetHandle& h) noexcept { return get_from(text_boxes_, h, WidgetKind::TextBox); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_FoldablePanel
    FoldablePanel* get_foldable_panel(const WidgetHandle& h) noexcept { return get_from(fold_panels_, h, WidgetKind::FoldablePanel); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressFlowing
    ProgressFlowing* get_progress_flowing(const WidgetHandle& h) noexcept { return get_from(progress_flow_, h, WidgetKind::ProgressFlowing); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_CloudyGlass
    CloudyGlass* get_cloudy_glass(const WidgetHandle& h) noexcept { return get_from(glass_, h, WidgetKind::CloudyGlass); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_SpinZoomWidget
    SpinZoomWidget* get_spin_zoom_widget(const WidgetHandle& h) noexcept { return get_from(spin_zoom_, h, WidgetKind::SpinZoomWidget); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_SpinningWheel
    SpinningWheel* get_spinning_wheel(const WidgetHandle& h) noexcept { return get_from(spinning_wheel_, h, WidgetKind::SpinningWheel); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_SpectrumView
    SpectrumView* get_spectrum_view(const WidgetHandle& h) noexcept { return get_from(spectrum_views_, h, WidgetKind::SpectrumView); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_DynamicNebula
    DynamicNebula* get_dynamic_nebula(const WidgetHandle& h) noexcept { return get_from(nebula_, h, WidgetKind::DynamicNebula); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_CrtScreen
    CrtScreen* get_crt_screen(const WidgetHandle& h) noexcept { return get_from(crt_, h, WidgetKind::CrtScreen); }
#endif

    ObjectBase* get(const WidgetHandle& h) noexcept {
        switch (h.kind) {
#if CHARM_VIVID_ENABLE_WIDGET_Container
            case WidgetKind::Container: return get_container(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ScrollContainer
            case WidgetKind::ScrollContainer: return get_scroll_container(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Dial
            case WidgetKind::Dial: return get_dial(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Arc
            case WidgetKind::Arc: return get_arc(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Image
            case WidgetKind::Image: return get_image(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ImageBox
            case WidgetKind::ImageBox: return get_image_box(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_BusyWheel
            case WidgetKind::BusyWheel: return get_busy_wheel(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ConsoleBox
            case WidgetKind::ConsoleBox: return get_console_box(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_BatteryGasGauge
            case WidgetKind::BatteryGasGauge: return get_battery_gasgauge(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Histogram
            case WidgetKind::Histogram: return get_histogram(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Label
            case WidgetKind::Label: return get_label(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Button
            case WidgetKind::Button: return get_button(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Checkbox
            case WidgetKind::Checkbox: return get_checkbox(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Slider
            case WidgetKind::Slider: return get_slider(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Switch
            case WidgetKind::Switch: return get_switch(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Progress
            case WidgetKind::Progress: return get_progress(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressBarRound
            case WidgetKind::ProgressBarRound: return get_progress_bar_round(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressBarSimple
            case WidgetKind::ProgressBarSimple: return get_progress_bar_simple(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressBarDrill
            case WidgetKind::ProgressBarDrill: return get_progress_bar_drill(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_List
            case WidgetKind::List: return get_list(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ListItem
            case WidgetKind::ListItem: return get_list_item(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ListView
            case WidgetKind::ListView: return get_list_view(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_IconList
            case WidgetKind::IconList: return get_icon_list(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextTrackingList
            case WidgetKind::TextTrackingList: return get_text_tracking_list(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextList
            case WidgetKind::TextList: return get_text_list(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ModalDialog
            case WidgetKind::ModalDialog: return get_modal_dialog(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ScrollBar
            case WidgetKind::ScrollBar: return get_scroll_bar(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_SegmentedControl
            case WidgetKind::SegmentedControl: return get_segmented_control(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextArea
            case WidgetKind::TextArea: return get_text_area(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextInput
            case WidgetKind::TextInput: return get_text_input(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_NumberInput
            case WidgetKind::NumberInput: return get_number_input(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_NumberList
            case WidgetKind::NumberList: return get_number_list(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ToggleGroup
            case WidgetKind::ToggleGroup: return get_toggle_group(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TableView
            case WidgetKind::TableView: return get_table_view(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TreeView
            case WidgetKind::TreeView: return get_tree_view(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Dropdown
            case WidgetKind::Dropdown: return get_dropdown(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TabView
            case WidgetKind::TabView: return get_tabview(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Roller
            case WidgetKind::Roller: return get_roller(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Spinner
            case WidgetKind::Spinner: return get_spinner(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Bar
            case WidgetKind::Bar: return get_bar(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_PopupLayer
            case WidgetKind::PopupLayer: return get_from(popups_, h, WidgetKind::PopupLayer);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Menu
            case WidgetKind::Menu: return get_menu(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_MenuItem
            case WidgetKind::MenuItem: return get_menu_item(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Radio
            case WidgetKind::Radio: return get_radio(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_RadioGroup
            case WidgetKind::RadioGroup: return get_radio_group(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Chart
            case WidgetKind::Chart: return get_chart(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Waveform
            case WidgetKind::Waveform: return get_waveform(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Gauge
            case WidgetKind::Gauge: return get_gauge(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_MeterPointer
            case WidgetKind::MeterPointer: return get_meter_pointer(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_PrimitivesCanvas
            case WidgetKind::PrimitivesCanvas: return get_primitives_canvas(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_PerfOverlay
            case WidgetKind::PerfOverlay: return get_perf_overlay(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Stepper
            case WidgetKind::Stepper: return get_stepper(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Timeline
            case WidgetKind::Timeline: return get_timeline(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_RichText
            case WidgetKind::RichText: return get_rich_text(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_CodeBlock
            case WidgetKind::CodeBlock: return get_code_block(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressWheel
            case WidgetKind::ProgressWheel: return get_progress_wheel(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_WaveformView
            case WidgetKind::WaveformView: return get_waveform_view(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_BatteryGauge
            case WidgetKind::BatteryGauge: return get_battery_gauge(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_HistogramView
            case WidgetKind::HistogramView: return get_histogram_view(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_RingIndication
            case WidgetKind::RingIndication: return get_ring_indication(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextBox
            case WidgetKind::TextBox: return get_text_box(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_FoldablePanel
            case WidgetKind::FoldablePanel: return get_foldable_panel(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressFlowing
            case WidgetKind::ProgressFlowing: return get_progress_flowing(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_CloudyGlass
            case WidgetKind::CloudyGlass: return get_cloudy_glass(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_SpinZoomWidget
            case WidgetKind::SpinZoomWidget: return get_spin_zoom_widget(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_SpinningWheel
            case WidgetKind::SpinningWheel: return get_spinning_wheel(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_SpectrumView
            case WidgetKind::SpectrumView: return get_spectrum_view(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_DynamicNebula
            case WidgetKind::DynamicNebula: return get_dynamic_nebula(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_CrtScreen
            case WidgetKind::CrtScreen: return get_crt_screen(h);
#endif
            default: return nullptr;
        }
    }

    void detach_from_parent_and_children(const WidgetHandle& h) noexcept {
        auto* obj = get(h);
        if (!obj) return;
        auto parent = obj->parent();
        if (parent) {
            if (auto* p = get(parent)) {
                p->remove_child(h);
            }
            obj->set_parent({});
        }
        const std::size_t count = obj->child_count();
        for (std::size_t i = 0; i < count; ++i) {
            auto child = obj->child_at(i);
            if (auto* c = get(child)) {
                if (c->parent() == h) c->set_parent({});
            }
        }
        obj->clear_children();
    }

    bool link(const WidgetHandle& parent, const WidgetHandle& child) noexcept {
        auto* p = get(parent);
        auto* c = get(child);
        if (!p || !c) return false;
        if (parent == child) return false;
        if (creates_cycle(parent, child)) return false;
        if (c->parent() && c->parent() != parent) {
            if (auto* old = get(c->parent())) {
                old->remove_child(child);
            }
        }
        if (p->has_child(child)) {
            c->set_parent(parent);
            return true;
        }
        if (!p->add_child(child)) return false;
        c->set_parent(parent);
        return true;
    }

    bool insert_before(const WidgetHandle& parent, const WidgetHandle& child, const WidgetHandle& before) noexcept {
        auto* p = get(parent);
        auto* c = get(child);
        if (!p || !c) return false;
        if (parent == child) return false;
        if (creates_cycle(parent, child)) return false;
        if (c->parent() && c->parent() != parent) {
            if (auto* old = get(c->parent())) {
                old->remove_child(child);
            }
        }
        if (p->has_child(child)) {
            p->remove_child(child);
        }
        if (!p->insert_child_before(child, before)) return false;
        c->set_parent(parent);
        return true;
    }

    bool insert_after(const WidgetHandle& parent, const WidgetHandle& child, const WidgetHandle& after) noexcept {
        auto* p = get(parent);
        auto* c = get(child);
        if (!p || !c) return false;
        if (parent == child) return false;
        if (creates_cycle(parent, child)) return false;
        if (c->parent() && c->parent() != parent) {
            if (auto* old = get(c->parent())) {
                old->remove_child(child);
            }
        }
        if (p->has_child(child)) {
            p->remove_child(child);
        }
        if (!p->insert_child_after(child, after)) return false;
        c->set_parent(parent);
        return true;
    }

    bool unlink(const WidgetHandle& parent, const WidgetHandle& child) noexcept {
        auto* p = get(parent);
        auto* c = get(child);
        if (!p || !c) return false;
        if (!p->remove_child(child)) return false;
        if (c->parent() == parent) c->set_parent({});
        return true;
    }

    void unlink_all_children(const WidgetHandle& parent) noexcept {
        auto* p = get(parent);
        if (!p) return;
        while (p->child_count() > 0) {
            auto child = p->child_at(0);
            auto* c = get(child);
            p->remove_child(child);
            if (c && c->parent() == parent) c->set_parent({});
        }
    }

    bool bring_to_front(const WidgetHandle& parent, const WidgetHandle& child) noexcept {
        auto* p = get(parent);
        if (!p) return false;
        return p->move_child_to_front(child);
    }

    bool send_to_back(const WidgetHandle& parent, const WidgetHandle& child) noexcept {
        auto* p = get(parent);
        if (!p) return false;
        return p->move_child_to_back(child);
    }

    template<typename Fn>
    void for_each_child(const WidgetHandle& parent, Fn&& fn) noexcept {
        auto* p = get(parent);
        if (!p) return;
        for (std::size_t i = 0; i < p->child_count(); ++i) {
            fn(p->child_at(i));
        }
    }

    template<typename Fn>
    void for_each_child_reverse(const WidgetHandle& parent, Fn&& fn) noexcept {
        auto* p = get(parent);
        if (!p) return;
        for (std::size_t i = p->child_count(); i > 0; --i) {
            fn(p->child_at(i - 1));
        }
    }

    std::size_t child_index(const WidgetHandle& parent, const WidgetHandle& child) noexcept {
        auto* p = get(parent);
        if (!p) return 0;
        return p->child_index(child);
    }

    template<typename Fn>
    void traverse(const WidgetHandle& root, Fn&& fn) noexcept {
        auto* obj = get(root);
        if (!obj) return;
        fn(root, *obj);
        for (std::size_t i = 0; i < obj->child_count(); ++i) {
            traverse(obj->child_at(i), std::forward<Fn>(fn));
        }
    }

    void destroy(const WidgetHandle& h) noexcept {
        detach_from_parent_and_children(h);
        switch (h.kind) {
#if CHARM_VIVID_ENABLE_WIDGET_Container
        case WidgetKind::Container: destroy_from(containers_, h, WidgetKind::Container); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ScrollContainer
        case WidgetKind::ScrollContainer: destroy_from(scrolls_, h, WidgetKind::ScrollContainer); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Dial
        case WidgetKind::Dial: destroy_from(dials_, h, WidgetKind::Dial); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Arc
        case WidgetKind::Arc: destroy_from(arcs_, h, WidgetKind::Arc); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Image
        case WidgetKind::Image: destroy_from(images_, h, WidgetKind::Image); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ImageBox
        case WidgetKind::ImageBox: destroy_from(image_boxes_, h, WidgetKind::ImageBox); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_BusyWheel
        case WidgetKind::BusyWheel: destroy_from(busy_wheels_, h, WidgetKind::BusyWheel); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ConsoleBox
        case WidgetKind::ConsoleBox: destroy_from(console_boxes_, h, WidgetKind::ConsoleBox); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_BatteryGasGauge
        case WidgetKind::BatteryGasGauge: destroy_from(battery_gasgauge_, h, WidgetKind::BatteryGasGauge); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Histogram
        case WidgetKind::Histogram: destroy_from(histogram_, h, WidgetKind::Histogram); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Label
        case WidgetKind::Label: destroy_from(labels_, h, WidgetKind::Label); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Button
        case WidgetKind::Button: destroy_from(buttons_, h, WidgetKind::Button); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Checkbox
        case WidgetKind::Checkbox: destroy_from(checkboxes_, h, WidgetKind::Checkbox); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Slider
        case WidgetKind::Slider: destroy_from(sliders_, h, WidgetKind::Slider); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Switch
        case WidgetKind::Switch: destroy_from(switches_, h, WidgetKind::Switch); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Progress
        case WidgetKind::Progress: destroy_from(progresses_, h, WidgetKind::Progress); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressBarRound
        case WidgetKind::ProgressBarRound: destroy_from(progress_round_, h, WidgetKind::ProgressBarRound); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressBarSimple
        case WidgetKind::ProgressBarSimple: destroy_from(progress_simple_, h, WidgetKind::ProgressBarSimple); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressBarDrill
        case WidgetKind::ProgressBarDrill: destroy_from(progress_drill_, h, WidgetKind::ProgressBarDrill); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_List
        case WidgetKind::List: destroy_from(lists_, h, WidgetKind::List); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ListItem
        case WidgetKind::ListItem: destroy_from(list_items_, h, WidgetKind::ListItem); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ListView
        case WidgetKind::ListView: destroy_from(list_views_, h, WidgetKind::ListView); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_IconList
        case WidgetKind::IconList: destroy_from(icon_lists_, h, WidgetKind::IconList); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextTrackingList
        case WidgetKind::TextTrackingList: destroy_from(text_tracking_, h, WidgetKind::TextTrackingList); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextList
        case WidgetKind::TextList: destroy_from(text_lists_, h, WidgetKind::TextList); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ModalDialog
        case WidgetKind::ModalDialog: destroy_from(modals_, h, WidgetKind::ModalDialog); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ScrollBar
        case WidgetKind::ScrollBar: destroy_from(scroll_bars_, h, WidgetKind::ScrollBar); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_SegmentedControl
        case WidgetKind::SegmentedControl: destroy_from(segments_, h, WidgetKind::SegmentedControl); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextArea
        case WidgetKind::TextArea: destroy_from(text_areas_, h, WidgetKind::TextArea); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextInput
        case WidgetKind::TextInput: destroy_from(text_inputs_, h, WidgetKind::TextInput); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_NumberInput
        case WidgetKind::NumberInput: destroy_from(number_inputs_, h, WidgetKind::NumberInput); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_NumberList
        case WidgetKind::NumberList: destroy_from(number_lists_, h, WidgetKind::NumberList); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ToggleGroup
        case WidgetKind::ToggleGroup: destroy_from(toggles_, h, WidgetKind::ToggleGroup); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TableView
        case WidgetKind::TableView: destroy_from(tables_, h, WidgetKind::TableView); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TreeView
        case WidgetKind::TreeView: destroy_from(trees_, h, WidgetKind::TreeView); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Dropdown
        case WidgetKind::Dropdown: destroy_from(dropdowns_, h, WidgetKind::Dropdown); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TabView
        case WidgetKind::TabView: destroy_from(tabviews_, h, WidgetKind::TabView); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Roller
        case WidgetKind::Roller: destroy_from(rollers_, h, WidgetKind::Roller); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Spinner
        case WidgetKind::Spinner: destroy_from(spinners_, h, WidgetKind::Spinner); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Bar
        case WidgetKind::Bar: destroy_from(bars_, h, WidgetKind::Bar); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_PopupLayer
        case WidgetKind::PopupLayer: destroy_from(popups_, h, WidgetKind::PopupLayer); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Menu
        case WidgetKind::Menu: destroy_from(menus_, h, WidgetKind::Menu); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_MenuItem
        case WidgetKind::MenuItem: destroy_from(menu_items_, h, WidgetKind::MenuItem); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Radio
        case WidgetKind::Radio: destroy_from(radios_, h, WidgetKind::Radio); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_RadioGroup
        case WidgetKind::RadioGroup: destroy_from(radio_groups_, h, WidgetKind::RadioGroup); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Chart
        case WidgetKind::Chart: destroy_from(charts_, h, WidgetKind::Chart); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Waveform
        case WidgetKind::Waveform: destroy_from(waveforms_, h, WidgetKind::Waveform); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Gauge
        case WidgetKind::Gauge: destroy_from(gauges_, h, WidgetKind::Gauge); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_MeterPointer
        case WidgetKind::MeterPointer: destroy_from(meters_, h, WidgetKind::MeterPointer); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_PrimitivesCanvas
        case WidgetKind::PrimitivesCanvas: destroy_from(prim_canvas_, h, WidgetKind::PrimitivesCanvas); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_PerfOverlay
        case WidgetKind::PerfOverlay: destroy_from(perf_overlays_, h, WidgetKind::PerfOverlay); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Stepper
        case WidgetKind::Stepper: destroy_from(steppers_, h, WidgetKind::Stepper); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Timeline
        case WidgetKind::Timeline: destroy_from(timelines_, h, WidgetKind::Timeline); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_RichText
        case WidgetKind::RichText: destroy_from(rich_texts_, h, WidgetKind::RichText); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_CodeBlock
        case WidgetKind::CodeBlock: destroy_from(code_blocks_, h, WidgetKind::CodeBlock); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressWheel
        case WidgetKind::ProgressWheel: destroy_from(progress_wheels_, h, WidgetKind::ProgressWheel); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_WaveformView
        case WidgetKind::WaveformView: destroy_from(waveform_views_, h, WidgetKind::WaveformView); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_BatteryGauge
        case WidgetKind::BatteryGauge: destroy_from(batteries_, h, WidgetKind::BatteryGauge); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_HistogramView
        case WidgetKind::HistogramView: destroy_from(histograms_, h, WidgetKind::HistogramView); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_RingIndication
        case WidgetKind::RingIndication: destroy_from(rings_, h, WidgetKind::RingIndication); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextBox
        case WidgetKind::TextBox: destroy_from(text_boxes_, h, WidgetKind::TextBox); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_FoldablePanel
        case WidgetKind::FoldablePanel: destroy_from(fold_panels_, h, WidgetKind::FoldablePanel); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressFlowing
        case WidgetKind::ProgressFlowing: destroy_from(progress_flow_, h, WidgetKind::ProgressFlowing); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_CloudyGlass
        case WidgetKind::CloudyGlass: destroy_from(glass_, h, WidgetKind::CloudyGlass); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_SpinZoomWidget
        case WidgetKind::SpinZoomWidget: destroy_from(spin_zoom_, h, WidgetKind::SpinZoomWidget); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_SpinningWheel
        case WidgetKind::SpinningWheel: destroy_from(spinning_wheel_, h, WidgetKind::SpinningWheel); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_SpectrumView
        case WidgetKind::SpectrumView: destroy_from(spectrum_views_, h, WidgetKind::SpectrumView); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_DynamicNebula
        case WidgetKind::DynamicNebula: destroy_from(nebula_, h, WidgetKind::DynamicNebula); break;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_CrtScreen
        case WidgetKind::CrtScreen: destroy_from(crt_, h, WidgetKind::CrtScreen); break;
#endif
            default: break;
        }
        if (overlay_ == h) overlay_ = {};
    }

    void destroy_tree(const WidgetHandle& h) noexcept {
        auto* obj = get(h);
        if (!obj) return;
        while (obj->child_count() > 0) {
            auto child = obj->child_at(obj->child_count() - 1);
            destroy_tree(child);
            obj = get(h);
            if (!obj) break;
        }
        destroy(h);
    }

    struct TreeSanitizeReport {
        int removed{};
        int missing{};
        int self_ref{};
        int invalid_parent{};
        int cycle{};
        WidgetHandle last_parent{};
        WidgetHandle last_child{};
    };

    struct TreeValidateReport {
        int issues{};
        int missing{};
        int self_ref{};
        int invalid_parent{};
        int cycle{};
        WidgetHandle last_parent{};
        WidgetHandle last_child{};
    };

    const TreeSanitizeReport& last_sanitize_report() const noexcept {
        return last_report_;
    }

    TreeValidateReport validate_tree(const WidgetHandle& root) const noexcept {
        TreeValidateReport rep{};
        struct Frame { WidgetHandle h; std::size_t idx; };
        constexpr int kMaxDepth = 128;
        Frame stack[kMaxDepth]{};
        WidgetHandle path[kMaxDepth]{};
        int depth = 0;
        if (!root) return rep;
        stack[0] = {root, 0};
        path[0] = root;
        depth = 1;

        auto in_path = [&](WidgetHandle h) -> bool {
            for (int i = 0; i < depth; ++i) {
                if (path[i] == h) return true;
            }
            return false;
        };

        while (depth > 0) {
            auto& frame = stack[depth - 1];
            auto* obj = const_cast<UiFactory*>(this)->get(frame.h);
            if (!obj) {
                --depth;
                continue;
            }

            if (frame.idx >= obj->child_count()) {
                --depth;
                continue;
            }

            auto child = obj->child_at(frame.idx);
            ++frame.idx;

            auto* ch = const_cast<UiFactory*>(this)->get(child);
            if (!ch) {
                ++rep.issues; ++rep.missing;
                rep.last_parent = frame.h; rep.last_child = child;
                continue;
            }
            if (child == frame.h) {
                ++rep.issues; ++rep.self_ref;
                rep.last_parent = frame.h; rep.last_child = child;
                continue;
            }
            if (ch->parent() != frame.h) {
                ++rep.issues; ++rep.invalid_parent;
                rep.last_parent = frame.h; rep.last_child = child;
                continue;
            }
            if (in_path(child)) {
                ++rep.issues; ++rep.cycle;
                rep.last_parent = frame.h; rep.last_child = child;
                continue;
            }

            if (depth < kMaxDepth) {
                stack[depth] = {child, 0};
                path[depth] = child;
                ++depth;
            }
        }
        return rep;
    }

    void sanitize_tree(const WidgetHandle& root) noexcept {
        last_report_ = {};
        struct Frame { WidgetHandle h; std::size_t idx; };
        constexpr int kMaxDepth = 128;
        Frame stack[kMaxDepth]{};
        WidgetHandle path[kMaxDepth]{};
        int depth = 0;
        if (!root) return;
        stack[0] = {root, 0};
        path[0] = root;
        depth = 1;

        auto in_path = [&](WidgetHandle h) -> bool {
            for (int i = 0; i < depth; ++i) {
                if (path[i] == h) return true;
            }
            return false;
        };

        while (depth > 0) {
            auto& frame = stack[depth - 1];
            auto* obj = get(frame.h);
            if (!obj) {
                --depth;
                continue;
            }

            if (frame.idx >= obj->child_count()) {
                --depth;
                continue;
            }

            const std::size_t idx = frame.idx;
            auto child = obj->child_at(idx);
            ++frame.idx;

            auto* ch = get(child);
            if (!ch) {
                record_remove(frame.h, child, RemoveReason::Missing);
                obj->remove_child(child);
                frame.idx = (idx > 0) ? idx - 1 : 0;
                continue;
            }
            if (child == frame.h) {
                record_remove(frame.h, child, RemoveReason::Self);
                obj->remove_child(child);
                frame.idx = (idx > 0) ? idx - 1 : 0;
                continue;
            }
            if (ch->parent() != frame.h) {
                record_remove(frame.h, child, RemoveReason::InvalidParent);
                obj->remove_child(child);
                frame.idx = (idx > 0) ? idx - 1 : 0;
                continue;
            }
            if (in_path(child)) {
                record_remove(frame.h, child, RemoveReason::Cycle);
                obj->remove_child(child);
                frame.idx = (idx > 0) ? idx - 1 : 0;
                continue;
            }

            if (depth < kMaxDepth) {
                stack[depth] = {child, 0};
                path[depth] = child;
                ++depth;
            }
        }
    }

private:
    template <typename Handle>
    static WidgetHandle make_handle(std::optional<Handle> h, WidgetKind kind) noexcept {
        if (!h) return {};
        return WidgetHandle{kind, h->index, h->generation};
    }

    template <typename Pool>
    static typename Pool::Handle to_handle(const WidgetHandle& h) noexcept {
        return typename Pool::Handle{h.index, h.generation};
    }

    template <typename Pool>
    static auto get_from(Pool& pool, const WidgetHandle& h, WidgetKind kind) noexcept
        -> decltype(pool.get(typename Pool::Handle{})) {
        if (h.kind != kind) return nullptr;
        return pool.get(to_handle<Pool>(h));
    }

    template <typename Pool>
    static void destroy_from(Pool& pool, const WidgetHandle& h, WidgetKind kind) noexcept {
        if (h.kind != kind) return;
        pool.destroy(to_handle<Pool>(h));
    }

    enum class RemoveReason {
        Missing,
        Self,
        InvalidParent,
        Cycle
    };

    void record_remove(WidgetHandle parent, WidgetHandle child, RemoveReason reason) noexcept {
        ++last_report_.removed;
        last_report_.last_parent = parent;
        last_report_.last_child = child;
        switch (reason) {
        case RemoveReason::Missing: ++last_report_.missing; break;
        case RemoveReason::Self: ++last_report_.self_ref; break;
        case RemoveReason::InvalidParent: ++last_report_.invalid_parent; break;
        case RemoveReason::Cycle: ++last_report_.cycle; break;
        }
    }

    bool creates_cycle(const WidgetHandle& parent, const WidgetHandle& child) noexcept {
        // If parent is inside child's ancestor chain, linking would create a cycle.
        auto* obj = get(parent);
        while (obj) {
            const auto p = obj->parent();
            if (!p) break;
            if (p == child) return true;
            obj = get(p);
        }
        return false;
    }

    HandlePool<Container, 16> containers_{};
#if CHARM_VIVID_ENABLE_WIDGET_ScrollContainer
    HandlePool<ScrollContainer, 16> scrolls_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Dial
    HandlePool<Dial, 16> dials_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Arc
    HandlePool<Arc, 16> arcs_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Image
    HandlePool<Image, 32> images_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ImageBox
    HandlePool<ImageBox, 16> image_boxes_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_BusyWheel
    HandlePool<BusyWheel, 16> busy_wheels_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ConsoleBox
    HandlePool<ConsoleBox, 8> console_boxes_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_BatteryGasGauge
    HandlePool<BatteryGasGauge, 16> battery_gasgauge_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Histogram
    HandlePool<Histogram, 16> histogram_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Label
    HandlePool<Label, 128> labels_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Button
    HandlePool<Button, 64> buttons_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Checkbox
    HandlePool<Checkbox, 64> checkboxes_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Slider
    HandlePool<Slider, 64> sliders_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Switch
    HandlePool<Switch, 64> switches_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Progress
    HandlePool<Progress, 64> progresses_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressBarRound
    HandlePool<ProgressBarRound, 16> progress_round_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressBarSimple
    HandlePool<ProgressBarSimple, 32> progress_simple_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressBarDrill
    HandlePool<ProgressBarDrill, 16> progress_drill_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_List
    HandlePool<List, 32> lists_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ListItem
    HandlePool<ListItem, 128> list_items_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ListView
    HandlePool<ListView, 16> list_views_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_IconList
    HandlePool<IconList, 16> icon_lists_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextTrackingList
    HandlePool<TextTrackingList, 16> text_tracking_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextList
    HandlePool<TextList, 16> text_lists_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ModalDialog
    HandlePool<ModalDialog, 8> modals_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ScrollBar
    HandlePool<ScrollBar, 32> scroll_bars_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_SegmentedControl
    HandlePool<SegmentedControl, 32> segments_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextArea
    HandlePool<TextArea, 32> text_areas_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextInput
    HandlePool<TextInput, 64> text_inputs_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_NumberInput
    HandlePool<NumberInput, 32> number_inputs_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_NumberList
    HandlePool<NumberList, 16> number_lists_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ToggleGroup
    HandlePool<ToggleGroup, 32> toggles_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TableView
    HandlePool<TableView, 8> tables_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TreeView
    HandlePool<TreeView, 8> trees_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Dropdown
    HandlePool<Dropdown, 64> dropdowns_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TabView
    HandlePool<TabView, 16> tabviews_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Roller
    HandlePool<Roller, 32> rollers_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Spinner
    HandlePool<Spinner, 32> spinners_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Bar
    HandlePool<Bar, 64> bars_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_PopupLayer
    HandlePool<PopupLayer, 8> popups_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Menu
    HandlePool<Menu, 16> menus_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_MenuItem
    HandlePool<MenuItem, 64> menu_items_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Radio
    HandlePool<Radio, 64> radios_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_RadioGroup
    HandlePool<RadioGroup, 16> radio_groups_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Chart
    HandlePool<Chart, 16> charts_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Waveform
    HandlePool<Waveform, 8> waveforms_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Gauge
    HandlePool<Gauge, 16> gauges_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_MeterPointer
    HandlePool<MeterPointer, 16> meters_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_PrimitivesCanvas
    HandlePool<PrimitivesCanvas, 8> prim_canvas_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_PerfOverlay
    HandlePool<PerfOverlay, 8> perf_overlays_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Stepper
    HandlePool<Stepper, 16> steppers_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Timeline
    HandlePool<Timeline, 16> timelines_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_RichText
    HandlePool<RichText, 16> rich_texts_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_CodeBlock
    HandlePool<CodeBlock, 16> code_blocks_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressWheel
    HandlePool<ProgressWheel, 16> progress_wheels_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_WaveformView
    HandlePool<WaveformView, 16> waveform_views_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_BatteryGauge
    HandlePool<BatteryGauge, 16> batteries_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_HistogramView
    HandlePool<HistogramView, 16> histograms_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_RingIndication
    HandlePool<RingIndication, 16> rings_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextBox
    HandlePool<TextBox, 32> text_boxes_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_FoldablePanel
    HandlePool<FoldablePanel, 16> fold_panels_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressFlowing
    HandlePool<ProgressFlowing, 16> progress_flow_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_CloudyGlass
    HandlePool<CloudyGlass, 16> glass_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_SpinZoomWidget
    HandlePool<SpinZoomWidget, 8> spin_zoom_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_SpinningWheel
    HandlePool<SpinningWheel, 16> spinning_wheel_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_DynamicNebula
    HandlePool<DynamicNebula, 8> nebula_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_CrtScreen
    HandlePool<CrtScreen, 8> crt_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_SpectrumView
    HandlePool<SpectrumView, 16> spectrum_views_{};
#endif
    WidgetHandle overlay_{};
    TreeSanitizeReport last_report_{};
};
