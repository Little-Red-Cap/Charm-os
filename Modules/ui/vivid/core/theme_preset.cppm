module;
#include <type_traits>
export module charm.core.theme_preset;

export import charm.core.style;
export import charm.core.style_sheet;
export import charm.widgets.button;
export import charm.widgets.arc;
export import charm.widgets.bar;
export import charm.widgets.checkbox;
export import charm.widgets.code_block;
export import charm.widgets.label;
export import charm.widgets.list;
export import charm.widgets.list_view;
export import charm.widgets.icon_list;
export import charm.widgets.text_list;
export import charm.widgets.text_tracking_list;
export import charm.widgets.dial;
export import charm.widgets.dropdown_popup;
export import charm.widgets.image;
export import charm.widgets.image_box;
export import charm.widgets.led;
export import charm.widgets.message_box;
export import charm.widgets.modal_dialog;
export import charm.widgets.progress;
export import charm.widgets.progress_bar_simple;
export import charm.widgets.progress_bar_round;
export import charm.widgets.progress_bar_drill;
export import charm.widgets.progress_bar_flowing;
export import charm.widgets.progress_flowing;
export import charm.widgets.progress_wheel;
export import charm.widgets.spinner;
export import charm.widgets.scroll_container;
export import charm.widgets.scrollbar;
export import charm.widgets.segmented_control;
export import charm.widgets.slider;
export import charm.widgets.switcher;
export import charm.widgets.radio;
export import charm.widgets.radio_group;
export import charm.widgets.text_input;
export import charm.widgets.text_area;
export import charm.widgets.text_box;
export import charm.widgets.number_input;
export import charm.widgets.number_list;
export import charm.widgets.toggle_group;
export import charm.widgets.table_view;
export import charm.widgets.tree_view;
export import charm.widgets.dropdown;
export import charm.widgets.menu;
export import charm.widgets.menu_item;
export import charm.widgets.tabview;
export import charm.widgets.roller;
export import charm.widgets.rich_text;
export import charm.widgets.stepper;
export import charm.widgets.timeline;
export import charm.widgets.perf_overlay;
export import charm.widgets.popup_layer;
export import charm.widgets.primitives_canvas;
export import charm.widgets.foldable_panel;
export import charm.widgets.cloudy_glass;
export import charm.widgets.dynamic_nebula;
export import charm.widgets.crt_screen;
export import charm.widgets.spectrum_view;
export import charm.widgets.spinning_wheel;
export import charm.widgets.spin_zoom_widget;
export import charm.widgets.meter_pointer;
export import charm.widgets.battery_gauge;
export import charm.widgets.battery_gasgauge;
export import charm.widgets.histogram;
export import charm.widgets.histogram_view;
export import charm.widgets.chart;
export import charm.widgets.waveform;
export import charm.widgets.waveform_view;
export import charm.widgets.gauge;
export import charm.widgets.ring_indication;
export import charm.widgets.busy_wheel;
export import charm.widgets.console_box;

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
    if (preset.has_label) theme.set<Label>(preset.label);
    if (preset.has_button) theme.set<Button>(preset.button);
    if (preset.has_checkbox) theme.set<Checkbox>(preset.checkbox);
    if (preset.has_list_view) theme.set<ListView>(preset.list_view);
    if (preset.has_list_item) theme.set<ListItem>(preset.list_item);
    if (preset.has_list) theme.set<List>(preset.list);
    if (preset.has_icon_list) theme.set<IconList>(preset.icon_list);
    if (preset.has_text_list) theme.set<TextList>(preset.text_list);
    if (preset.has_text_tracking_list) theme.set<TextTrackingList>(preset.text_tracking_list);
    if (preset.has_progress) theme.set<Progress>(preset.progress);
    if (preset.has_progress_bar_simple) theme.set<ProgressBarSimple>(preset.progress_bar_simple);
    if (preset.has_progress_bar_round) theme.set<ProgressBarRound>(preset.progress_bar_round);
    if (preset.has_progress_bar_drill) theme.set<ProgressBarDrill>(preset.progress_bar_drill);
    if (preset.has_progress_flowing) theme.set<ProgressFlowing>(preset.progress_flowing);
    if (preset.has_progress_wheel) theme.set<ProgressWheel>(preset.progress_wheel);
    if (preset.has_spinner) theme.set<Spinner>(preset.spinner);
    if (preset.has_scroll_container) theme.set<ScrollContainer>(preset.scroll_container);
    if (preset.has_scroll_bar) theme.set<ScrollBar>(preset.scroll_bar);
    if (preset.has_segmented_control) theme.set<SegmentedControl>(preset.segmented_control);
    if (preset.has_slider) theme.set<Slider>(preset.slider);
    if (preset.has_switch) theme.set<Switch>(preset.switcher);
    if (preset.has_radio) theme.set<Radio>(preset.radio);
    if (preset.has_radio_group) theme.set<RadioGroup>(preset.radio_group);
    if (preset.has_text_input) theme.set<TextInput>(preset.text_input);
    if (preset.has_text_area) theme.set<TextArea>(preset.text_area);
    if (preset.has_text_box) theme.set<TextBox>(preset.text_box);
    if (preset.has_number_input) theme.set<NumberInput>(preset.number_input);
    if (preset.has_number_list) theme.set<NumberList>(preset.number_list);
    if (preset.has_toggle_group) theme.set<ToggleGroup>(preset.toggle_group);
    if (preset.has_table_view) theme.set<TableView>(preset.table_view);
    if (preset.has_tree_view) theme.set<TreeView>(preset.tree_view);
    if (preset.has_dropdown) theme.set<Dropdown>(preset.dropdown);
    if (preset.has_menu) theme.set<Menu>(preset.menu);
    if (preset.has_menu_item) theme.set<MenuItem>(preset.menu_item);
    if (preset.has_tab_view) theme.set<TabView>(preset.tab_view);
    if (preset.has_roller) theme.set<Roller>(preset.roller);
    if (preset.has_stepper) theme.set<Stepper>(preset.stepper);
    if (preset.has_perf_overlay) theme.set<PerfOverlay>(preset.perf_overlay);
    if (preset.has_foldable_panel) theme.set<FoldablePanel>(preset.foldable_panel);
    if (preset.has_cloudy_glass) theme.set<CloudyGlass>(preset.cloudy_glass);
    if (preset.has_dynamic_nebula) theme.set<DynamicNebula>(preset.dynamic_nebula);
    if (preset.has_crt_screen) theme.set<CrtScreen>(preset.crt_screen);
    if (preset.has_spectrum_view) theme.set<SpectrumView>(preset.spectrum_view);
    if (preset.has_battery_gasgauge) theme.set<BatteryGasGauge>(preset.battery_gasgauge);
    if (preset.has_histogram) theme.set<Histogram>(preset.histogram);
    if (preset.has_histogram_view) theme.set<HistogramView>(preset.histogram_view);
    if (preset.has_chart) theme.set<Chart>(preset.chart);
    if (preset.has_waveform) theme.set<Waveform>(preset.waveform);
    if (preset.has_waveform_view) theme.set<WaveformView>(preset.waveform_view);
    if (preset.has_gauge) theme.set<Gauge>(preset.gauge);
    if (preset.has_ring_indication) theme.set<RingIndication>(preset.ring_indication);
    if (preset.has_busy_wheel) theme.set<BusyWheel>(preset.busy_wheel);
    if (preset.has_console_box) theme.set<ConsoleBox>(preset.console_box);
}

