    struct NowTextLayout {
        int text_col_y{};
        int text_col_h{};
        int title_h{};
        int subtitle_h{};
        int progress_h{};
        int time_row_h{};
        int time_row_y{};
        int text_gap{};
    };

    struct NowPlayingLayout {
        int text_padding{};
        int controls_padding{};
        int time_padding{};
        int progress_section_pad_x{};
        int progress_section_pad_y{};
        int progress_section_gap{};
        int progress_section_radius{};
        int progress_section_bg_alpha{};
        int progress_section_border_alpha{};
        int progress_visual_h{};
        int time_row_radius{};
        int time_row_bg_alpha{};
        int time_row_border_alpha{};
        int time_label_inner_pad{};
        int info_tag_radius{};
        int cover_size{};
        int cover_top_gap{};
        int cover_plate_pad{};
        int cover_corner_radius{};
        int title_h{};
        int subtitle_h{};
        int text_gap{};
        int text_after_cover_gap{};
        int progress_after_text_gap{};
        int progress_pad_x_extra{};
        int progress_pad_y_extra{};
        int progress_gap_extra{};
        int time_row_h{};
        int controls_after_progress_gap{};
        int control_side_min{};
        int control_play_min{};
        int control_size_delta{};
        int control_gap_delta{};
        int aux_panel_w{};
        int aux_panel_h{};
        int aux_button{};
        int aux_gap{};
        int aux_after_controls_gap{};
        int aux_inner_pad_x{};
        int aux_inner_pad_top{};
        int aux_icon_px{};
        int collage_small{};
        int collage_tiny{};
        int collage_left_dx{};
        int collage_left_dy{};
        int collage_right_dx{};
        int collage_right_dy{};
        int collage_corner_radius{};
        int collage_shadow_alpha{};
        int collage_shadow_offset_y{};
        int collage_shadow_spread{};
        int collage_shadow_radius{};
        int control_side{};
        int control_play{};
        int control_gap{};
        int control_side_icon{};
        int control_play_icon{};
        int time_side_w{};
        int time_min_info_w{};
        int time_inner_gap{};
        int top_button_size{};
        int top_back_icon{};
        int top_more_icon{};
        int top_title_x{};
        int top_title_y{};
        int top_title_h{};
        int top_title_side_gap{};
        int lyrics_button_w{};
        int cover_debug_gap{};
        int cover_debug_h{};
    };

    struct NowPlayingBuildMetrics {
        int top_bar_y{};
        int top_button{};
        int cover_radius{};
        Rect cover_rect{};
        Rect cover_plate_rect{};
        int title_h{};
        int subtitle_h{};
        int text_gap{};
        Rect text_rect{};
        int progress_h{};
        int progress_pad_x{};
        int progress_pad_y{};
        int progress_gap{};
        int time_row_h{};
        Rect progress_section_rect{};
        Rect progress_visual_rect{};
        Rect progress_hit_rect{};
        Rect time_row_rect{};
        int time_side_w{};
        int time_info_w{};
        int time_row_pad{};
        Rect controls_rect{};
        int small_btn{};
        int play_btn{};
        Rect aux_panel_rect{};
        int aux_button{};
        int aux_gap{};
        Rect top_bar_rect{};
        Rect now_back_rect{};
        Rect now_more_rect{};
        Rect now_lyrics_rect{};
        Rect top_title_rect{};
    };

    struct NowPlayingProgressSpec {
        Rect section{};
        Rect visual{};
        Rect hit{};
        Rect time_row{};
        int time_side_w{};
        int info_w{};
        int row_pad{};
    };

    static NowTextLayout make_now_text_layout(const UiLayout& layout) noexcept {
        const int title_h = 28;
        const int subtitle_h = 14;
        const int progress_h = 12;
        const int time_row_h = 24;
        const int text_gap = 6;
        const int header_top = (layout.top_bar_y + layout.top_bar_h + 12) + kCoverSize;
        const int text_col_y = header_top + 12;
        const int text_col_h = title_h + subtitle_h + text_gap;
        const int time_row_y = text_col_y + text_col_h + 44;

        return NowTextLayout{
            .text_col_y = text_col_y,
            .text_col_h = text_col_h,
            .title_h = title_h,
            .subtitle_h = subtitle_h,
            .progress_h = progress_h,
            .time_row_h = time_row_h,
            .time_row_y = time_row_y,
            .text_gap = text_gap,
        };
    }

    static NowPlayingLayout make_now_playing_layout() noexcept {
        return NowPlayingLayout{
            .text_padding = 0,
            .controls_padding = 0,
            .time_padding = 0,
            .progress_section_pad_x = 14,
            .progress_section_pad_y = 12,
            .progress_section_gap = 8,
            .progress_section_radius = 22,
            .progress_section_bg_alpha = 54,
            .progress_section_border_alpha = 0,
            .progress_visual_h = 16,
            .time_row_radius = 18,
            .time_row_bg_alpha = 46,
            .time_row_border_alpha = 0,
            .time_label_inner_pad = 6,
            .info_tag_radius = 14,
            .cover_size = 392,
            .cover_top_gap = 34,
            .cover_plate_pad = 6,
            .cover_corner_radius = 28,
            .title_h = 46,
            .subtitle_h = 30,
            .text_gap = 8,
            .text_after_cover_gap = 26,
            .progress_after_text_gap = 26,
            .progress_pad_x_extra = 0,
            .progress_pad_y_extra = 0,
            .progress_gap_extra = 0,
            .time_row_h = 32,
            .controls_after_progress_gap = 34,
            .control_side_min = 78,
            .control_play_min = 96,
            .control_size_delta = 0,
            .control_gap_delta = 0,
            .aux_panel_w = 208,
            .aux_panel_h = 60,
            .aux_button = 44,
            .aux_gap = 10,
            .aux_after_controls_gap = 26,
            .aux_inner_pad_x = 8,
            .aux_inner_pad_top = 8,
            .aux_icon_px = 22,
            .collage_small = 64,
            .collage_tiny = 52,
            .collage_left_dx = -18,
            .collage_left_dy = -8,
            .collage_right_dx = 18,
            .collage_right_dy = 28,
            .collage_corner_radius = 12,
            .collage_shadow_alpha = 80,
            .collage_shadow_offset_y = 2,
            .collage_shadow_spread = 2,
            .collage_shadow_radius = 10,
            .control_side = 96,
            .control_play = 116,
            .control_gap = 14,
            .control_side_icon = 30,
            .control_play_icon = 38,
            .time_side_w = 52,
            .time_min_info_w = 170,
            .time_inner_gap = 16,
            .top_button_size = 40,
            .top_back_icon = 22,
            .top_more_icon = 18,
            .top_title_x = 48,
            .top_title_y = 8,
            .top_title_h = 24,
            .top_title_side_gap = 96,
            .lyrics_button_w = 40,
            .cover_debug_gap = 4,
            .cover_debug_h = 12,
        };
    }

    static NowPlayingProgressSpec make_now_playing_progress_spec(const NowPlayingLayout& layout,
                                                                 const Rect& section,
                                                                 int progress_h,
                                                                 int pad_x,
                                                                 int pad_y,
                                                                 int gap,
                                                                 int time_row_h) noexcept {
        constexpr int kMinSeekHitHeight = 36;
        const int track_w = section.w - pad_x * 2;
        const Rect visual{
            pad_x,
            pad_y,
            track_w,
            progress_h,
        };
        const Rect hit{
            pad_x,
            pad_y - (kMinSeekHitHeight - progress_h) / 2,
            track_w,
            std::max(progress_h, kMinSeekHitHeight),
        };
        const Rect time_row{
            pad_x,
            pad_y + progress_h + gap,
            track_w,
            time_row_h,
        };
        const int time_side_w = layout.time_side_w + 4;
        const int available_info_w =
            static_cast<int>(time_row.w) - time_side_w * 2 - layout.time_inner_gap;
        const int info_w = std::max(layout.time_min_info_w, available_info_w);
        const int extra = time_row.w - (time_side_w * 2 + info_w);
        const int row_pad = (extra > 0) ? (extra / 2) : 0;
        return {
            .section = section,
            .visual = visual,
            .hit = hit,
            .time_row = time_row,
            .time_side_w = time_side_w,
            .info_w = info_w,
            .row_pad = row_pad,
        };
    }

    static NowPlayingBuildMetrics make_now_playing_build_metrics(const UiLayout& layout,
                                                                const NowPlayingLayout& now_layout) noexcept {
        const auto expand_rect = [](const Rect& rect, int pad) noexcept -> Rect {
            return {rect.x - pad, rect.y - pad, rect.w + pad * 2, rect.h + pad * 2};
        };

        const int top_bar_y = layout.top_bar_y + 8;
        const int top_button = now_layout.top_button_size;
        const int cover_size = now_layout.cover_size;
        const int cover_radius = now_layout.cover_corner_radius + 4;
        const Rect cover_rect{
            (screen_width - cover_size) / 2,
            top_bar_y + top_button + now_layout.cover_top_gap,
            cover_size,
            cover_size,
        };
        const Rect cover_plate_rect = expand_rect(cover_rect, now_layout.cover_plate_pad);

        const int title_h = now_layout.title_h;
        const int subtitle_h = now_layout.subtitle_h;
        const int text_gap = now_layout.text_gap;
        const int text_col_y = cover_rect.y + cover_rect.h + now_layout.text_after_cover_gap;
        const int text_col_h = title_h + text_gap + subtitle_h;
        const Rect text_rect{
            kUiPadding,
            text_col_y,
            screen_width - kUiPadding * 2,
            text_col_h,
        };

        const int progress_h = now_layout.progress_visual_h;
        const int progress_pad_x = now_layout.progress_section_pad_x + now_layout.progress_pad_x_extra;
        const int progress_pad_y = now_layout.progress_section_pad_y + now_layout.progress_pad_y_extra;
        const int progress_gap = now_layout.progress_section_gap + now_layout.progress_gap_extra;
        const int time_row_h = now_layout.time_row_h;
        const int progress_section_w = screen_width - kUiPadding * 2;
        const int progress_section_y = text_col_y + text_col_h + now_layout.progress_after_text_gap;
        const int progress_section_h = progress_pad_y * 2 + progress_h + progress_gap + time_row_h;
        const Rect progress_section_rect{
            kUiPadding,
            progress_section_y,
            progress_section_w,
            progress_section_h,
        };
        const NowPlayingProgressSpec progress_spec =
            make_now_playing_progress_spec(now_layout,
                                           progress_section_rect,
                                           progress_h,
                                           progress_pad_x,
                                           progress_pad_y,
                                           progress_gap,
                                           time_row_h);

        const int small_btn = std::max(now_layout.control_side_min,
                                       now_layout.control_side + now_layout.control_size_delta);
        const int play_btn = std::max(now_layout.control_play_min,
                                      now_layout.control_play + now_layout.control_size_delta);
        const int controls_gap = std::max(10, now_layout.control_gap + now_layout.control_gap_delta);
        const int controls_w = small_btn * 2 + play_btn + controls_gap * 2;
        const int controls_x = (screen_width - controls_w) / 2;
        const int controls_y = progress_section_y + progress_section_h + now_layout.controls_after_progress_gap;
        const Rect controls_rect{controls_x, controls_y, controls_w, play_btn};

        const int aux_panel_w = now_layout.aux_panel_w;
        const int aux_panel_h = now_layout.aux_panel_h;
        const int aux_button = now_layout.aux_button;
        const int aux_gap = now_layout.aux_gap;
        const int aux_panel_x = (screen_width - aux_panel_w) / 2;
        const int aux_panel_y = controls_y + play_btn + now_layout.aux_after_controls_gap;
        const Rect aux_panel_rect{aux_panel_x, aux_panel_y, aux_panel_w, aux_panel_h};

        const Rect top_bar_rect{0, top_bar_y - 4, screen_width, top_button + 8};
        const Rect now_back_rect{kUiPadding, top_bar_y, top_button, top_button};
        const int lyrics_w = 86;
        const Rect now_more_rect{screen_width - kUiPadding - top_button, top_bar_y, top_button, top_button};
        const Rect now_lyrics_rect{now_more_rect.x - 12 - lyrics_w, top_bar_y, lyrics_w, top_button};
        const Rect top_title_rect{
            now_back_rect.x + now_back_rect.w + 18,
            top_bar_y + 4,
            now_lyrics_rect.x - (now_back_rect.x + now_back_rect.w + 32),
            28,
        };

        return {
            .top_bar_y = top_bar_y,
            .top_button = top_button,
            .cover_radius = cover_radius,
            .cover_rect = cover_rect,
            .cover_plate_rect = cover_plate_rect,
            .title_h = title_h,
            .subtitle_h = subtitle_h,
            .text_gap = text_gap,
            .text_rect = text_rect,
            .progress_h = progress_h,
            .progress_pad_x = progress_pad_x,
            .progress_pad_y = progress_pad_y,
            .progress_gap = progress_gap,
            .time_row_h = time_row_h,
            .progress_section_rect = progress_section_rect,
            .progress_visual_rect = progress_spec.visual,
            .progress_hit_rect = progress_spec.hit,
            .time_row_rect = progress_spec.time_row,
            .time_side_w = progress_spec.time_side_w,
            .time_info_w = progress_spec.info_w,
            .time_row_pad = progress_spec.row_pad,
            .controls_rect = controls_rect,
            .small_btn = small_btn,
            .play_btn = play_btn,
            .aux_panel_rect = aux_panel_rect,
            .aux_button = aux_button,
            .aux_gap = aux_gap,
            .top_bar_rect = top_bar_rect,
            .now_back_rect = now_back_rect,
            .now_more_rect = now_more_rect,
            .now_lyrics_rect = now_lyrics_rect,
            .top_title_rect = top_title_rect,
        };
    }
