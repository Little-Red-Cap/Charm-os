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
                                             const NowPlayingBuildMetrics& metrics) {
        h.now_cover_plate = builder.create_container();
        mark_draw_scope(builder, h.now_cover_plate, PlayerDrawScopeId::NowCover);
        anchor_rect(builder, h.now_cover_plate, metrics.cover_plate_rect);
        {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_border_width = true,
                .apply_corner_radius = true,
                .bg_color = {kUiInfoTagBg.r, kUiInfoTagBg.g, kUiInfoTagBg.b, 0},
                .border_color = {kUiTitle.r, kUiTitle.g, kUiTitle.b, 0},
                .border_width = 0,
                .corner_radius = metrics.cover_radius + 10,
            });
            builder.set_style_override(h.now_cover_plate, patch);
            builder.set_hit_testable(h.now_cover_plate, false);
        }
        builder.link(now_playing, h.now_cover_plate);

        h.cover = builder.create_image();
        mark_draw_scope(builder, h.cover, PlayerDrawScopeId::NowCover);
        anchor_rect(builder, h.cover, metrics.cover_rect);
        ::ui::scene::configure_image_surface(builder, h.cover, metrics.cover_radius);
        builder.link(now_playing, h.cover);
    }

#if CHARM_PLAYER_LYRICS
    static void build_now_playing_lyrics_area(::ui::scene::SceneBuilder& builder,
                                              WidgetHandle now_playing,
                                              UiHandles& h,
                                              const NowPlayingBuildMetrics& metrics) {
        h.now_lyrics_panel = builder.create_container();
        mark_draw_scope(builder, h.now_lyrics_panel, PlayerDrawScopeId::NowCover);
        anchor_rect(builder, h.now_lyrics_panel, metrics.lyrics_panel_rect);
        {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_border_width = true,
                .apply_corner_radius = true,
                .bg_color = {kUiInfoTagBg.r, kUiInfoTagBg.g, kUiInfoTagBg.b, 36},
                .border_color = {kUiTitle.r, kUiTitle.g, kUiTitle.b, 22},
                .border_width = 0,
                .corner_radius = metrics.cover_radius + 10,
            });
            builder.set_style_override(h.now_lyrics_panel, patch);
            builder.set_hit_testable(h.now_lyrics_panel, false);
        }

        auto make_lyrics_label = [&](WidgetHandle& handle,
                                     const Rect& rect,
                                     int font_px,
                                     FontWeight weight,
                                     const rgba& color) {
            handle = builder.create_label_static("");
            anchor_rect(builder, handle, rect);
            StylePatch patch = make_player_text_patch(PlayerTextRole::PageSubtitle, color);
            patch.has_font = true;
            patch.font = &get_player_font_px(font_px, weight);
            builder.set_style_override(handle, patch);
            builder.set_label_align(handle, ::ui::scene::TextAlignH::Center, ::ui::scene::TextAlignV::Center);
            builder.set_hit_testable(handle, false);
            builder.link(h.now_lyrics_panel, handle);
        };

        make_lyrics_label(h.now_lyrics_prev,
                          metrics.lyrics_prev_rect,
                          20,
                          FontWeight::Regular,
                          kUiSubtitle);
        make_lyrics_label(h.now_lyrics_current,
                          metrics.lyrics_current_rect,
                          28,
                          FontWeight::Bold,
                          kUiTitle);
        make_lyrics_label(h.now_lyrics_next,
                          metrics.lyrics_next_rect,
                          20,
                          FontWeight::Regular,
                          kUiSubtitle);
        make_lyrics_label(h.now_lyrics_status,
                          metrics.lyrics_status_rect,
                          14,
                          FontWeight::Medium,
                          kUiTime);

        builder.link(now_playing, h.now_lyrics_panel);
    }