export
inline void apply_baseline_theme_preset(const Style& base) noexcept {
    auto& theme = Theme::instance();
    auto apply_base = [&](Style& s) {
        s.bg_color = base.bg_color;
        s.bg_hover = base.bg_hover;
        s.bg_pressed = base.bg_pressed;
        s.bg_disabled = base.bg_disabled;
        s.padding = base.padding;
        s.corner_radius = base.corner_radius;
        s.border_width = base.border_width;
        s.border_color = base.border_color;
        s.border_hover = base.border_hover;
        s.border_pressed = base.border_pressed;
        s.border_disabled = base.border_disabled;
        s.border_focus = base.border_focus;
        s.font_color = base.font_color;
        s.font_color_disabled = base.font_color_disabled;
        s.scrollbar_margin = base.scrollbar_margin;
        s.scrollbar_thumb_min = base.scrollbar_thumb_min;
        s.accent_color = base.accent_color;
        s.accent_hover = base.accent_hover;
        s.accent_pressed = base.accent_pressed;
        s.accent_disabled = base.accent_disabled;
        s.on_accent = base.on_accent;
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
    preset.has_icon_list = true;
    preset.icon_list = theme.get<IconList>();
    apply_base(preset.icon_list);
    preset.has_text_list = true;
    preset.text_list = theme.get<TextList>();
    apply_base(preset.text_list);
    preset.has_text_tracking_list = true;
    preset.text_tracking_list = theme.get<TextTrackingList>();
    apply_base(preset.text_tracking_list);
    preset.has_progress = true;
    preset.progress = theme.get<Progress>();
    apply_base(preset.progress);
    preset.has_progress_bar_simple = true;
    preset.progress_bar_simple = theme.get<ProgressBarSimple>();
    apply_base(preset.progress_bar_simple);
    preset.has_progress_bar_round = true;
    preset.progress_bar_round = theme.get<ProgressBarRound>();
    apply_base(preset.progress_bar_round);
    preset.has_progress_bar_drill = true;
    preset.progress_bar_drill = theme.get<ProgressBarDrill>();
    apply_base(preset.progress_bar_drill);
    preset.has_progress_flowing = true;
    preset.progress_flowing = theme.get<ProgressFlowing>();
    apply_base(preset.progress_flowing);
    preset.has_progress_wheel = true;
    preset.progress_wheel = theme.get<ProgressWheel>();
    apply_base(preset.progress_wheel);
    preset.has_spinner = true;
    preset.spinner = theme.get<Spinner>();
    apply_base(preset.spinner);
    preset.has_scroll_container = true;
    preset.scroll_container = theme.get<ScrollContainer>();
    apply_base(preset.scroll_container);
    preset.has_scroll_bar = true;
    preset.scroll_bar = theme.get<ScrollBar>();
    apply_base(preset.scroll_bar);
    preset.has_segmented_control = true;
    preset.segmented_control = theme.get<SegmentedControl>();
    apply_base(preset.segmented_control);
    preset.has_slider = true;
    preset.slider = theme.get<Slider>();
    apply_base(preset.slider);
    preset.has_switch = true;
    preset.switcher = theme.get<Switch>();
    apply_base(preset.switcher);
    preset.has_radio = true;
    preset.radio = theme.get<Radio>();
    apply_base(preset.radio);
    preset.has_radio_group = true;
    preset.radio_group = theme.get<RadioGroup>();
    apply_base(preset.radio_group);
    preset.has_text_input = true;
    preset.text_input = theme.get<TextInput>();
    apply_base(preset.text_input);
    preset.has_text_area = true;
    preset.text_area = theme.get<TextArea>();
    apply_base(preset.text_area);
    preset.has_text_box = true;
    preset.text_box = theme.get<TextBox>();
    apply_base(preset.text_box);
    preset.has_number_input = true;
    preset.number_input = theme.get<NumberInput>();
    apply_base(preset.number_input);
    preset.has_number_list = true;
    preset.number_list = theme.get<NumberList>();
    apply_base(preset.number_list);
    preset.has_toggle_group = true;
    preset.toggle_group = theme.get<ToggleGroup>();
    apply_base(preset.toggle_group);
    preset.has_table_view = true;
    preset.table_view = theme.get<TableView>();
    apply_base(preset.table_view);
    preset.has_tree_view = true;
    preset.tree_view = theme.get<TreeView>();
    apply_base(preset.tree_view);
    preset.has_dropdown = true;
    preset.dropdown = theme.get<Dropdown>();
    apply_base(preset.dropdown);
    preset.has_menu = true;
    preset.menu = theme.get<Menu>();
    apply_base(preset.menu);
    preset.has_menu_item = true;
    preset.menu_item = theme.get<MenuItem>();
    apply_base(preset.menu_item);
    preset.has_tab_view = true;
    preset.tab_view = theme.get<TabView>();
    apply_base(preset.tab_view);
    preset.has_roller = true;
    preset.roller = theme.get<Roller>();
    apply_base(preset.roller);
    preset.has_stepper = true;
    preset.stepper = theme.get<Stepper>();
    apply_base(preset.stepper);
    preset.has_perf_overlay = true;
    preset.perf_overlay = theme.get<PerfOverlay>();
    apply_base(preset.perf_overlay);
    preset.has_foldable_panel = true;
    preset.foldable_panel = theme.get<FoldablePanel>();
    apply_base(preset.foldable_panel);
    preset.has_cloudy_glass = true;
    preset.cloudy_glass = theme.get<CloudyGlass>();
    apply_base(preset.cloudy_glass);
    preset.has_dynamic_nebula = true;
    preset.dynamic_nebula = theme.get<DynamicNebula>();
    apply_base(preset.dynamic_nebula);
    preset.has_crt_screen = true;
    preset.crt_screen = theme.get<CrtScreen>();
    apply_base(preset.crt_screen);
    preset.has_spectrum_view = true;
    preset.spectrum_view = theme.get<SpectrumView>();
    apply_base(preset.spectrum_view);
    preset.has_battery_gasgauge = true;
    preset.battery_gasgauge = theme.get<BatteryGasGauge>();
    apply_base(preset.battery_gasgauge);
    preset.has_histogram = true;
    preset.histogram = theme.get<Histogram>();
    apply_base(preset.histogram);
    preset.has_histogram_view = true;
    preset.histogram_view = theme.get<HistogramView>();
    apply_base(preset.histogram_view);
    preset.has_chart = true;
    preset.chart = theme.get<Chart>();
    apply_base(preset.chart);
    preset.has_waveform = true;
    preset.waveform = theme.get<Waveform>();
    apply_base(preset.waveform);
    preset.has_waveform_view = true;
    preset.waveform_view = theme.get<WaveformView>();
    apply_base(preset.waveform_view);
    preset.has_gauge = true;
    preset.gauge = theme.get<Gauge>();
    apply_base(preset.gauge);
    preset.has_ring_indication = true;
    preset.ring_indication = theme.get<RingIndication>();
    apply_base(preset.ring_indication);
    preset.has_busy_wheel = true;
    preset.busy_wheel = theme.get<BusyWheel>();
    apply_base(preset.busy_wheel);
    preset.has_console_box = true;
    preset.console_box = theme.get<ConsoleBox>();
    apply_base(preset.console_box);

    apply_theme_preset(preset);
}

export
inline void apply_tokens_to_all_widgets(const ThemeTokens& tokens) noexcept {
    auto& theme = Theme::instance();
    theme.set_tokens(tokens);

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
    apply_widget(static_cast<IconList*>(nullptr));
    apply_widget(static_cast<TextList*>(nullptr));
    apply_widget(static_cast<TextTrackingList*>(nullptr));
    apply_widget(static_cast<Progress*>(nullptr));
    apply_widget(static_cast<ProgressBarSimple*>(nullptr));
    apply_widget(static_cast<ProgressBarRound*>(nullptr));
    apply_widget(static_cast<ProgressBarDrill*>(nullptr));
    apply_widget(static_cast<ProgressBarFlowing*>(nullptr));
    apply_widget(static_cast<ProgressFlowing*>(nullptr));
    apply_widget(static_cast<ProgressWheel*>(nullptr));
    apply_widget(static_cast<Spinner*>(nullptr));
    apply_widget(static_cast<ScrollContainer*>(nullptr));
    apply_widget(static_cast<ScrollBar*>(nullptr));
    apply_widget(static_cast<SegmentedControl*>(nullptr));
    apply_widget(static_cast<Slider*>(nullptr));
    apply_widget(static_cast<Switch*>(nullptr));
    apply_widget(static_cast<Radio*>(nullptr));
    apply_widget(static_cast<RadioGroup*>(nullptr));
    apply_widget(static_cast<TextInput*>(nullptr));
    apply_widget(static_cast<TextArea*>(nullptr));
    apply_widget(static_cast<TextBox*>(nullptr));
    apply_widget(static_cast<NumberInput*>(nullptr));
    apply_widget(static_cast<NumberList*>(nullptr));
    apply_widget(static_cast<ToggleGroup*>(nullptr));
    apply_widget(static_cast<TableView*>(nullptr));
    apply_widget(static_cast<TreeView*>(nullptr));
    apply_widget(static_cast<Dropdown*>(nullptr));
    apply_widget(static_cast<DropdownPopup*>(nullptr));
    apply_widget(static_cast<Menu*>(nullptr));
    apply_widget(static_cast<MenuItem*>(nullptr));
    apply_widget(static_cast<TabView*>(nullptr));
    apply_widget(static_cast<Roller*>(nullptr));
    apply_widget(static_cast<Stepper*>(nullptr));
    apply_widget(static_cast<Timeline*>(nullptr));
    apply_widget(static_cast<PerfOverlay*>(nullptr));
    apply_widget(static_cast<PopupLayer*>(nullptr));
    apply_widget(static_cast<ModalDialog*>(nullptr));
    apply_widget(static_cast<MessageBox*>(nullptr));
    apply_widget(static_cast<PrimitivesCanvas*>(nullptr));
    apply_widget(static_cast<FoldablePanel*>(nullptr));
    apply_widget(static_cast<CloudyGlass*>(nullptr));
    apply_widget(static_cast<DynamicNebula*>(nullptr));
    apply_widget(static_cast<CrtScreen*>(nullptr));
    apply_widget(static_cast<SpectrumView*>(nullptr));
    apply_widget(static_cast<SpinningWheel*>(nullptr));
    apply_widget(static_cast<SpinZoomWidget*>(nullptr));
    apply_widget(static_cast<MeterPointer*>(nullptr));
    apply_widget(static_cast<Arc*>(nullptr));
    apply_widget(static_cast<Bar*>(nullptr));
    apply_widget(static_cast<Dial*>(nullptr));
    apply_widget(static_cast<Image*>(nullptr));
    apply_widget(static_cast<ImageBox*>(nullptr));
    apply_widget(static_cast<Led*>(nullptr));
    apply_widget(static_cast<BusyWheel*>(nullptr));
    apply_widget(static_cast<ConsoleBox*>(nullptr));
    apply_widget(static_cast<BatteryGauge*>(nullptr));
    apply_widget(static_cast<BatteryGasGauge*>(nullptr));
    apply_widget(static_cast<Histogram*>(nullptr));
    apply_widget(static_cast<HistogramView*>(nullptr));
    apply_widget(static_cast<Chart*>(nullptr));
    apply_widget(static_cast<Waveform*>(nullptr));
    apply_widget(static_cast<WaveformView*>(nullptr));
    apply_widget(static_cast<Gauge*>(nullptr));
    apply_widget(static_cast<RingIndication*>(nullptr));
    apply_widget(static_cast<RichText*>(nullptr));
    apply_widget(static_cast<CodeBlock*>(nullptr));
}

export
inline void apply_theme_tokens(const ThemeTokens& tokens) noexcept {
    auto& theme = Theme::instance();
    theme.set_tokens(tokens);
    apply_tokens_to_all_widgets(tokens);
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
    preset.label = theme.get<Label>();
    preset.label.font_color = kText;

    preset.has_button = true;
    preset.button = theme.get<Button>();
    preset.button.bg_color = kSystemBlue;
    preset.button.bg_hover = kSystemBlueHover;
    preset.button.bg_pressed = kSystemBluePressed;
    preset.button.border_color = kSystemBlue;
    preset.button.border_hover = kSystemBlueHover;
    preset.button.border_pressed = kSystemBluePressed;
    preset.button.border_focus = kSystemBlue;
    preset.button.font_color = kSurfaceElevated;
    preset.button.corner_radius = 10;
    preset.button.padding = 8;

    preset.has_segmented_control = true;
    preset.segmented_control = theme.get<SegmentedControl>();
    preset.segmented_control.bg_color = kSurface;
    preset.segmented_control.border_color = kBorder;
    preset.segmented_control.bg_pressed = kSurfaceElevated;
    preset.segmented_control.border_pressed = kBorder;
    preset.segmented_control.font_color = kText;
    preset.segmented_control.corner_radius = 10;
    preset.segmented_control.padding = 4;

    preset.has_slider = true;
    preset.slider = theme.get<Slider>();
    preset.slider.bg_color = kSystemBlue;
    preset.slider.bg_hover = kSystemBlueHover;
    preset.slider.bg_pressed = kSystemBluePressed;
    preset.slider.border_color = kTrack;
    preset.slider.border_hover = kTrack;
    preset.slider.border_pressed = kTrack;
    preset.slider.font_color = kSurfaceElevated;
    preset.slider.accent_color = kSystemBlue;
    preset.slider.accent_hover = kSystemBlueHover;
    preset.slider.accent_pressed = kSystemBluePressed;
    preset.slider.on_accent = kSurfaceElevated;
    preset.slider.border_focus = kSystemBlue;
    preset.slider.padding = 8;

    preset.has_switch = true;
    preset.switcher = theme.get<Switch>();
    preset.switcher.bg_color = kSurface;
    preset.switcher.border_color = kBorder;
    preset.switcher.bg_pressed = kSystemGreen;
    preset.switcher.border_pressed = kSystemGreen;
    preset.switcher.accent_color = kSystemGreen;
    preset.switcher.on_accent = kSurfaceElevated;
    preset.switcher.border_focus = kSystemBlue;
    preset.switcher.corner_radius = 10;
    preset.switcher.padding = 4;

    preset.has_list_view = true;
    preset.list_view = theme.get<ListView>();
    preset.list_view.scrollbar_margin = 2;
    preset.list_view.scrollbar_thumb_min = 12;

    preset.has_scroll_container = true;
    preset.scroll_container = theme.get<ScrollContainer>();
    preset.scroll_container.scrollbar_margin = 2;
    preset.scroll_container.scrollbar_thumb_min = 12;

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
    preset.label = theme.get<Label>();
    preset.label.font_color = kOnSurface;

    preset.has_button = true;
    preset.button = theme.get<Button>();
    preset.button.bg_color = kPrimary;
    preset.button.bg_hover = kPrimaryHover;
    preset.button.bg_pressed = kPrimaryPressed;
    preset.button.border_color = kPrimary;
    preset.button.border_hover = kPrimaryHover;
    preset.button.border_pressed = kPrimaryPressed;
    preset.button.border_focus = kPrimary;
    preset.button.font_color = kOnPrimary;
    preset.button.corner_radius = 12;
    preset.button.padding = 10;

    preset.has_segmented_control = true;
    preset.segmented_control = theme.get<SegmentedControl>();
    preset.segmented_control.bg_color = kSurfaceVariant;
    preset.segmented_control.border_color = kOutline;
    preset.segmented_control.bg_pressed = kSurface;
    preset.segmented_control.border_pressed = kOutline;
    preset.segmented_control.font_color = kOnSurface;
    preset.segmented_control.corner_radius = 12;
    preset.segmented_control.padding = 4;

    preset.has_slider = true;
    preset.slider = theme.get<Slider>();
    preset.slider.bg_color = kPrimary;
    preset.slider.bg_hover = kPrimaryHover;
    preset.slider.bg_pressed = kPrimaryPressed;
    preset.slider.border_color = kOutline;
    preset.slider.border_hover = kOutline;
    preset.slider.border_pressed = kOutline;
    preset.slider.font_color = kOnPrimary;
    preset.slider.accent_color = kPrimary;
    preset.slider.accent_hover = kPrimaryHover;
    preset.slider.accent_pressed = kPrimaryPressed;
    preset.slider.on_accent = kOnPrimary;
    preset.slider.border_focus = kPrimary;
    preset.slider.padding = 8;

    preset.has_switch = true;
    preset.switcher = theme.get<Switch>();
    preset.switcher.bg_color = kSurfaceVariant;
    preset.switcher.border_color = kOutline;
    preset.switcher.bg_pressed = kPrimary;
    preset.switcher.border_pressed = kPrimary;
    preset.switcher.accent_color = kPrimary;
    preset.switcher.on_accent = kOnPrimary;
    preset.switcher.border_focus = kPrimary;
    preset.switcher.font_color = kOnPrimary;
    preset.switcher.corner_radius = 12;
    preset.switcher.padding = 4;

    preset.has_list_view = true;
    preset.list_view = theme.get<ListView>();
    preset.list_view.scrollbar_margin = 2;
    preset.list_view.scrollbar_thumb_min = 12;

    preset.has_scroll_container = true;
    preset.scroll_container = theme.get<ScrollContainer>();
    preset.scroll_container.scrollbar_margin = 2;
    preset.scroll_container.scrollbar_thumb_min = 12;

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
    preset.label = theme.get<Label>();
    preset.label.font_color = kLvText;

    preset.has_button = true;
    preset.button = theme.get<Button>();
    preset.button.bg_color = kLvBg;
    preset.button.bg_hover = kLvBgHover;
    preset.button.bg_pressed = kLvBgPressed;
    preset.button.border_color = kLvBorder;
    preset.button.border_hover = kLvBorder;
    preset.button.border_pressed = kLvBorder;
    preset.button.border_focus = kLvBorderFocus;
    preset.button.font_color = kLvText;
    preset.button.corner_radius = 6;
    preset.button.padding = 6;

    preset.has_segmented_control = true;
    preset.segmented_control = theme.get<SegmentedControl>();
    preset.segmented_control.bg_color = kLvBg;
    preset.segmented_control.border_color = kLvBorder;
    preset.segmented_control.bg_pressed = kLvBgPressed;
    preset.segmented_control.border_pressed = kLvBorder;
    preset.segmented_control.font_color = kLvText;
    preset.segmented_control.corner_radius = 6;
    preset.segmented_control.padding = 4;

    preset.has_slider = true;
    preset.slider = theme.get<Slider>();
    preset.slider.bg_color = kLvAccent;
    preset.slider.bg_hover = kLvAccent;
    preset.slider.bg_pressed = kLvAccent;
    preset.slider.border_color = kLvBorder;
    preset.slider.border_hover = kLvBorder;
    preset.slider.border_pressed = kLvBorder;
    preset.slider.font_color = kLvText;
    preset.slider.accent_color = kLvAccent;
    preset.slider.on_accent = kLvText;
    preset.slider.border_focus = kLvBorderFocus;
    preset.slider.padding = 6;

    preset.has_switch = true;
    preset.switcher = theme.get<Switch>();
    preset.switcher.bg_color = kLvBg;
    preset.switcher.border_color = kLvBorder;
    preset.switcher.bg_pressed = kLvAccent;
    preset.switcher.border_pressed = kLvAccent;
    preset.switcher.accent_color = kLvAccent;
    preset.switcher.on_accent = kLvText;
    preset.switcher.border_focus = kLvBorderFocus;
    preset.switcher.corner_radius = 6;
    preset.switcher.padding = 4;

    preset.has_list_view = true;
    preset.list_view = theme.get<ListView>();
    preset.list_view.scrollbar_margin = 2;
    preset.list_view.scrollbar_thumb_min = 12;

    preset.has_scroll_container = true;
    preset.scroll_container = theme.get<ScrollContainer>();
    preset.scroll_container.scrollbar_margin = 2;
    preset.scroll_container.scrollbar_thumb_min = 12;

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
    preset.label = theme.get<Label>();
    preset.label.font_color = kText;

    preset.has_button = true;
    preset.button = theme.get<Button>();
    preset.button.bg_color = kPanel;
    preset.button.bg_hover = kPanelHover;
    preset.button.bg_pressed = kPanelPressed;
    preset.button.border_color = kPanelBorder;
    preset.button.border_hover = kPanelBorder;
    preset.button.border_pressed = kAccent;
    preset.button.border_focus = kAccent;
    preset.button.font_color = kText;
    preset.button.corner_radius = 8;
    preset.button.padding = 6;

    preset.has_progress = true;
    preset.progress = theme.get<Progress>();
    preset.progress.bg_color = kPanel;
    preset.progress.border_color = kPanelBorder;
    preset.progress.bg_pressed = kAccent;
    preset.progress.accent_color = kAccent;
    preset.progress.on_accent = kText;
    preset.progress.border_focus = kAccent;
    preset.progress.corner_radius = 6;
    preset.progress.padding = 4;

    preset.has_progress_bar_simple = true;
    preset.progress_bar_simple = theme.get<ProgressBarSimple>();
    preset.progress_bar_simple.bg_color = kPanel;
    preset.progress_bar_simple.border_color = kPanelBorder;
    preset.progress_bar_simple.bg_pressed = kAccent;
    preset.progress_bar_simple.accent_color = kAccent;
    preset.progress_bar_simple.on_accent = kText;
    preset.progress_bar_simple.border_focus = kAccent;
    preset.progress_bar_simple.corner_radius = 6;
    preset.progress_bar_simple.padding = 4;

    preset.has_cloudy_glass = true;
    preset.cloudy_glass = theme.get<CloudyGlass>();
    preset.cloudy_glass.bg_color = kPanel;
    preset.cloudy_glass.border_color = kPanelBorder;
    preset.cloudy_glass.corner_radius = 10;
    preset.cloudy_glass.glass_highlight_pos = 18;
    preset.cloudy_glass.glass_highlight_alpha = 90;
    preset.cloudy_glass.glass_shadow_alpha = 50;
    preset.cloudy_glass.glass_opacity_min = 40;
    preset.cloudy_glass.glass_opacity_max = 190;

    preset.has_dynamic_nebula = true;
    preset.dynamic_nebula = theme.get<DynamicNebula>();
    preset.dynamic_nebula.font_color = kAccentSoft;

    preset.has_busy_wheel = true;
    preset.busy_wheel = theme.get<BusyWheel>();
    preset.busy_wheel.font_color = kAccent;

    apply_theme_preset(preset);
}
