module;
export module charm.core.theme_preset;

export import charm.core.style;
export import charm.widgets.button;
export import charm.widgets.label;
export import charm.widgets.list_view;
export import charm.widgets.progress;
export import charm.widgets.scrollbar;
export import charm.widgets.segmented_control;
export import charm.widgets.text_input;
export import charm.widgets.number_input;
export import charm.widgets.toggle_group;
export import charm.widgets.table_view;
export import charm.widgets.tree_view;
export import charm.widgets.perf_overlay;

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
    bool has_scroll_bar{false};
    Style scroll_bar{};
    bool has_segmented_control{false};
    Style segmented_control{};
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
};

export
inline void apply_theme_preset(const ThemePreset& preset) noexcept {
    auto& theme = Theme::instance();
    if (preset.has_label) theme.set<Label>(preset.label);
    if (preset.has_button) theme.set<Button>(preset.button);
    if (preset.has_list_view) theme.set<ListView>(preset.list_view);
    if (preset.has_progress) theme.set<Progress>(preset.progress);
    if (preset.has_scroll_bar) theme.set<ScrollBar>(preset.scroll_bar);
    if (preset.has_segmented_control) theme.set<SegmentedControl>(preset.segmented_control);
    if (preset.has_text_input) theme.set<TextInput>(preset.text_input);
    if (preset.has_number_input) theme.set<NumberInput>(preset.number_input);
    if (preset.has_toggle_group) theme.set<ToggleGroup>(preset.toggle_group);
    if (preset.has_table_view) theme.set<TableView>(preset.table_view);
    if (preset.has_tree_view) theme.set<TreeView>(preset.tree_view);
    if (preset.has_perf_overlay) theme.set<PerfOverlay>(preset.perf_overlay);
}