#endif

    static void build_now_playing_text_area(::ui::scene::SceneBuilder& builder,
                                            WidgetHandle now_playing,
                                            UiHandles& h,
                                            const NowPlayingLayout& now_layout,
                                            const NowPlayingBuildMetrics& metrics) {
        StylePatch title_patch = make_player_text_patch(PlayerTextRole::PageTitle, kUiTitle);
        title_patch.has_font = true;
        title_patch.font = &get_player_font_px(34, FontWeight::Bold);

        StylePatch subtitle_patch = make_player_text_patch(PlayerTextRole::PageSubtitle, kUiSubtitle);
        subtitle_patch.has_font = true;
        subtitle_patch.font = &get_player_font_px(22, FontWeight::Medium);

        const auto title_block = ::ui::scene::build_title_block(builder, now_playing, {
            .rect = metrics.text_rect,
            .title_text = "",
            .subtitle_text = "",
            .title_style = title_patch,
            .subtitle_style = subtitle_patch,
            .title_height = metrics.title_h,
            .subtitle_height = metrics.subtitle_h,
            .gap = metrics.text_gap,
            .padding = now_layout.text_padding,
            .title_align_h = ::ui::scene::TextAlignH::Center,
            .title_align_v = ::ui::scene::TextAlignV::Center,
            .subtitle_align_h = ::ui::scene::TextAlignH::Center,
            .subtitle_align_v = ::ui::scene::TextAlignV::Center,
            .cross_align = ::ui::scene::LayoutAlign::Start,
            .title_hit_testable = false,
            .subtitle_hit_testable = false,
        });
        h.now_text_group = title_block.root;
        h.title = title_block.title;
        h.subtitle = title_block.subtitle;
        mark_draw_scope(builder, h.now_text_group, PlayerDrawScopeId::NowText);
    }

    struct NowPlayingProgressHandles {
        WidgetHandle group{};
        WidgetHandle visual{};
        WidgetHandle hit{};
        WidgetHandle time_left{};
        WidgetHandle info_tag{};
        WidgetHandle time_right{};
    };

    static NowPlayingProgressHandles create_now_playing_progress_widgets(::ui::scene::SceneBuilder& builder,
                                                                        UiHandles& h,
                                                                        const NowPlayingLayout& now_layout) {
        NowPlayingProgressHandles out{};

        h.progress_visual = builder.create_slider();
        out.visual = h.progress_visual;
        builder.set_range(out.visual, 0, 100);
        builder.set_value(out.visual, 0);
        builder.set_hit_testable(out.visual, false);
        apply_now_progress_visual_style(builder, out.visual);

        h.progress = builder.create_container();
        out.hit = h.progress;
        builder.set_hit_testable(out.hit, true);

        h.time_left = builder.create_label_static("");
        out.time_left = h.time_left;
        h.time_right = builder.create_label_static("");
        out.time_right = h.time_right;
        apply_now_time_side_style(builder, out.time_left, now_layout, false);
        apply_now_time_side_style(builder, out.time_right, now_layout, true);

        h.info_tag = builder.create_button_static("");
        out.info_tag = h.info_tag;
        apply_now_info_tag_style(builder, out.info_tag, now_layout);
        builder.set_hit_testable(out.info_tag, false);

        h.now_progress_group = builder.create_container();
        out.group = h.now_progress_group;
        mark_draw_scope(builder, out.group, PlayerDrawScopeId::NowProgress);
        return out;
    }

    static WidgetHandle build_now_playing_time_row(::ui::scene::SceneBuilder& builder,
                                                   const NowPlayingProgressHandles& handles,
                                                   const NowPlayingLayout& now_layout,
                                                   const NowPlayingBuildMetrics& metrics) {
        ::ui::scene::RowBuilder time_row{builder,
                                         metrics.time_row_rect,
                                         0,
                                         metrics.time_row_pad + now_layout.time_padding,
                                         ::ui::scene::LayoutAlign::Center};
        const WidgetHandle root = time_row.root();
        apply_now_time_row_style(builder, root, now_layout);
        time_row.add(handles.time_left, metrics.time_side_w, metrics.time_row_h);
        time_row.add(handles.info_tag, metrics.time_info_w, metrics.time_row_h);
        time_row.add(handles.time_right, metrics.time_side_w, metrics.time_row_h);
        return root;
    }

    static void build_now_playing_progress_area(::ui::scene::SceneBuilder& builder,
                                                WidgetHandle now_playing,
                                                UiHandles& h,
                                                const NowPlayingLayout& now_layout,
                                                const NowPlayingBuildMetrics& metrics) {
        const NowPlayingProgressHandles progress =
            create_now_playing_progress_widgets(builder, h, now_layout);
        anchor_rect(builder, progress.group, metrics.progress_section_rect);
        apply_now_progress_section_style(builder, progress.group, now_layout);

        anchor_rect(builder, progress.visual, metrics.progress_visual_rect);
        anchor_rect(builder, progress.hit, metrics.progress_hit_rect);
        builder.link(progress.group, progress.visual);
        builder.link(progress.group, progress.hit);

        const WidgetHandle time_row =
            build_now_playing_time_row(builder, progress, now_layout, metrics);

        builder.link(progress.group, time_row);
        builder.link(now_playing, progress.group);
    }

    static WidgetHandle add_now_control_side_button(::ui::scene::SceneBuilder& builder,
                                                    ::ui::scene::RowBuilder& row,
                                                    int button_size,
                                                    ::ui::gfx::ImageId icon,
                                                    int icon_px) {
        const WidgetHandle button = builder.create_button_static("");
        row.add(button, button_size, button_size);
        builder.set_button_icon(button, icon);
        builder.set_button_icon_size(button, icon_px);
        apply_control_side_shell_style(builder, button, button_size, icon_px, button_size / 2);
        return button;
    }

    static WidgetHandle add_now_control_play_button(::ui::scene::SceneBuilder& builder,
                                                    ::ui::scene::RowBuilder& row,
                                                    int button_size,
                                                    ::ui::gfx::ImageId icon,
                                                    int icon_px) {
        const WidgetHandle button = builder.create_button_static("");
        row.add(button, button_size, button_size);
        builder.set_button_icon(button, icon);
        builder.set_button_icon_size(button, icon_px);
        apply_control_play_shell_style(builder, button, button_size, icon_px, button_size / 2);
        return button;
    }

    static WidgetHandle add_now_aux_button(::ui::scene::SceneBuilder& builder,
                                           ::ui::scene::RowBuilder& row,
                                           int button_size,
                                           ::ui::gfx::ImageId icon,
                                           int icon_px,
                                           bool interactive) {
        const WidgetHandle button = builder.create_button_static("");
        row.add(button, button_size, button_size);
        builder.set_button_icon(button, icon);
        builder.set_button_icon_size(button, icon_px);
        apply_icon_button_shell_style(builder, button, PlayerStyleClass::ControlSide, 12, button_size / 2);
        builder.set_hit_testable(button, interactive);
        return button;
    }

    static WidgetHandle create_now_top_icon_button(::ui::scene::SceneBuilder& builder,
                                                   const Rect& rect,
                                                   ::ui::gfx::ImageId icon,
                                                   int icon_px,
                                                   int padding) {
        const WidgetHandle button = builder.create_button_static("");
        anchor_rect(builder, button, rect);
        builder.set_button_icon(button, icon);
        builder.set_button_icon_size(button, icon_px);
        apply_top_icon_button_shell_style(builder, button, padding, rect.h / 2);
        return button;
    }

    static WidgetHandle create_now_top_text_button(::ui::scene::SceneBuilder& builder,
                                                   const Rect& rect,
                                                   const char* text,
                                                   int padding,
                                                   int font_px) {
        const WidgetHandle button = builder.create_button_static(text);
        anchor_rect(builder, button, rect);
        builder.set_label_align(button, ::ui::scene::TextAlignH::Center, ::ui::scene::TextAlignV::Center);
        apply_top_icon_button_shell_style(builder, button, padding, rect.h / 2);
        {
            StylePatch patch{};
            patch.has_font = true;
            patch.font = &get_player_font_px(font_px, FontWeight::Medium);
            builder.set_style_patch(button, patch);
        }
        return button;
    }

    static void build_now_playing_controls_area(::ui::scene::SceneBuilder& builder,
                                                WidgetHandle now_playing,
                                                UiHandles& h,
                                                const NowPlayingLayout& now_layout,
                                                const NowPlayingBuildMetrics& metrics,
                                                const PlayerIconIds& icons) {
        const int controls_gap = std::max(10, now_layout.control_gap + now_layout.control_gap_delta);
        ::ui::scene::RowBuilder controls_row{builder,
                                             metrics.controls_rect,
                                             controls_gap,
                                             now_layout.controls_padding,
                                             ::ui::scene::LayoutAlign::Center};
        h.controls = controls_row.root();
        mark_draw_scope(builder, h.controls, PlayerDrawScopeId::NowControls);

        h.btn_prev = add_now_control_side_button(
            builder, controls_row, metrics.small_btn, icons.prev, now_layout.control_side_icon + 2);
        h.btn_pause = add_now_control_play_button(
            builder, controls_row, metrics.play_btn, icons.play, now_layout.control_play_icon + 2);
        h.btn_next = add_now_control_side_button(
            builder, controls_row, metrics.small_btn, icons.next, now_layout.control_side_icon + 2);

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
                                           const NowPlayingLayout& now_layout,
                                           const NowPlayingBuildMetrics& metrics,
                                           const PlayerIconIds& icons) {
        h.now_aux_group = builder.create_container();
        mark_draw_scope(builder, h.now_aux_group, PlayerDrawScopeId::NowControls);
        anchor_rect(builder, h.now_aux_group, metrics.aux_panel_rect);
        {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_border_width = true,
                .apply_corner_radius = true,
                .bg_color = {kUiInfoTagBg.r, kUiInfoTagBg.g, kUiInfoTagBg.b, 28},
                .border_color = {kUiInfoTagBorderSoft.r, kUiInfoTagBorderSoft.g, kUiInfoTagBorderSoft.b, 18},
                .border_width = 0,
                .corner_radius = metrics.aux_panel_rect.h / 2,
            });
            builder.set_style_override(h.now_aux_group, patch);
            builder.set_hit_testable(h.now_aux_group, false);
        }

        ::ui::scene::RowBuilder aux_row{builder,
                                        {now_layout.aux_inner_pad_x,
                                         now_layout.aux_inner_pad_top,
                                         metrics.aux_panel_rect.w - now_layout.aux_inner_pad_x * 2,
                                         metrics.aux_button},
                                        metrics.aux_gap,
                                        0,
                                        ::ui::scene::LayoutAlign::Center};
        const WidgetHandle aux_actions_root = aux_row.root();

        const int aux_icon = now_layout.aux_icon_px;
        add_now_aux_button(builder, aux_row, metrics.aux_button, icons.shuffle, aux_icon, false);
        h.btn_mode = add_now_aux_button(builder, aux_row, metrics.aux_button, icons.loop, aux_icon, true);
        add_now_aux_button(builder, aux_row, metrics.aux_button, icons.more, aux_icon, false);

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

        h.now_back = create_now_top_icon_button(
            builder, metrics.now_back_rect, icons.down, now_layout.top_back_icon + 2, 9);

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

        h.now_more = create_now_top_icon_button(
            builder, metrics.now_more_rect, icons.more, now_layout.top_more_icon + 2, 11);
#if CHARM_PLAYER_LYRICS
        h.now_lyrics = create_now_top_text_button(builder, metrics.now_lyrics_rect, "Lyrics", 10, 16);
#endif

        builder.link(now_playing, h.now_top_bar);
        builder.link(h.now_top_bar, h.now_back);
        builder.link(h.now_top_bar, top_title);
        builder.link(h.now_top_bar, h.now_more);
#if CHARM_PLAYER_LYRICS
        builder.link(h.now_top_bar, h.now_lyrics);
#endif
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
        build_now_playing_cover_area(builder, now_playing, h, metrics);
#if CHARM_PLAYER_LYRICS
        build_now_playing_lyrics_area(builder, now_playing, h, metrics);
#endif
        build_now_playing_text_area(builder, now_playing, h, now_layout, metrics);
        build_now_playing_progress_area(builder, now_playing, h, now_layout, metrics);
        build_now_playing_controls_area(builder, now_playing, h, now_layout, metrics, icons);
        build_now_playing_aux_area(builder, now_playing, h, now_layout, metrics, icons);

#if defined(CHARM_PLAYER_COVER_DEBUG)
        build_now_playing_cover_debug_label(builder, now_playing, h, now_layout, metrics);
#endif
    }
