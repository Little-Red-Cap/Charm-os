    struct HomeLayout {
        int scroll_bottom_gap{};
        int scroll_step{};
        int scroll_extra{};
        int title_x{};
        int title_y{};
        int title_w{};
        int title_line_h{};
        int title_line_gap{};
        int subtitle_y{};
        int subtitle_h{};
        int play_size{};
        int play_icon_px{};
        int play_y{};
        int collage_x{};
        int collage_y{};
        int collage_w{};
        int collage_h{};
        int collage_big{};
        int collage_small_left{};
        int collage_small_right{};
        int collage_small_center{};
        int stat_card_x{};
        int stat_card_y{};
        int stat_card_w{};
        int stat_card_h{};
        int stat_card_gap{};
        int stat_probe_card_h{};
        int toolbar_button_size{};
        int toolbar_icon_px{};
        int toolbar_button_gap{};
    };

    static HomeLayout make_home_layout(const UiLayout& layout) noexcept {
        const int title_y = layout.top_bar_y + 8;
        const int collage_w = std::clamp(screen_width - 72, 348, 472);
        const int collage_h = std::clamp(collage_w + 8, 340, 430);
        const int collage_big = std::clamp(collage_w - 132, 214, 276);
        const int collage_small_left = std::clamp(collage_w / 4, 78, 100);
        const int collage_small_right = std::clamp(collage_w / 4 + 10, 88, 110);
        const int collage_small_center = std::clamp(collage_w / 5 + 4, 76, 92);
        return HomeLayout{
            .scroll_bottom_gap = 8,
            .scroll_step = 32,
            .scroll_extra = 620,
            .title_x = kUiPadding,
            .title_y = title_y,
            .title_w = screen_width - kUiPadding * 2 - 96,
            .title_line_h = 92,
            .title_line_gap = 76,
            .subtitle_y = title_y + 176,
            .subtitle_h = 28,
            .play_size = 66,
            .play_icon_px = 28,
            .play_y = title_y + 84,
            .collage_x = (screen_width - collage_w) / 2,
            .collage_y = title_y + 206,
            .collage_w = collage_w,
            .collage_h = collage_h,
            .collage_big = collage_big,
            .collage_small_left = collage_small_left,
            .collage_small_right = collage_small_right,
            .collage_small_center = collage_small_center,
            .stat_card_x = kUiPadding,
            .stat_card_y = title_y + 206 + collage_h + 44,
            .stat_card_w = screen_width - kUiPadding * 2,
            .stat_card_h = 134,
            .stat_card_gap = 18,
            .stat_probe_card_h = 256,
            .toolbar_button_size = 36,
            .toolbar_icon_px = 18,
            .toolbar_button_gap = 44,
        };
    }
