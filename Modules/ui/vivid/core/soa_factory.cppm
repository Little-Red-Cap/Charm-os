module;
#include <cstdint>

export module charm.core.soa_factory;

export import charm.core.soa_kernel;
import charm.core.soa_registry;
import charm.core.soa_payload;

export
class SoaFactory {
public:
    explicit SoaFactory(SoaKernel& kernel) noexcept : kernel_(kernel) {}

    WidgetHandle create_container() noexcept { return kernel_.create(WidgetKind::Container); }
    WidgetHandle create_label(const char* text) noexcept {
        auto h = kernel_.create(WidgetKind::Label);
        kernel_.set_text(h, text);
        kernel_.set_hit_testable(h, false);
        return h;
    }
    WidgetHandle create_image() noexcept {
        auto h = kernel_.create(WidgetKind::Image);
        kernel_.set_hit_testable(h, false);
        return h;
    }
    WidgetHandle create_text_input(const char* text) noexcept {
        auto h = kernel_.create(WidgetKind::TextInput);
        kernel_.set_text(h, text);
        kernel_.set_hit_testable(h, false);
        return h;
    }
    WidgetHandle create_text_area(const char* text) noexcept {
        auto h = kernel_.create(WidgetKind::TextArea);
        kernel_.set_text(h, text);
        kernel_.set_hit_testable(h, false);
        return h;
    }
    WidgetHandle create_number_input(const char* text) noexcept {
        auto h = kernel_.create(WidgetKind::NumberInput);
        kernel_.set_text(h, text);
        kernel_.set_hit_testable(h, false);
        return h;
    }
    WidgetHandle create_text_box(const char* text) noexcept {
        auto h = kernel_.create(WidgetKind::TextBox);
        kernel_.set_text(h, text);
        kernel_.set_hit_testable(h, false);
        return h;
    }
    WidgetHandle create_segmented_control() noexcept {
        auto h = kernel_.create(WidgetKind::SegmentedControl);
        kernel_.set_focusable(h, true);
        return h;
    }
    WidgetHandle create_tab_view() noexcept {
        auto h = kernel_.create(WidgetKind::TabView);
        kernel_.set_focusable(h, true);
        return h;
    }
    WidgetHandle create_tab_bar() noexcept {
        return create_tab_view();
    }
    WidgetHandle create_navigation_bar() noexcept {
        auto h = create_tab_view();
        kernel_.set_variant(h, 1);
        return h;
    }
    WidgetHandle create_toggle_group(WidgetKind group_kind = WidgetKind::None) noexcept {
        auto h = kernel_.create(WidgetKind::ToggleGroup);
        kernel_.set_hit_testable(h, false);
        kernel_.set_toggle_group_kind(h, group_kind);
        return h;
    }
    WidgetHandle create_button(const char* text) noexcept {
        auto h = kernel_.create(WidgetKind::Button);
        kernel_.set_text(h, text);
        return h;
    }
    WidgetHandle create_icon_button() noexcept {
        auto h = kernel_.create(WidgetKind::IconButton);
        return h;
    }
    void set_button_icon(WidgetHandle h, soa_detail::ImageId icon) noexcept {
        kernel_.set_button_icon(h, icon);
    }
    void set_button_icon_size(WidgetHandle h, std::uint8_t size) noexcept {
        kernel_.set_button_icon_size(h, size);
    }
    void set_image(WidgetHandle h, soa_detail::ImageId image) noexcept {
        kernel_.set_image(h, image);
    }
    WidgetHandle create_switch() noexcept {
        auto h = kernel_.create(WidgetKind::Switch);
        return h;
    }
    WidgetHandle create_slider() noexcept {
        auto h = kernel_.create(WidgetKind::Slider);
        return h;
    }
    WidgetHandle create_scrollbar() noexcept {
        auto h = kernel_.create(WidgetKind::ScrollBar);
        kernel_.set_focusable(h, true);
        return h;
    }
    WidgetHandle create_scrollbar_for(WidgetHandle target) noexcept {
        auto h = create_scrollbar();
        kernel_.set_scrollbar_target(h, target);
        return h;
    }
    WidgetHandle create_progress() noexcept {
        auto h = kernel_.create(WidgetKind::Progress);
        kernel_.set_hit_testable(h, false);
        return h;
    }
    WidgetHandle create_progress_wheel() noexcept {
        auto h = kernel_.create(WidgetKind::ProgressWheel);
        kernel_.set_hit_testable(h, false);
        return h;
    }
    WidgetHandle create_progress_bar_simple() noexcept {
        auto h = kernel_.create(WidgetKind::ProgressBarSimple);
        kernel_.set_hit_testable(h, false);
        return h;
    }
    WidgetHandle create_progress_bar_round() noexcept {
        auto h = kernel_.create(WidgetKind::ProgressBarRound);
        kernel_.set_hit_testable(h, false);
        return h;
    }
    WidgetHandle create_progress_flowing() noexcept {
        auto h = kernel_.create(WidgetKind::ProgressFlowing);
        kernel_.set_hit_testable(h, false);
        return h;
    }
    WidgetHandle create_spinner() noexcept {
        auto h = kernel_.create(WidgetKind::Spinner);
        kernel_.set_hit_testable(h, false);
        return h;
    }
    void set_spinner_phase(WidgetHandle h, std::uint8_t phase) noexcept {
        kernel_.set_spinner_phase(h, phase);
    }
    WidgetHandle create_checkbox(const char* text) noexcept {
        auto h = kernel_.create(WidgetKind::Checkbox);
        kernel_.set_text(h, text);
        kernel_.set_focusable(h, true);
        return h;
    }
    WidgetHandle create_radio(const char* text) noexcept {
        auto h = kernel_.create(WidgetKind::Radio);
        kernel_.set_text(h, text);
        kernel_.set_focusable(h, true);
        return h;
    }
    WidgetHandle create_list() noexcept {
        auto h = kernel_.create(WidgetKind::List);
        kernel_.set_clip_children(h, true);
        kernel_.set_layout_kind(h, SoaLayoutKind::List);
        return h;
    }
    WidgetHandle create_list_view() noexcept {
        auto h = kernel_.create(WidgetKind::ListView);
        kernel_.set_clip_children(h, true);
        kernel_.set_focusable(h, true);
        kernel_.set_scroll_step(h, 24);
        return h;
    }
    WidgetHandle create_icon_list() noexcept {
        auto h = kernel_.create(WidgetKind::IconList);
        kernel_.set_clip_children(h, true);
        kernel_.set_focusable(h, true);
        kernel_.set_scroll_step(h, 24);
        return h;
    }
    WidgetHandle create_table_view() noexcept {
        auto h = kernel_.create(WidgetKind::TableView);
        kernel_.set_focusable(h, true);
        kernel_.set_scroll_step(h, 24);
        return h;
    }
    WidgetHandle create_tree_view() noexcept {
        auto h = kernel_.create(WidgetKind::TreeView);
        kernel_.set_focusable(h, true);
        kernel_.set_scroll_step(h, 24);
        return h;
    }
    WidgetHandle create_list_item(const char* text) noexcept {
        auto h = kernel_.create(WidgetKind::ListItem);
        kernel_.set_text(h, text);
        kernel_.set_focusable(h, true);
        return h;
    }
    WidgetHandle create_menu() noexcept {
        auto h = kernel_.create(WidgetKind::Menu);
        kernel_.set_clip_children(h, true);
        return h;
    }
    WidgetHandle create_menu_item(const char* text) noexcept {
        auto h = kernel_.create(WidgetKind::MenuItem);
        kernel_.set_text(h, text);
        kernel_.set_focusable(h, true);
        return h;
    }
    WidgetHandle create_text_list() noexcept {
        auto h = kernel_.create(WidgetKind::TextList);
        kernel_.set_clip_children(h, true);
        kernel_.set_focusable(h, true);
        kernel_.set_scroll_step(h, 1);
        return h;
    }
    WidgetHandle create_console_box() noexcept {
        auto h = kernel_.create(WidgetKind::ConsoleBox);
        kernel_.set_clip_children(h, true);
        kernel_.set_focusable(h, false);
        kernel_.set_scroll_step(h, 24);
        kernel_.set_list_row_height(h, 18);
        kernel_.set_console_follow_tail(h, true);
        return h;
    }
    WidgetHandle create_scroll_container() noexcept {
        auto h = kernel_.create(WidgetKind::ScrollContainer);
        kernel_.set_clip_children(h, true);
        kernel_.set_focusable(h, true);
        return h;
    }
    WidgetHandle create_stepper() noexcept {
        auto h = kernel_.create(WidgetKind::Stepper);
        kernel_.set_stepper_count(h, 3);
        kernel_.set_stepper_current(h, 0);
        return h;
    }
    WidgetHandle create_number_list() noexcept {
        auto h = kernel_.create(WidgetKind::NumberList);
        kernel_.set_focusable(h, true);
        kernel_.set_number_list_row_height(h, 28);
        kernel_.set_number_list_wheel_step(h, 28);
        kernel_.set_number_list_count(h, 10);
        kernel_.set_number_list_range(h, 0, 1);
        kernel_.set_number_list_selected(h, 0);
        return h;
    }
    WidgetHandle create_roller() noexcept {
        auto h = kernel_.create(WidgetKind::Roller);
        kernel_.set_focusable(h, true);
        kernel_.set_roller_row_height(h, 24);
        kernel_.set_roller_wheel_step(h, 24);
        return h;
    }

