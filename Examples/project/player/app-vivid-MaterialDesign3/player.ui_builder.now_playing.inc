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

    static NowPlayingBuildMetrics make_now_playing_build_metrics(const UiLayout& layout,
                                                                const NowPlayingLayout& now_layout) noexcept {
        const auto expand_rect = [](const Rect& rect, int pad) noexcept -> Rect {
            return {rect.x - pad, rect.y - pad, rect.w + pad * 2, rect.h + pad * 2};
        };

        const int top_bar_y = layout.top_bar_y + 8;
        const int top_button = now_layout.top_button_size;
        const int cover_size = 392;
        const int cover_radius = now_layout.cover_corner_radius + 4;
        const Rect cover_rect{
            (screen_width - cover_size) / 2,
            top_bar_y + top_button + 34,
            cover_size,
            cover_size,
        };
        const Rect cover_plate_rect = expand_rect(cover_rect, 6);

        const int title_h = 46;
        const int subtitle_h = 30;
        const int text_gap = 8;
        const int text_col_y = cover_rect.y + cover_rect.h + 26;
        const int text_col_h = title_h + text_gap + subtitle_h;
        const Rect text_rect{
            kUiPadding,
            text_col_y,
            screen_width - kUiPadding * 2,
            text_col_h,
        };

        const int progress_h = 18;
        const int progress_pad_x = now_layout.progress_section_pad_x + 4;
        const int progress_pad_y = now_layout.progress_section_pad_y + 2;
        const int progress_gap = now_layout.progress_section_gap + 4;
        const int time_row_h = 32;
        const int progress_section_w = screen_width - kUiPadding * 2;
        const int progress_section_y = text_col_y + text_col_h + 26;
        const int progress_section_h = progress_pad_y * 2 + progress_h + progress_gap + time_row_h;
        const Rect progress_section_rect{
            kUiPadding,
            progress_section_y,
            progress_section_w,
            progress_section_h,
        };

        const int small_btn = now_layout.control_side;
        const int play_btn = now_layout.control_play;
        const int controls_w = small_btn * 2 + play_btn + now_layout.control_gap * 2;
        const int controls_x = (screen_width - controls_w) / 2;
        const int controls_y = progress_section_y + progress_section_h + 34;
        const Rect controls_rect{controls_x, controls_y, controls_w, play_btn};

        const int aux_panel_w = 224;
        const int aux_panel_h = 68;
        const int aux_button = 52;
        const int aux_gap = 12;
        const int aux_panel_x = (screen_width - aux_panel_w) / 2;
        const int aux_panel_y = controls_y + play_btn + 30;
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

    static WidgetHandle build_now_playing_root(::ui::scene::SceneBuilder& builder, UiHandles& h) {
        const WidgetHandle now_playing = builder.create_container();
        anchor_rect(builder, now_playing, {0, 0, screen_width, screen_height});
        h.page_now_playing = now_playing;

        h.now_backdrop = builder.create_container();
        anchor_rect(builder, h.now_backdrop, {0, 0, screen_width, screen_height});
        {
            StylePatch patch{};
            patch.has_bg_color = true;
            patch.bg_color = kUiBackdropBase;
            patch.has_border_color = true;
            patch.border_color = {0, 0, 0, 0};
            patch.has_border_width = true;
            patch.border_width = 0;
            patch.has_corner_radius = true;
            patch.corner_radius = 0;
            builder.set_style_override(h.now_backdrop, patch);
        }

        builder.link(h.root, now_playing);
        builder.link(now_playing, h.now_backdrop);
        return now_playing;
    }

    static void create_now_playing_hidden_widgets(::ui::scene::SceneBuilder& builder,
                                                  WidgetHandle now_playing,
                                                  UiHandles& h) {
        h.status = builder.create_label_static("");
        anchor_rect(builder, h.status, {0, 0, 0, 0});

        h.mode_hint = builder.create_label_static("");
        anchor_rect(builder, h.mode_hint, {0, 0, 0, 0});

        h.spectrum = builder.create_container();
        anchor_rect(builder, h.spectrum, {0, 0, 0, 0});
        builder.set_hit_testable(h.spectrum, false);

        h.eq_panel = builder.create_container();
        anchor_rect(builder, h.eq_panel, {0, 0, 0, 0});
        builder.set_hit_testable(h.eq_panel, false);

        h.eq_title = builder.create_label_static("EQ");
        anchor_rect(builder, h.eq_title, {0, 0, 0, 0});

        constexpr std::array<const char*, kEqBands> kEqLabels{
            "60", "250", "1K", "4K", "16K"
        };
        for (std::size_t i = 0; i < kEqBands; ++i) {
            h.eq_labels[i] = builder.create_label_static(kEqLabels[i]);
            anchor_rect(builder, h.eq_labels[i], {0, 0, 0, 0});

            h.eq_sliders[i] = builder.create_slider();
            anchor_rect(builder, h.eq_sliders[i], {0, 0, 0, 0});
            builder.set_range(h.eq_sliders[i], -12, 12);
            builder.set_value(h.eq_sliders[i], 0);

            h.eq_values[i] = builder.create_label_static("");
            anchor_rect(builder, h.eq_values[i], {0, 0, 0, 0});
        }

        h.volume_label = builder.create_label_static("Vol");
        anchor_rect(builder, h.volume_label, {0, 0, 0, 0});
        h.volume_slider = builder.create_slider();
        anchor_rect(builder, h.volume_slider, {0, 0, 0, 0});
        builder.set_range(h.volume_slider, 0, 100);
        builder.set_value(h.volume_slider, 80);
        h.volume_value = builder.create_label_static("");
        anchor_rect(builder, h.volume_value, {0, 0, 0, 0});

        h.dc_label = builder.create_label_static("DC");
        anchor_rect(builder, h.dc_label, {0, 0, 0, 0});
        h.dc_switch = builder.create_switch();
        anchor_rect(builder, h.dc_switch, {0, 0, 0, 0});
        builder.set_checked(h.dc_switch, true);

        h.clip_label = builder.create_label_static("Clip");
        anchor_rect(builder, h.clip_label, {0, 0, 0, 0});
        h.clip_switch = builder.create_switch();
        anchor_rect(builder, h.clip_switch, {0, 0, 0, 0});
        builder.set_checked(h.clip_switch, true);
        h.clip_slider = builder.create_slider();
        anchor_rect(builder, h.clip_slider, {0, 0, 0, 0});
        builder.set_range(h.clip_slider, 60, 100);
        builder.set_value(h.clip_slider, 85);
        h.clip_value = builder.create_label_static("");
        anchor_rect(builder, h.clip_value, {0, 0, 0, 0});

        builder.link(now_playing, h.status);
        builder.link(now_playing, h.mode_hint);
        builder.link(now_playing, h.spectrum);
        builder.link(now_playing, h.eq_panel);
        builder.link(now_playing, h.eq_title);
        for (std::size_t i = 0; i < kEqBands; ++i) {
            builder.link(now_playing, h.eq_labels[i]);
            builder.link(now_playing, h.eq_sliders[i]);
            builder.link(now_playing, h.eq_values[i]);
        }
        builder.link(now_playing, h.volume_label);
        builder.link(now_playing, h.volume_slider);
        builder.link(now_playing, h.volume_value);
        builder.link(now_playing, h.dc_label);
        builder.link(now_playing, h.dc_switch);
        builder.link(now_playing, h.clip_label);
        builder.link(now_playing, h.clip_switch);
        builder.link(now_playing, h.clip_slider);
        builder.link(now_playing, h.clip_value);
    }

    static void build_now_playing_cover_area(::ui::scene::SceneBuilder& builder,
                                             WidgetHandle now_playing,
                                             UiHandles& h,
                                             const NowPlayingLayout& now_layout,
                                             const NowPlayingBuildMetrics& metrics) {
        h.now_cover_plate = builder.create_container();
        anchor_rect(builder, h.now_cover_plate, metrics.cover_plate_rect);
        {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_corner_radius = true,
                .bg_color = {kUiInfoTagBg.r, kUiInfoTagBg.g, kUiInfoTagBg.b, 0},
                .border_color = {kUiTitle.r, kUiTitle.g, kUiTitle.b, 0},
                .corner_radius = metrics.cover_radius + 10,
            });
            builder.set_style_override(h.now_cover_plate, patch);
            builder.set_hit_testable(h.now_cover_plate, false);
        }
        builder.link(now_playing, h.now_cover_plate);

        h.cover = builder.create_image();
        anchor_rect(builder, h.cover, metrics.cover_rect);
        apply_now_cover_style(builder, h.cover, metrics.cover_radius, false, 0, 0, 0, 0);
        builder.link(now_playing, h.cover);

        constexpr bool kShowCoverCollage = false;
        if constexpr (kShowCoverCollage) {
            const int collage_small = now_layout.collage_small;
            const int collage_tiny = now_layout.collage_tiny;

            h.cover_left = builder.create_image();
            anchor_rect(builder, h.cover_left, {metrics.cover_rect.x + now_layout.collage_left_dx,
                                                metrics.cover_rect.y + metrics.cover_rect.h - collage_small + now_layout.collage_left_dy,
                                                collage_small,
                                                collage_small});
            apply_now_cover_style(builder, h.cover_left,
                                  now_layout.collage_corner_radius,
                                  true,
                                  static_cast<std::uint8_t>(now_layout.collage_shadow_alpha),
                                  now_layout.collage_shadow_offset_y,
                                  now_layout.collage_shadow_spread,
                                  now_layout.collage_shadow_radius);
            builder.link(now_playing, h.cover_left);

            h.cover_right = builder.create_image();
            anchor_rect(builder, h.cover_right, {metrics.cover_rect.x + metrics.cover_rect.w - collage_tiny + now_layout.collage_right_dx,
                                                 metrics.cover_rect.y + metrics.cover_rect.h / 2 + now_layout.collage_right_dy,
                                                 collage_tiny,
                                                 collage_tiny});
            apply_now_cover_style(builder, h.cover_right,
                                  now_layout.collage_corner_radius,
                                  true,
                                  static_cast<std::uint8_t>(now_layout.collage_shadow_alpha),
                                  now_layout.collage_shadow_offset_y,
                                  now_layout.collage_shadow_spread,
                                  now_layout.collage_shadow_radius);
            builder.link(now_playing, h.cover_right);
        }
    }

    static void build_now_playing_text_area(::ui::scene::SceneBuilder& builder,
                                            WidgetHandle now_playing,
                                            UiHandles& h,
                                            const NowPlayingLayout& now_layout,
                                            const NowPlayingBuildMetrics& metrics) {
        h.title = builder.create_label_static("");
        {
            StylePatch patch = make_player_text_patch(PlayerTextRole::PageTitle, kUiTitle);
            patch.has_font = true;
            patch.font = &get_player_font_px(34, FontWeight::Bold);
            builder.set_style_override(h.title, patch);
        }
        builder.set_label_align(h.title, ::ui::scene::TextAlignH::Center, ::ui::scene::TextAlignV::Center);
        builder.set_hit_testable(h.title, false);

        h.subtitle = builder.create_label_static("");
        {
            StylePatch patch = make_player_text_patch(PlayerTextRole::PageSubtitle, kUiSubtitle);
            patch.has_font = true;
            patch.font = &get_player_font_px(22, FontWeight::Medium);
            builder.set_style_override(h.subtitle, patch);
        }
        builder.set_label_align(h.subtitle, ::ui::scene::TextAlignH::Center, ::ui::scene::TextAlignV::Center);
        builder.set_hit_testable(h.subtitle, false);

        ::ui::scene::ColumnBuilder text_col{builder,
                                            metrics.text_rect,
                                            metrics.text_gap,
                                            now_layout.text_padding,
                                            ::ui::scene::LayoutAlign::Start};
        h.now_text_group = text_col.root();
        text_col.add(h.title, metrics.text_rect.w, metrics.title_h);
        text_col.add(h.subtitle, metrics.text_rect.w, metrics.subtitle_h);
        builder.link(now_playing, h.now_text_group);
    }

    static void build_now_playing_progress_area(::ui::scene::SceneBuilder& builder,
                                                WidgetHandle now_playing,
                                                UiHandles& h,
                                                const NowPlayingLayout& now_layout,
                                                const NowPlayingBuildMetrics& metrics) {
        h.progress_visual = builder.create_slider();
        builder.set_range(h.progress_visual, 0, 100);
        builder.set_value(h.progress_visual, 0);
        builder.set_hit_testable(h.progress_visual, false);
        {
            StylePatch patch = ::ui::scene::make_seek_bar_style_patch({
                .bg_color = kUiProgressBg,
                .track_color = kUiProgressBorder,
                .fill_color = kUiSwitchOn,
                .padding = 3,
                .corner_radius = 9,
            });
            builder.set_style_override(h.progress_visual, patch);
        }

        h.progress = builder.create_container();
        builder.set_hit_testable(h.progress, true);

        h.time_left = builder.create_label_static("");
        h.time_right = builder.create_label_static("");
        {
            StylePatch patch = make_player_text_patch(PlayerTextRole::MetaText, kUiTime);
            patch.has_font = true;
            patch.font = &get_player_font_px(18, FontWeight::Medium);
            patch.has_padding = true;
            patch.padding = now_layout.time_label_inner_pad + 2;
            builder.set_style_override(h.time_left, patch);
            builder.set_style_override(h.time_right, patch);
        }
        builder.set_label_align(h.time_left, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        builder.set_label_align(h.time_right, ::ui::scene::TextAlignH::Right, ::ui::scene::TextAlignV::Center);

        h.info_tag = builder.create_button_static("");
        {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_corner_radius = true,
                .bg_color = {kUiInfoTagBg.r, kUiInfoTagBg.g, kUiInfoTagBg.b, 255},
                .border_color = {kUiSwitchOn.r, kUiSwitchOn.g, kUiSwitchOn.b, 0},
                .corner_radius = now_layout.info_tag_radius,
            });
            patch.has_font = true;
            patch.font = &get_player_font_px(18, FontWeight::Medium);
            patch.has_font_color = true;
            patch.font_color = kUiTitle;
            patch.has_padding = true;
            patch.padding = 8;
            builder.set_style_override(h.info_tag, patch);
        }
        builder.set_hit_testable(h.info_tag, false);

        h.now_progress_group = builder.create_container();
        anchor_rect(builder, h.now_progress_group, metrics.progress_section_rect);
        {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_corner_radius = true,
                .bg_color = {kUiInfoTagBg.r, kUiInfoTagBg.g, kUiInfoTagBg.b, 54},
                .border_color = {kUiInfoTagBorderSoft.r, kUiInfoTagBorderSoft.g, kUiInfoTagBorderSoft.b, 0},
                .corner_radius = now_layout.progress_section_radius + 4,
            });
            builder.set_style_override(h.now_progress_group, patch);
        }

        anchor_rect(builder, h.progress_visual,
                    {metrics.progress_pad_x,
                     metrics.progress_pad_y,
                     metrics.progress_section_rect.w - metrics.progress_pad_x * 2,
                     metrics.progress_h});
        anchor_rect(builder, h.progress,
                    {metrics.progress_pad_x,
                     metrics.progress_pad_y,
                     metrics.progress_section_rect.w - metrics.progress_pad_x * 2,
                     std::max(metrics.progress_h, 26)});
        builder.link(h.now_progress_group, h.progress_visual);
        builder.link(h.now_progress_group, h.progress);

        const int time_row_w = metrics.progress_section_rect.w - metrics.progress_pad_x * 2;
        const int time_side_w = now_layout.time_side_w + 4;
        const int info_w = std::max(now_layout.time_min_info_w,
                                    time_row_w - time_side_w * 2 - now_layout.time_inner_gap);
        const int extra = time_row_w - (time_side_w * 2 + info_w);
        const int row_pad = (extra > 0) ? (extra / 2) : 0;
        const Rect time_row_rect{
            metrics.progress_pad_x,
            metrics.progress_pad_y + metrics.progress_h + metrics.progress_gap,
            time_row_w,
            metrics.time_row_h,
        };
        ::ui::scene::RowBuilder time_row{builder,
                                         time_row_rect,
                                         0,
                                         row_pad + now_layout.time_padding,
                                         ::ui::scene::LayoutAlign::Center};
        const WidgetHandle time_row_root = time_row.root();
        {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_corner_radius = true,
                .bg_color = {kUiProgressBg.r, kUiProgressBg.g, kUiProgressBg.b, 46},
                .border_color = {kUiInfoTagBorderSoft.r, kUiInfoTagBorderSoft.g, kUiInfoTagBorderSoft.b, 0},
                .corner_radius = now_layout.time_row_radius,
            });
            builder.set_style_override(time_row_root, patch);
        }
        time_row.add(h.time_left, time_side_w, metrics.time_row_h);
        time_row.add(h.info_tag, info_w, metrics.time_row_h);
        time_row.add(h.time_right, time_side_w, metrics.time_row_h);

        builder.link(h.now_progress_group, time_row_root);
        builder.link(now_playing, h.now_progress_group);
    }

    static void build_now_playing_controls_area(::ui::scene::SceneBuilder& builder,
                                                WidgetHandle now_playing,
                                                UiHandles& h,
                                                const NowPlayingLayout& now_layout,
                                                const NowPlayingBuildMetrics& metrics,
                                                const PlayerIconIds& icons) {
        ::ui::scene::RowBuilder controls_row{builder,
                                             metrics.controls_rect,
                                             now_layout.control_gap,
                                             now_layout.controls_padding,
                                             ::ui::scene::LayoutAlign::Center};
        h.controls = controls_row.root();

        h.btn_prev = builder.create_button_static("");
        controls_row.add(h.btn_prev, metrics.small_btn, metrics.small_btn);
        builder.set_button_icon(h.btn_prev, icons.prev);
        builder.set_button_icon_size(h.btn_prev, now_layout.control_side_icon + 2);
        apply_control_side_style(builder, h.btn_prev, metrics.small_btn, now_layout.control_side_icon + 2);
        apply_clean_button_shell_style(builder, h.btn_prev,
                                       (metrics.small_btn - (now_layout.control_side_icon + 2)) / 2, 30);

        h.btn_pause = builder.create_button_static("");
        controls_row.add(h.btn_pause, metrics.play_btn, metrics.play_btn);
        builder.set_button_icon(h.btn_pause, icons.play);
        builder.set_button_icon_size(h.btn_pause, now_layout.control_play_icon + 2);
        apply_control_play_style(builder, h.btn_pause, metrics.play_btn, now_layout.control_play_icon + 2);
        apply_clean_button_shell_style(builder, h.btn_pause,
                                       (metrics.play_btn - (now_layout.control_play_icon + 2)) / 2, 34);

        h.btn_next = builder.create_button_static("");
        controls_row.add(h.btn_next, metrics.small_btn, metrics.small_btn);
        builder.set_button_icon(h.btn_next, icons.next);
        builder.set_button_icon_size(h.btn_next, now_layout.control_side_icon + 2);
        apply_control_side_style(builder, h.btn_next, metrics.small_btn, now_layout.control_side_icon + 2);
        apply_clean_button_shell_style(builder, h.btn_next,
                                       (metrics.small_btn - (now_layout.control_side_icon + 2)) / 2, 30);

        constexpr bool kShowModeButton = false;
        if constexpr (kShowModeButton) {
            h.btn_mode = builder.create_button_static("");
            anchor_rect(builder, h.btn_mode, {0, 0, 0, 0});
            builder.link(h.controls, h.btn_mode);
        }

        builder.link(now_playing, h.controls);
    }

    static void build_now_playing_aux_area(::ui::scene::SceneBuilder& builder,
                                           WidgetHandle now_playing,
                                           UiHandles& h,
                                           const NowPlayingBuildMetrics& metrics,
                                           const PlayerIconIds& icons) {
        h.now_aux_group = builder.create_container();
        anchor_rect(builder, h.now_aux_group, metrics.aux_panel_rect);
        {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_corner_radius = true,
                .bg_color = {kUiInfoTagBg.r, kUiInfoTagBg.g, kUiInfoTagBg.b, 58},
                .border_color = {kUiInfoTagBorderSoft.r, kUiInfoTagBorderSoft.g, kUiInfoTagBorderSoft.b, 0},
                .corner_radius = metrics.aux_panel_rect.h / 2,
            });
            builder.set_style_override(h.now_aux_group, patch);
            builder.set_hit_testable(h.now_aux_group, false);
        }

        ::ui::scene::RowBuilder aux_row{builder,
                                        {8, 8, metrics.aux_panel_rect.w - 16, metrics.aux_button},
                                        metrics.aux_gap,
                                        0,
                                        ::ui::scene::LayoutAlign::Center};
        const WidgetHandle aux_actions_root = aux_row.root();

        const auto style_aux_button = [&](WidgetHandle button, int icon_px, bool interactive) {
            apply_control_side_style(builder, button, metrics.aux_button, icon_px);
            apply_clean_button_shell_style(builder, button, 12, 18);
            builder.set_hit_testable(button, interactive);
        };

        const int aux_icon = 22;
        const WidgetHandle aux_shuffle = builder.create_button_static("");
        aux_row.add(aux_shuffle, metrics.aux_button, metrics.aux_button);
        builder.set_button_icon(aux_shuffle, icons.shuffle);
        builder.set_button_icon_size(aux_shuffle, aux_icon);
        style_aux_button(aux_shuffle, aux_icon, false);

        h.btn_mode = builder.create_button_static("");
        aux_row.add(h.btn_mode, metrics.aux_button, metrics.aux_button);
        builder.set_button_icon(h.btn_mode, icons.loop);
        builder.set_button_icon_size(h.btn_mode, aux_icon);
        style_aux_button(h.btn_mode, aux_icon, true);

        const WidgetHandle aux_more = builder.create_button_static("");
        aux_row.add(aux_more, metrics.aux_button, metrics.aux_button);
        builder.set_button_icon(aux_more, icons.more);
        builder.set_button_icon_size(aux_more, aux_icon);
        style_aux_button(aux_more, aux_icon, false);

        builder.link(now_playing, h.now_aux_group);
        builder.link(h.now_aux_group, aux_actions_root);
    }

    static void build_now_playing_top_bar(::ui::scene::SceneBuilder& builder,
                                          WidgetHandle now_playing,
                                          UiHandles& h,
                                          const NowPlayingLayout& now_layout,
                                          const NowPlayingBuildMetrics& metrics,
                                          const PlayerIconIds& icons) {
        h.now_top_bar = builder.create_container();
        anchor_rect(builder, h.now_top_bar, metrics.top_bar_rect);

        h.now_back = builder.create_button_static("");
        anchor_rect(builder, h.now_back, metrics.now_back_rect);
        builder.set_button_icon(h.now_back, icons.down);
        builder.set_button_icon_size(h.now_back, now_layout.top_back_icon + 2);
        apply_top_bar_button_style(builder, h.now_back);
        apply_clean_button_shell_style(builder, h.now_back, 9, 20);

        const WidgetHandle top_title = builder.create_label_static("Now Playing");
        anchor_rect(builder, top_title, metrics.top_title_rect);
        {
            StylePatch patch = make_player_text_patch(PlayerTextRole::PageSubtitle, kUiTitle);
            patch.has_font = true;
            patch.font = &get_player_font_px(20, FontWeight::Medium);
            builder.set_style_override(top_title, patch);
        }
        builder.set_label_align(top_title, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        builder.set_hit_testable(top_title, false);

        h.now_more = builder.create_button_static("");
        anchor_rect(builder, h.now_more, metrics.now_more_rect);
        builder.set_button_icon(h.now_more, icons.more);
        builder.set_button_icon_size(h.now_more, now_layout.top_more_icon + 2);
        apply_top_bar_button_style(builder, h.now_more);
        apply_clean_button_shell_style(builder, h.now_more, 11, 20);

        h.now_lyrics = builder.create_button_static("Lyrics");
        anchor_rect(builder, h.now_lyrics, metrics.now_lyrics_rect);
        builder.set_label_align(h.now_lyrics, ::ui::scene::TextAlignH::Center, ::ui::scene::TextAlignV::Center);
        apply_top_bar_button_style(builder, h.now_lyrics);
        apply_clean_button_shell_style(builder, h.now_lyrics, 10, 20);
        {
            StylePatch patch{};
            patch.has_font = true;
            patch.font = &get_player_font_px(16, FontWeight::Medium);
            builder.set_style_patch(h.now_lyrics, patch);
        }

        builder.link(now_playing, h.now_top_bar);
        builder.link(h.now_top_bar, h.now_back);
        builder.link(h.now_top_bar, top_title);
        builder.link(h.now_top_bar, h.now_more);
        builder.link(h.now_top_bar, h.now_lyrics);
    }

#if defined(CHARM_PLAYER_COVER_DEBUG)
    static void build_now_playing_cover_debug_label(::ui::scene::SceneBuilder& builder,
                                                    WidgetHandle now_playing,
                                                    UiHandles& h,
                                                    const NowPlayingLayout& now_layout,
                                                    const NowPlayingBuildMetrics& metrics) {
        constexpr bool kShowCoverDebugLabel = false;
        if constexpr (!kShowCoverDebugLabel) return;

        h.cover_debug = builder.create_label_static("");
        apply_time_label_style(builder, h.cover_debug);
        builder.set_label_align(h.cover_debug, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, h.cover_debug,
                    {kUiPadding,
                     metrics.progress_section_rect.y + metrics.progress_section_rect.h + now_layout.cover_debug_gap,
                     screen_width - kUiPadding * 2,
                     now_layout.cover_debug_h});
        builder.set_hit_testable(h.cover_debug, false);
        builder.link(now_playing, h.cover_debug);
    }
#endif

    static void build_now_playing(::ui::scene::SceneBuilder& builder, UiHandles& h,
                                  const UiLayout& layout, const NowTextLayout& text_layout,
                                  const PlayerIconIds& icons) {
        (void)text_layout;

        const NowPlayingLayout now_layout = make_now_playing_layout();
        const NowPlayingBuildMetrics metrics = make_now_playing_build_metrics(layout, now_layout);

        const WidgetHandle now_playing = build_now_playing_root(builder, h);
        create_now_playing_hidden_widgets(builder, now_playing, h);
        build_now_playing_top_bar(builder, now_playing, h, now_layout, metrics, icons);
        build_now_playing_cover_area(builder, now_playing, h, now_layout, metrics);
        build_now_playing_text_area(builder, now_playing, h, now_layout, metrics);
        build_now_playing_progress_area(builder, now_playing, h, now_layout, metrics);
        build_now_playing_controls_area(builder, now_playing, h, now_layout, metrics, icons);
        build_now_playing_aux_area(builder, now_playing, h, metrics, icons);

#if defined(CHARM_PLAYER_COVER_DEBUG)
        build_now_playing_cover_debug_label(builder, now_playing, h, now_layout, metrics);
#endif
    }
