module;
export module charm.core.theme_preset;

export import charm.core.style;
export import charm.widgets.button;
export import charm.widgets.label;
export import charm.widgets.list_view;
export import charm.widgets.progress;
export import charm.widgets.progress_bar_simple;
export import charm.widgets.scrollbar;
export import charm.widgets.segmented_control;
export import charm.widgets.slider;
export import charm.widgets.switcher;
export import charm.widgets.text_input;
export import charm.widgets.number_input;
export import charm.widgets.toggle_group;
export import charm.widgets.table_view;
export import charm.widgets.tree_view;
export import charm.widgets.perf_overlay;
export import charm.widgets.foldable_panel;
export import charm.widgets.cloudy_glass;
export import charm.widgets.dynamic_nebula;
export import charm.widgets.crt_screen;
export import charm.widgets.spectrum_view;
export import charm.widgets.battery_gasgauge;
export import charm.widgets.histogram;
export import charm.widgets.busy_wheel;
export import charm.widgets.console_box;

export
struct ThemePreset {
    bool has_label{false};
    Style label{};
    bool has_button{false};
    Style button{};
    bool has_list_view{false};
    Style list_view{};
    bool has_progress{false};
    Style progress{};
    bool has_progress_bar_simple{false};
    Style progress_bar_simple{};
    bool has_scroll_bar{false};
    Style scroll_bar{};
    bool has_segmented_control{false};
    Style segmented_control{};
    bool has_slider{false};
    Style slider{};
    bool has_switch{false};
    Style switcher{};
    bool has_text_input{false};
    Style text_input{};
    bool has_number_input{false};
    Style number_input{};
    bool has_toggle_group{false};
    Style toggle_group{};
    bool has_table_view{false};
    Style table_view{};
    bool has_tree_view{false};
    Style tree_view{};
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
    if (preset.has_list_view) theme.set<ListView>(preset.list_view);
    if (preset.has_progress) theme.set<Progress>(preset.progress);
    if (preset.has_progress_bar_simple) theme.set<ProgressBarSimple>(preset.progress_bar_simple);
    if (preset.has_scroll_bar) theme.set<ScrollBar>(preset.scroll_bar);
    if (preset.has_segmented_control) theme.set<SegmentedControl>(preset.segmented_control);
    if (preset.has_slider) theme.set<Slider>(preset.slider);
    if (preset.has_switch) theme.set<Switch>(preset.switcher);
    if (preset.has_text_input) theme.set<TextInput>(preset.text_input);
    if (preset.has_number_input) theme.set<NumberInput>(preset.number_input);
    if (preset.has_toggle_group) theme.set<ToggleGroup>(preset.toggle_group);
    if (preset.has_table_view) theme.set<TableView>(preset.table_view);
    if (preset.has_tree_view) theme.set<TreeView>(preset.tree_view);
    if (preset.has_perf_overlay) theme.set<PerfOverlay>(preset.perf_overlay);
    if (preset.has_foldable_panel) theme.set<FoldablePanel>(preset.foldable_panel);
    if (preset.has_cloudy_glass) theme.set<CloudyGlass>(preset.cloudy_glass);
    if (preset.has_dynamic_nebula) theme.set<DynamicNebula>(preset.dynamic_nebula);
    if (preset.has_crt_screen) theme.set<CrtScreen>(preset.crt_screen);
    if (preset.has_spectrum_view) theme.set<SpectrumView>(preset.spectrum_view);
    if (preset.has_battery_gasgauge) theme.set<BatteryGasGauge>(preset.battery_gasgauge);
    if (preset.has_histogram) theme.set<Histogram>(preset.histogram);
    if (preset.has_busy_wheel) theme.set<BusyWheel>(preset.busy_wheel);
    if (preset.has_console_box) theme.set<ConsoleBox>(preset.console_box);
}

export
inline void apply_baseline_theme_preset(const Style& base) noexcept {
    auto& theme = Theme::instance();
    auto apply_base = [&](Style& s) {
        s.padding = base.padding;
        s.corner_radius = base.corner_radius;
        s.border_width = base.border_width;
        s.border_color = base.border_color;
        s.border_focus = base.border_focus;
    };

    ThemePreset preset{};
    preset.has_label = true;
    preset.label = theme.get<Label>();
    apply_base(preset.label);
    preset.has_button = true;
    preset.button = theme.get<Button>();
    apply_base(preset.button);
    preset.has_list_view = true;
    preset.list_view = theme.get<ListView>();
    apply_base(preset.list_view);
    preset.has_progress = true;
    preset.progress = theme.get<Progress>();
    apply_base(preset.progress);
    preset.has_progress_bar_simple = true;
    preset.progress_bar_simple = theme.get<ProgressBarSimple>();
    apply_base(preset.progress_bar_simple);
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
    preset.has_text_input = true;
    preset.text_input = theme.get<TextInput>();
    apply_base(preset.text_input);
    preset.has_number_input = true;
    preset.number_input = theme.get<NumberInput>();
    apply_base(preset.number_input);
    preset.has_toggle_group = true;
    preset.toggle_group = theme.get<ToggleGroup>();
    apply_base(preset.toggle_group);
    preset.has_table_view = true;
    preset.table_view = theme.get<TableView>();
    apply_base(preset.table_view);
    preset.has_tree_view = true;
    preset.tree_view = theme.get<TreeView>();
    apply_base(preset.tree_view);
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
    preset.has_busy_wheel = true;
    preset.busy_wheel = theme.get<BusyWheel>();
    apply_base(preset.busy_wheel);
    preset.has_console_box = true;
    preset.console_box = theme.get<ConsoleBox>();
    apply_base(preset.console_box);

    apply_theme_preset(preset);
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
    preset.slider.border_focus = kSystemBlue;
    preset.slider.padding = 8;

    preset.has_switch = true;
    preset.switcher = theme.get<Switch>();
    preset.switcher.bg_color = kSurface;
    preset.switcher.border_color = kBorder;
    preset.switcher.bg_pressed = kSystemGreen;
    preset.switcher.border_pressed = kSystemGreen;
    preset.switcher.border_focus = kSystemBlue;
    preset.switcher.corner_radius = 10;
    preset.switcher.padding = 4;

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
    constexpr rgba kTonal{234, 221, 255, 255};

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
    preset.slider.border_focus = kPrimary;
    preset.slider.padding = 8;

    preset.has_switch = true;
    preset.switcher = theme.get<Switch>();
    preset.switcher.bg_color = kSurfaceVariant;
    preset.switcher.border_color = kOutline;
    preset.switcher.bg_pressed = kPrimary;
    preset.switcher.border_pressed = kPrimary;
    preset.switcher.border_focus = kPrimary;
    preset.switcher.font_color = kOnPrimary;
    preset.switcher.corner_radius = 12;
    preset.switcher.padding = 4;

    // Tonal button feel via hover/pressed on surface variant.
    theme.patch<Button>(StylePatch{
        .has_bg_hover = true,
        .bg_hover = kTonal,
        .has_bg_pressed = true,
        .bg_pressed = kPrimaryPressed,
    });

    apply_theme_preset(preset);
}