    void set_segmented_count(WidgetHandle h, std::uint8_t count) noexcept {
        kernel_.set_segmented_count(h, count);
    }
    void set_segmented_label(WidgetHandle h, std::uint8_t index, const char* text) noexcept {
        kernel_.set_segmented_label(h, index, text);
    }
    void set_segmented_selected(WidgetHandle h, std::uint8_t index) noexcept {
        kernel_.set_segmented_selected(h, index);
    }
    void set_tab_bar_count(WidgetHandle h, std::uint8_t count) noexcept {
        set_segmented_count(h, count);
    }
    void set_tab_bar_label(WidgetHandle h, std::uint8_t index, const char* text) noexcept {
        set_segmented_label(h, index, text);
    }

    void set_stepper_count(WidgetHandle h, std::uint8_t count) noexcept {
        kernel_.set_stepper_count(h, count);
    }
    void set_stepper_current(WidgetHandle h, std::uint8_t index) noexcept {
        kernel_.set_stepper_current(h, index);
    }
    void set_stepper_label(WidgetHandle h, std::uint8_t index, const char* text) noexcept {
        kernel_.set_stepper_label(h, index, text);
    }
    void set_number_list_count(WidgetHandle h, std::uint16_t count) noexcept {
        kernel_.set_number_list_count(h, count);
    }
    void set_number_list_range(WidgetHandle h, int start, int delta) noexcept {
        kernel_.set_number_list_range(h, start, delta);
    }
    void set_number_list_selected(WidgetHandle h, int index) noexcept {
        kernel_.set_number_list_selected(h, index);
    }
    void set_number_list_row_height(WidgetHandle h, int row_h) noexcept {
        kernel_.set_number_list_row_height(h, row_h);
    }
    void set_number_list_wheel_step(WidgetHandle h, int step) noexcept {
        kernel_.set_number_list_wheel_step(h, step);
    }
    void set_roller_source(WidgetHandle h, std::uint16_t count,
                           const void* ctx, soa_detail::RollerTextFn fn) noexcept {
        kernel_.set_roller_source(h, count, ctx, fn);
    }
    void set_roller_selected(WidgetHandle h, int index) noexcept {
        kernel_.set_roller_selected(h, index);
    }
    void set_roller_row_height(WidgetHandle h, int row_h) noexcept {
        kernel_.set_roller_row_height(h, row_h);
    }
    void set_roller_wheel_step(WidgetHandle h, int step) noexcept {
        kernel_.set_roller_wheel_step(h, step);
    }
    void set_tab_bar_selected(WidgetHandle h, std::uint8_t index) noexcept {
        set_segmented_selected(h, index);
    }
    void set_navigation_bar_count(WidgetHandle h, std::uint8_t count) noexcept {
        set_segmented_count(h, count);
    }
    void set_navigation_bar_label(WidgetHandle h, std::uint8_t index, const char* text) noexcept {
        set_segmented_label(h, index, text);
    }
    void set_navigation_bar_selected(WidgetHandle h, std::uint8_t index) noexcept {
        set_segmented_selected(h, index);
    }
    void set_toggle_group_kind(WidgetHandle h, WidgetKind group_kind) noexcept {
        kernel_.set_toggle_group_kind(h, group_kind);
    }
    void set_text_list_count(WidgetHandle h, std::uint16_t count) noexcept {
        kernel_.set_text_list_count(h, count);
    }
    void set_text_list_item(WidgetHandle h, std::uint16_t index, const char* text) noexcept {
        kernel_.set_text_list_item(h, index, text);
    }
    void set_text_list_selected(WidgetHandle h, int index) noexcept {
        kernel_.set_text_list_selected(h, index);
    }
    void console_clear(WidgetHandle h) noexcept {
        kernel_.console_clear(h);
    }
    void console_append(WidgetHandle h, const char* text) noexcept {
        kernel_.console_append(h, text);
    }
    void set_console_follow_tail(WidgetHandle h, bool follow) noexcept {
        kernel_.set_console_follow_tail(h, follow);
    }
    void set_list_view_source(WidgetHandle h,
                              std::uint16_t count,
                              const void* ctx,
                              soa_detail::ListViewTextFn fn) noexcept {
        kernel_.set_list_view_source(h, count, ctx, fn);
    }
    void set_list_view_icon_source(WidgetHandle h,
                                   const void* ctx,
                                   soa_detail::ListViewIconFn fn,
                                   std::uint8_t icon_size = 0) noexcept {
        kernel_.set_list_view_icon_source(h, ctx, fn, icon_size);
    }
    void set_list_view_items(WidgetHandle h,
                             const char* const* items,
                             std::uint16_t count) noexcept {
        kernel_.set_list_view_source(h, count, items, &SoaFactory::list_view_text_from_array);
    }
    void set_list_view_count(WidgetHandle h, std::uint16_t count) noexcept {
        kernel_.set_list_view_count(h, count);
    }
    void set_list_view_selected(WidgetHandle h, int index) noexcept {
        kernel_.set_list_view_selected(h, index);
    }
    void set_table_view_source(WidgetHandle h,
                               std::uint16_t rows,
                               std::uint8_t cols,
                               const void* ctx,
                               soa_detail::TableViewTextFn fn) noexcept {
        kernel_.set_table_view_source(h, rows, cols, ctx, fn);
    }
    void set_table_view_header(WidgetHandle h,
                               const void* ctx,
                               soa_detail::TableViewHeaderFn fn) noexcept {
        kernel_.set_table_view_header(h, ctx, fn);
    }
    void set_table_view_header_height(WidgetHandle h, int height) noexcept {
        kernel_.set_table_view_header_height(h, height);
    }
    void set_table_view_header_padding(WidgetHandle h, int padding) noexcept {
        kernel_.set_table_view_header_padding(h, padding);
    }
    void set_table_view_header_style(WidgetHandle h, TableViewHeaderStyle style) noexcept {
        kernel_.set_table_view_header_style(h, style);
    }
    void set_table_view_header_divider(WidgetHandle h, bool enabled) noexcept {
        kernel_.set_table_view_header_divider(h, enabled);
    }
    void set_table_view_col_dividers(WidgetHandle h, bool enabled) noexcept {
        kernel_.set_table_view_col_dividers(h, enabled);
    }
    void set_table_view_col_divider_style(WidgetHandle h, TableViewColDividerStyle style) noexcept {
        kernel_.set_table_view_col_divider_style(h, style);
    }
    void set_table_view_count(WidgetHandle h, std::uint16_t rows) noexcept {
        kernel_.set_table_view_count(h, rows);
    }
    void set_table_view_col_width(WidgetHandle h, int col_width) noexcept {
        kernel_.set_table_view_col_width(h, col_width);
    }
    void set_table_view_col_width_fn(WidgetHandle h,
                                     const void* ctx,
                                     soa_detail::TableViewColWidthFn fn) noexcept {
        kernel_.set_table_view_col_width_fn(h, ctx, fn);
    }
    void set_table_view_scroll_x(WidgetHandle h, int x) noexcept {
        kernel_.set_table_view_scroll_x(h, x);
    }
    void set_tree_view_source(WidgetHandle h,
                              std::uint16_t count,
                              const void* text_ctx,
                              soa_detail::TreeViewTextFn text_fn,
                              const void* indent_ctx,
                              soa_detail::TreeViewIndentFn indent_fn) noexcept {
        kernel_.set_tree_view_source(h, count, text_ctx, text_fn, indent_ctx, indent_fn);
    }
    void set_tree_view_count(WidgetHandle h, std::uint16_t count) noexcept {
        kernel_.set_tree_view_count(h, count);
    }
    void set_tree_view_indent_px(WidgetHandle h, std::uint8_t px) noexcept {
        kernel_.set_tree_view_indent_px(h, px);
    }
    void set_tree_view_max_indent_px(WidgetHandle h, int px) noexcept {
        kernel_.set_tree_view_max_indent_px(h, px);
    }
    void set_tree_view_min_text_avail_px(WidgetHandle h, int px) noexcept {
        kernel_.set_tree_view_min_text_avail_px(h, px);
    }

    bool link(WidgetHandle parent, WidgetHandle child) noexcept {
        return kernel_.link(parent, child);
    }

    SoaKernel& kernel() noexcept { return kernel_; }
    const SoaKernel& kernel() const noexcept { return kernel_; }

private:
    static const char* list_view_text_from_array(const void* ctx, std::uint16_t index) noexcept {
        const auto* items = static_cast<const char* const*>(ctx);
        if (!items) return "";
        const char* text = items[index];
        return text ? text : "";
    }

    SoaKernel& kernel_;
};
