    static void build_now_playing(::ui::scene::SceneBuilder& builder, UiHandles& h,
                                  const UiLayout& layout, const NowTextLayout& text_layout,
                                  const PlayerIconIds& icons) {
        constexpr int kNowTextPadding = 0;
        constexpr int kNowControlsPadding = 0;
        constexpr int kNowTimePadding = 0;
        const int title_h = text_layout.title_h;
        const int subtitle_h = text_layout.subtitle_h;
        const int progress_h = text_layout.progress_h;
        const int time_row_h = text_layout.time_row_h;
        const int time_row_y = text_layout.time_row_y;
        const int text_col_y = text_layout.text_col_y;
        const int text_col_h = text_layout.text_col_h;
        const int text_gap = text_layout.text_gap;
        WidgetHandle text_col_root{};

        h.cover = builder.create_image();
        anchor_rect(builder, h.cover, {layout.cover_x, layout.cover_y, kCoverSize, kCoverSize});
        {
            StylePatch patch{};
            patch.has_corner_radius = true;
            patch.corner_radius = 28;
            builder.set_style_override(h.cover, patch);
        }

        constexpr bool kShowCoverCollage = false;
        if (kShowCoverCollage) {
            const int collage_small = 64;
            const int collage_tiny = 52;
            h.cover_left = builder.create_image();
            anchor_rect(builder, h.cover_left, {layout.cover_x - 18,
                                                layout.cover_y + kCoverSize - collage_small - 8,
                                                collage_small, collage_small});
            {
                StylePatch patch{};
                patch.has_corner_radius = true;
                patch.corner_radius = 12;
                patch.has_shadow_enabled = true;
                patch.shadow_enabled = true;
                patch.has_shadow_color = true;
                patch.shadow_color = {0, 0, 0, 80};
                patch.has_shadow_offset_x = true;
                patch.shadow_offset_x = 0;
                patch.has_shadow_offset_y = true;
                patch.shadow_offset_y = 2;
                patch.has_shadow_spread = true;
                patch.shadow_spread = 2;
                patch.has_shadow_radius = true;
                patch.shadow_radius = 10;
                builder.set_style_override(h.cover_left, patch);
            }
            h.cover_right = builder.create_image();
            anchor_rect(builder, h.cover_right, {layout.cover_x + kCoverSize - collage_tiny + 18,
                                                 layout.cover_y + kCoverSize / 2 + 28,
                                                 collage_tiny, collage_tiny});
            {
                StylePatch patch{};
                patch.has_corner_radius = true;
                patch.corner_radius = 12;
                patch.has_shadow_enabled = true;
                patch.shadow_enabled = true;
                patch.has_shadow_color = true;
                patch.shadow_color = {0, 0, 0, 80};
                patch.has_shadow_offset_x = true;
                patch.shadow_offset_x = 0;
                patch.has_shadow_offset_y = true;
                patch.shadow_offset_y = 2;
                patch.has_shadow_spread = true;
                patch.shadow_spread = 2;
                patch.has_shadow_radius = true;
                patch.shadow_radius = 10;
                builder.set_style_override(h.cover_right, patch);
            }
        }

        h.title = builder.create_label_static("");
        apply_text_label_style(builder, h.title, kUiTitle,
                               get_font_weighted(FontId::Normal, FontWeight::Bold));
        builder.set_label_align(h.title, ::ui::scene::TextAlignH::Center, ::ui::scene::TextAlignV::Center);
        builder.set_hit_testable(h.title, false);

        h.subtitle = builder.create_label_static("");
        apply_text_label_style(builder, h.subtitle, kUiSubtitle,
                               get_font_weighted(FontId::Small, FontWeight::Regular));
        builder.set_label_align(h.subtitle, ::ui::scene::TextAlignH::Center, ::ui::scene::TextAlignV::Center);
        builder.set_hit_testable(h.subtitle, false);

        h.progress = builder.create_progress();
        builder.set_range(h.progress, 0, 100);
        builder.set_value(h.progress, 0);
        builder.set_hit_testable(h.progress, true);

        h.time_left = builder.create_label_static("");
        h.time_right = builder.create_label_static("");
        h.info_tag = builder.create_button_static("");
        apply_time_label_style(builder, h.time_left);
        apply_time_label_style(builder, h.time_right);
        builder.set_label_align(h.time_right, ::ui::scene::TextAlignH::Right, ::ui::scene::TextAlignV::Center);
        apply_info_tag_style(builder, h.info_tag);
        builder.set_hit_testable(h.info_tag, false);

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
            const int row_y = 0;
            h.eq_labels[i] = builder.create_label_static(kEqLabels[i]);
            anchor_rect(builder, h.eq_labels[i], {0, row_y, 0, 0});

            const int slider_x = 0;
            const int slider_w = 0;
            h.eq_sliders[i] = builder.create_slider();
            anchor_rect(builder, h.eq_sliders[i], {slider_x, row_y, slider_w, 0});
            builder.set_range(h.eq_sliders[i], -12, 12);
            builder.set_value(h.eq_sliders[i], 0);

            h.eq_values[i] = builder.create_label_static("");
            anchor_rect(builder, h.eq_values[i], {0, row_y, 0, 0});
        }

        const int vol_y = 0;
        h.volume_label = builder.create_label_static("Vol");
        anchor_rect(builder, h.volume_label, {0, vol_y, 0, 0});
        const int vol_slider_x = 0;
        const int vol_slider_w = 0;
        h.volume_slider = builder.create_slider();
        anchor_rect(builder, h.volume_slider, {vol_slider_x, vol_y, vol_slider_w, 0});
        builder.set_range(h.volume_slider, 0, 100);
        builder.set_value(h.volume_slider, 80);
        h.volume_value = builder.create_label_static("");
        anchor_rect(builder, h.volume_value, {0, vol_y, 0, 0});

        const int dc_y = 0;
        h.dc_label = builder.create_label_static("DC");
        anchor_rect(builder, h.dc_label, {0, dc_y, 0, 0});
        h.dc_switch = builder.create_switch();
        anchor_rect(builder, h.dc_switch, {0, dc_y, 0, 0});
        builder.set_checked(h.dc_switch, true);

        const int clip_y = 0;
        h.clip_label = builder.create_label_static("Clip");
        anchor_rect(builder, h.clip_label, {0, clip_y, 0, 0});
        h.clip_switch = builder.create_switch();
        anchor_rect(builder, h.clip_switch, {0, clip_y, 0, 0});
        builder.set_checked(h.clip_switch, true);
        const int clip_slider_x = 0;
        const int clip_slider_w = 0;
        h.clip_slider = builder.create_slider();
        anchor_rect(builder, h.clip_slider, {clip_slider_x, clip_y, clip_slider_w, 0});
        builder.set_range(h.clip_slider, 60, 100);
        builder.set_value(h.clip_slider, 85);
        h.clip_value = builder.create_label_static("");
        anchor_rect(builder, h.clip_value, {0, clip_y, 0, 0});

        constexpr int control_side = 96;
        constexpr int control_play = 116;
        const int small_btn = control_side;
        const int play_btn = control_play;
        const int btn_gap = 14;
        const Rect controls_rect{layout.controls_x, layout.controls_y, layout.controls_w, control_play};
        ::ui::scene::RowBuilder controls_row{builder, controls_rect, btn_gap, kNowControlsPadding,
                                             ::ui::scene::LayoutAlign::Center};
        h.controls = controls_row.root();

        h.btn_prev = builder.create_button_static("");
        controls_row.add(h.btn_prev, small_btn, small_btn);
        builder.set_button_icon(h.btn_prev, icons.prev);
        builder.set_button_icon_size(h.btn_prev, 24);
        apply_control_side_style(builder, h.btn_prev, small_btn, 24);

        h.btn_pause = builder.create_button_static("");
        controls_row.add(h.btn_pause, play_btn, play_btn);
        builder.set_button_icon(h.btn_pause, icons.play);
        builder.set_button_icon_size(h.btn_pause, 30);
        apply_control_play_style(builder, h.btn_pause, play_btn, 30);

        h.btn_next = builder.create_button_static("");
        controls_row.add(h.btn_next, small_btn, small_btn);
        builder.set_button_icon(h.btn_next, icons.next);
        builder.set_button_icon_size(h.btn_next, 24);
        apply_control_side_style(builder, h.btn_next, small_btn, 24);

        constexpr bool kShowModeButton = false;
        if constexpr (kShowModeButton) {
            h.btn_mode = builder.create_button_static("");
            anchor_rect(builder, h.btn_mode, {0, 0, 0, 0});
            builder.link(h.controls, h.btn_mode);
        }

        {
            const Rect text_rect{kUiPadding, text_col_y,
                                 screen_width - kUiPadding * 2, text_col_h};
            ::ui::scene::ColumnBuilder text_col{builder, text_rect, text_gap, kNowTextPadding,
                                                ::ui::scene::LayoutAlign::Start};
            text_col_root = text_col.root();
            text_col.add(h.title, text_rect.w, title_h);
            text_col.add(h.subtitle, text_rect.w, subtitle_h);
            text_col.add(h.progress, text_rect.w, progress_h);
        }
        WidgetHandle time_row_root{};
        {
            const int time_row_w = screen_width - kUiPadding * 2;
            const int time_row_x = kUiPadding;
            const int time_side_w = 52;
            const int info_w = std::max(170, time_row_w - time_side_w * 2 - 16);
            const int extra = time_row_w - (time_side_w * 2 + info_w);
            const int row_pad = (extra > 0) ? (extra / 2) : 0;
            const Rect time_row_rect{time_row_x, time_row_y, time_row_w, time_row_h};
            ::ui::scene::RowBuilder time_row{builder, time_row_rect, 0, row_pad + kNowTimePadding,
                                             ::ui::scene::LayoutAlign::Center};
            time_row_root = time_row.root();
            time_row.add(h.time_left, time_side_w, time_row_h);
            time_row.add(h.info_tag, info_w, time_row_h);
            time_row.add(h.time_right, time_side_w, time_row_h);
        }

