    static void build_home(::ui::scene::SceneBuilder& builder, UiHandles& h,
                           const UiLayout& layout, const PlayerIconIds& icons) {
        const HomeLayout home_layout = make_home_layout(layout);

        const WidgetHandle home = builder.create_container();
        anchor_rect(builder, home, {0, 0, screen_width, screen_height});
        h.page_home = home;

        const auto home_layers = ::ui::scene::create_page_layers(builder, home, {0, 0, screen_width, screen_height});

        const WidgetHandle home_scroll = builder.create_scroll_container();
        anchor_rect(builder, home_scroll,
                    {0, 0, screen_width, layout.bottom_bar_y - home_layout.scroll_bottom_gap});
        builder.set_scroll_step(home_scroll, home_layout.scroll_step);
        h.home_scroll = home_scroll;

        const WidgetHandle home_content = builder.create_container();
        anchor_rect(builder, home_content,
                    {0, 0, screen_width, screen_height + home_layout.scroll_extra});

        h.home_backdrop = home_layers.backdrop;
        {
            StylePatch patch{};
            patch.has_bg_color = true;
            patch.bg_color = kUiBackdropBase;
            patch.has_border_color = true;
            patch.border_color = {0, 0, 0, 0};
            patch.has_corner_radius = true;
            patch.corner_radius = 0;
            builder.set_style_override(h.home_backdrop, patch);
        }

        h.home_title_top = builder.create_label_static("Your");
        apply_home_hero_title_style(builder, h.home_title_top, kUiTitle);
        builder.set_label_align(h.home_title_top, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, h.home_title_top,
                    {home_layout.title_x, home_layout.title_y, home_layout.title_w, home_layout.title_line_h});

        h.home_title_bottom = builder.create_label_static("Mix");
        apply_home_hero_title_style(builder, h.home_title_bottom, kUiTitle);
        builder.set_label_align(h.home_title_bottom, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, h.home_title_bottom,
                    {home_layout.title_x,
                     home_layout.title_y + home_layout.title_line_gap,
                     home_layout.title_w,
                     home_layout.title_line_h});

        h.home_subtitle = builder.create_label_static("Today's Mix for you");
        apply_player_text_style(builder, h.home_subtitle, PlayerTextRole::HeroSubtitle, kUiSubtitle);
        builder.set_label_align(h.home_subtitle, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, h.home_subtitle,
                    {home_layout.title_x, home_layout.subtitle_y, home_layout.title_w, home_layout.subtitle_h});

        h.home_play = builder.create_button_static("");
        const int play_x = screen_width - kUiPadding - home_layout.play_size;
        anchor_rect(builder, h.home_play,
                    {play_x, home_layout.play_y, home_layout.play_size, home_layout.play_size});
        builder.set_button_icon(h.home_play, icons.shuffle);
        builder.set_button_icon_size(h.home_play, home_layout.play_icon_px);
        {
            StylePatch patch{};
            patch.has_bg_color = true;
            patch.bg_color = kUiTabActive;
            patch.has_border_color = true;
            patch.border_color = kUiTabActive;
            patch.has_corner_radius = true;
            patch.corner_radius = home_layout.play_size / 2;
            builder.set_style_override(h.home_play, patch);
        }

        const WidgetHandle collage_root = builder.create_container();
        anchor_rect(builder, collage_root,
                    {home_layout.collage_x,
                     home_layout.collage_y,
                     home_layout.collage_w,
                     home_layout.collage_h});

        const auto add_collage_glow = [&](const Rect& rect,
                                          rgba bg,
                                          rgba shadow,
                                          int radius,
                                          int shadow_offset_y,
                                          int shadow_spread,
                                          int shadow_radius) {
            const WidgetHandle glow = builder.create_container();
            anchor_rect(builder, glow, rect);
            StylePatch patch{};
            patch.has_bg_color = true;
            patch.bg_color = bg;
            patch.has_border_color = true;
            patch.border_color = {0, 0, 0, 0};
            patch.has_corner_radius = true;
            patch.corner_radius = radius;
            patch.has_shadow_enabled = true;
            patch.shadow_enabled = true;
            patch.has_shadow_color = true;
            patch.shadow_color = shadow;
            patch.has_shadow_offset_y = true;
            patch.shadow_offset_y = shadow_offset_y;
            patch.has_shadow_spread = true;
            patch.shadow_spread = shadow_spread;
            patch.has_shadow_radius = true;
            patch.shadow_radius = shadow_radius;
            builder.set_style_override(glow, patch);
            builder.link(collage_root, glow);
        };

        const Rect big_cover_rect{
            (home_layout.collage_w - home_layout.collage_big) / 2,
            22,
            home_layout.collage_big,
            home_layout.collage_big,
        };
        const Rect left_cover_rect{
            18,
            42,
            home_layout.collage_small_left,
            home_layout.collage_small_left,
        };
        const Rect right_cover_rect{
            home_layout.collage_w - (home_layout.collage_small_right + 28) - 18,
            34,
            home_layout.collage_small_right + 28,
            home_layout.collage_small_right - 8,
        };
        const Rect center_cover_rect{
            (home_layout.collage_w - (home_layout.collage_small_center + 62)) / 2,
            big_cover_rect.y + big_cover_rect.h - 18,
            home_layout.collage_small_center + 62,
            home_layout.collage_small_center - 12,
        };
        const Rect bottom_left_cover_rect{
            28,
            home_layout.collage_h - (home_layout.collage_small_center + 18) - 18,
            home_layout.collage_small_center + 18,
            home_layout.collage_small_center + 18,
        };
        const Rect bottom_right_cover_rect{
            home_layout.collage_w - home_layout.collage_small_center - 20,
            home_layout.collage_h - home_layout.collage_small_center - 18,
            home_layout.collage_small_center,
            home_layout.collage_small_center,
        };

        const auto expand_rect = [](const Rect& rect, int pad) noexcept -> Rect {
            return {rect.x - pad, rect.y - pad, rect.w + pad * 2, rect.h + pad * 2};
        };

        add_collage_glow(expand_rect(big_cover_rect, 18),
                         {kUiSwitchOn.r, kUiSwitchOn.g, kUiSwitchOn.b, 18},
                         {kUiSwitchOn.r, kUiSwitchOn.g, kUiSwitchOn.b, 24},
                         (big_cover_rect.w + 36) / 2,
                         14,
                         4,
                         28);
        add_collage_glow(expand_rect(center_cover_rect, 10),
                         {kUiTitle.r, kUiTitle.g, kUiTitle.b, 8},
                         {0, 0, 0, 22},
                         (center_cover_rect.h + 20) / 2,
                         6,
                         0,
                         16);

        const auto add_cover = [&](WidgetHandle& image_handle,
                                   const Rect& image_rect,
                                   int image_radius,
                                   int plate_pad,
                                   int plate_radius,
                                   bool emphasize = false) {
            const WidgetHandle plate = builder.create_container();
            anchor_rect(builder, plate, expand_rect(image_rect, plate_pad));
            apply_home_cover_plate_style(builder, plate, plate_radius, emphasize);
            builder.link(collage_root, plate);

            image_handle = builder.create_image();
            anchor_rect(builder, image_handle, image_rect);
            apply_home_cover_style(builder, image_handle, image_radius);
            builder.link(collage_root, image_handle);
        };

        add_cover(h.home_cover_big, big_cover_rect, 28, 3, 31, true);
        add_cover(h.home_cover_left, left_cover_rect, left_cover_rect.w / 2, 3, (left_cover_rect.w + 6) / 2);
        add_cover(h.home_cover_right, right_cover_rect, right_cover_rect.h / 2, 3, right_cover_rect.h / 2 + 3, true);
        add_cover(h.home_cover_small, center_cover_rect, center_cover_rect.h / 2, 3, center_cover_rect.h / 2 + 3);
        add_cover(h.home_cover_bottom_left, bottom_left_cover_rect, 26, 3, 29);
        add_cover(h.home_cover_bottom_right,
                  bottom_right_cover_rect,
                  bottom_right_cover_rect.w / 2,
                  3,
                  (bottom_right_cover_rect.w + 6) / 2);

        create_home_daily_mix_card(builder, h, home_content,
                                   {home_layout.stat_card_x,
                                    home_layout.stat_card_y,
                                    home_layout.stat_card_w,
                                    home_layout.stat_card_h},
                                   "Fresh picks from your favorites", "24 tracks");
        create_home_recently_played_card(builder, h, home_content,
                                         {home_layout.stat_card_x,
                                          home_layout.stat_card_y + home_layout.stat_card_h + home_layout.stat_card_gap,
                                          home_layout.stat_card_w,
                                          home_layout.stat_card_h},
                                         icons,
                                         "Resume albums and mixes you touched", "2 hours");
        create_home_stats_probe_card(builder, h, home_content,
                                     {home_layout.stat_card_x,
                                      home_layout.stat_card_y + (home_layout.stat_card_h + home_layout.stat_card_gap) * 2,
                                      home_layout.stat_card_w,
                                      home_layout.stat_probe_card_h},
                                     icons);

        const WidgetHandle home_toolbar = builder.create_container();
        const Rect top_bar_rect{layout.top_bar_x, layout.top_bar_y, layout.top_bar_w, layout.top_bar_h};
        const ::ui::scene::TopBarButtonSpec home_primary_spec{
            .width = home_layout.toolbar_button_size,
            .height = home_layout.toolbar_button_size,
            .top_gap = 0,
        };
        const ::ui::scene::TopBarButtonSpec home_secondary_spec{
            .width = home_layout.toolbar_button_size,
            .height = home_layout.toolbar_button_size,
            .top_gap = 0,
        };
        const ::ui::scene::TopBarLayout home_top_layout = ::ui::scene::make_top_bar_layout(
            top_bar_rect,
            nullptr,
            nullptr,
            &home_primary_spec,
            home_layout.toolbar_button_size + 4,
            &home_secondary_spec,
            home_layout.toolbar_button_size + home_layout.toolbar_button_gap + 4);
        anchor_rect(builder, home_toolbar, top_bar_rect);
        const WidgetHandle home_settings = create_top_icon_button(builder,
            home_top_layout.right_primary_rect,
            icons.settings, home_layout.toolbar_icon_px);
        const WidgetHandle home_more = create_top_icon_button(builder,
            home_top_layout.right_secondary_rect,
            icons.more, home_layout.toolbar_icon_px);

        builder.link(home_layers.content, home_scroll);
        builder.link(home_scroll, home_content);
        builder.link(home_content, h.home_title_top);
        builder.link(home_content, h.home_title_bottom);
        builder.link(home_content, h.home_subtitle);
        builder.link(home_content, h.home_play);
        builder.link(home_content, collage_root);
        builder.link(home_layers.chrome, home_toolbar);
        builder.link(home_toolbar, home_settings);
        builder.link(home_toolbar, home_more);
    }
