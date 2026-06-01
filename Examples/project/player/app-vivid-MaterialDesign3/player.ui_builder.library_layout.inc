    struct LibraryLayout {
        int tabs_padding{};
        int shuffle_padding{};
        int nav_padding{};
        int top_title_x{};
        int top_title_y{};
        int top_title_h{};
        int top_title_right_gap{};
        int top_button_size{};
        int top_icon_px{};
        int top_button_right_gap{};
        int shuffle_w{};
        int shuffle_icon_px{};
        int list_row_h{};
        int list_header_h{};
        int list_title_h{};
        int list_path_h{};
        int list_title_top_gap{};
        int list_title_left_gap{};
        int list_title_right_gap{};
        int list_sort_w{};
        int list_sort_h{};
        int list_sort_top_offset{};
        int list_sort_right_gap{};
        int list_sort_icon_px{};
        int list_path_top_gap{};
        int list_path_side_gap{};
        int list_path_bg_h_extra{};
        int list_path_bg_radius{};
        int list_path_icon_left_gap{};
        int list_path_icon_top_gap{};
        int list_path_icon_size{};
        int list_path_icon_px{};
        int list_path_text_left{};
        int list_path_text_top{};
        int list_path_text_right_gap{};
        int list_sort_radius{};
        int list_path_icon_radius{};
        int list_body_top_gap{};
        int list_body_bottom_gap{};
        int list_body_side_gap{};
        int list_scroll_w{};
        int list_scroll_right_gap{};
        int list_hint_left_gap{};
        int list_hint_bottom_gap{};
        int list_hint_h{};
        int list_hint_right_gap{};
    };

    static LibraryLayout make_library_layout() noexcept {
        return LibraryLayout{
            .tabs_padding = 0,
            .shuffle_padding = 0,
            .nav_padding = 0,
            .top_title_x = 8,
            .top_title_y = 8,
            .top_title_h = 24,
            .top_title_right_gap = 56,
            .top_button_size = 40,
            .top_icon_px = 18,
            .top_button_right_gap = 46,
            .shuffle_w = 136,
            .shuffle_icon_px = 14,
            .list_row_h = 74,
            .list_header_h = 104,
            .list_title_h = 30,
            .list_path_h = 20,
            .list_title_top_gap = 12,
            .list_title_left_gap = 24,
            .list_title_right_gap = 34,
            .list_sort_w = 84,
            .list_sort_h = 34,
            .list_sort_top_offset = 0,
            .list_sort_right_gap = 88,
            .list_sort_icon_px = 12,
            .list_path_top_gap = 10,
            .list_path_side_gap = 20,
            .list_path_bg_h_extra = 12,
            .list_path_bg_radius = 16,
            .list_path_icon_left_gap = 15,
            .list_path_icon_top_gap = 4,
            .list_path_icon_size = 22,
            .list_path_icon_px = 12,
            .list_path_text_left = 43,
            .list_path_text_top = 4,
            .list_path_text_right_gap = 34,
            .list_sort_radius = 18,
            .list_path_icon_radius = 12,
            .list_body_top_gap = 10,
            .list_body_bottom_gap = 18,
            .list_body_side_gap = 18,
            .list_scroll_w = 10,
            .list_scroll_right_gap = 16,
            .list_hint_left_gap = 22,
            .list_hint_bottom_gap = 18,
            .list_hint_h = 18,
            .list_hint_right_gap = 34,
        };
    }