#if defined(CHARM_PLAYER_COVER_DEBUG)
        constexpr bool kShowCoverDebugLabel = false;
        if constexpr (kShowCoverDebugLabel) {
            h.cover_debug = builder.create_label_static("");
            apply_time_label_style(builder, h.cover_debug);
            builder.set_label_align(h.cover_debug, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
            anchor_rect(builder, h.cover_debug, {kUiPadding, time_row_y + time_row_h + 4,
                                                 screen_width - kUiPadding * 2, 12});
            builder.set_hit_testable(h.cover_debug, false);
        }
#endif

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

        const WidgetHandle top_bar = builder.create_container();
        anchor_rect(builder, top_bar, {layout.top_bar_x, layout.top_bar_y, layout.top_bar_w, layout.top_bar_h});
        h.now_back = builder.create_button_static("");
        anchor_rect(builder, h.now_back, {layout.top_bar_x, layout.top_bar_y, 40, 40});
        builder.set_button_icon(h.now_back, icons.down);
        builder.set_button_icon_size(h.now_back, 20);
        apply_top_bar_button_style(builder, h.now_back);
        const WidgetHandle top_title = builder.create_label_static("Now Playing");
        anchor_rect(builder, top_title, {layout.top_bar_x + 48, layout.top_bar_y + 8,
                                         layout.top_bar_w - 96, 24});
        const WidgetHandle top_more = builder.create_button_static("");
        anchor_rect(builder, top_more, {layout.top_bar_x + layout.top_bar_w - 40, layout.top_bar_y, 40, 40});
        builder.set_button_icon(top_more, icons.more);
        builder.set_button_icon_size(top_more, 14);
        apply_top_bar_button_style(builder, top_more);
        h.now_more = top_more;

        const WidgetHandle top_lyrics = builder.create_button_static("Lyrics");
        anchor_rect(builder, top_lyrics, {layout.top_bar_x + layout.top_bar_w - 88, layout.top_bar_y, 40, 40});
        builder.set_label_align(top_lyrics, ::ui::scene::TextAlignH::Center, ::ui::scene::TextAlignV::Center);
        apply_top_bar_button_style(builder, top_lyrics);
        h.now_lyrics = top_lyrics;

        builder.link(h.root, now_playing);
        builder.link(now_playing, h.now_backdrop);
        builder.link(now_playing, top_bar);
        builder.link(top_bar, h.now_back);
        builder.link(top_bar, top_title);
        builder.link(top_bar, top_more);
        builder.link(top_bar, top_lyrics);

        builder.link(now_playing, h.cover);
        if (kShowCoverCollage) {
            builder.link(now_playing, h.cover_left);
            builder.link(now_playing, h.cover_right);
        }
        if (text_col_root) {
            builder.link(now_playing, text_col_root);
        }
        if (time_row_root) {
            builder.link(now_playing, time_row_root);
        }
#if defined(CHARM_PLAYER_COVER_DEBUG)
        builder.link(now_playing, h.cover_debug);
#endif
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
        builder.link(now_playing, h.controls);
    }
