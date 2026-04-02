    static void build_home(::ui::scene::SceneBuilder& builder, UiHandles& h,
                           const UiLayout& layout, const PlayerIconIds& icons) {
        const HomeLayout home_layout = make_home_layout(layout);

        const WidgetHandle home = builder.create_container();
        anchor_rect(builder, home, {0, 0, screen_width, screen_height});
        h.page_home = home;

        const WidgetHandle home_scroll = builder.create_scroll_container();
        anchor_rect(builder, home_scroll,
                    {0, 0, screen_width, layout.bottom_bar_y - home_layout.scroll_bottom_gap});
        builder.set_scroll_step(home_scroll, home_layout.scroll_step);

        const WidgetHandle home_content = builder.create_container();
        anchor_rect(builder, home_content,
                    {0, 0, screen_width, screen_height + home_layout.scroll_extra});

        h.home_backdrop = builder.create_container();
        anchor_rect(builder, h.home_backdrop, {0, 0, screen_width, screen_height});
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
        apply_player_text_style(builder, h.home_title_top, PlayerTextRole::HeroTitle, kUiTitle);
        builder.set_label_align(h.home_title_top, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, h.home_title_top,
                    {home_layout.title_x, home_layout.title_y, home_layout.title_w, home_layout.title_line_h});

        h.home_title_bottom = builder.create_label_static("Mix");
        apply_player_text_style(builder, h.home_title_bottom, PlayerTextRole::HeroTitle, kUiTitle);
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

        h.home_cover_big = builder.create_image();
        anchor_rect(builder, h.home_cover_big,
                    {home_layout.collage_x,
                     home_layout.collage_y,
                     home_layout.collage_big,
                     home_layout.collage_big});
        apply_home_cover_style(builder, h.home_cover_big, home_layout.collage_big, false);

        h.home_cover_left = builder.create_image();
        anchor_rect(builder, h.home_cover_left,
                    {home_layout.collage_x - 26,
                     home_layout.collage_y + home_layout.collage_big - home_layout.collage_small_left + 20,
                     home_layout.collage_small_left,
                     home_layout.collage_small_left});
        apply_home_cover_style(builder, h.home_cover_left, home_layout.collage_small_left, true);

        h.home_cover_right = builder.create_image();
        anchor_rect(builder, h.home_cover_right,
                    {home_layout.collage_x + home_layout.collage_big - home_layout.collage_small_right + 12,
                     home_layout.collage_y + home_layout.collage_big / 2 + 18,
                     home_layout.collage_small_right,
                     home_layout.collage_small_right});
        apply_home_cover_style(builder, h.home_cover_right, home_layout.collage_small_right, true);

        h.home_cover_small = builder.create_image();
        anchor_rect(builder, h.home_cover_small,
                    {home_layout.collage_x + home_layout.collage_big / 2 - home_layout.collage_small_center / 2,
                     home_layout.collage_y + home_layout.collage_big - home_layout.collage_small_center + 8,
                     home_layout.collage_small_center,
                     home_layout.collage_small_center});
        apply_home_cover_style(builder, h.home_cover_small, home_layout.collage_small_center, true);

        h.home_cover_bottom_left = builder.create_image();
        anchor_rect(builder, h.home_cover_bottom_left,
                    {home_layout.collage_x + home_layout.collage_big / 2 - home_layout.collage_small_center - 44,
                     home_layout.collage_y + home_layout.collage_big - home_layout.collage_small_center + 28,
                     home_layout.collage_small_center,
                     home_layout.collage_small_center});
        apply_home_cover_style(builder, h.home_cover_bottom_left, home_layout.collage_small_center, true);

        h.home_cover_bottom_right = builder.create_image();
        anchor_rect(builder, h.home_cover_bottom_right,
                    {home_layout.collage_x + home_layout.collage_big / 2 + 38,
                     home_layout.collage_y + home_layout.collage_big - home_layout.collage_small_center + 28,
                     home_layout.collage_small_center,
                     home_layout.collage_small_center});
        apply_home_cover_style(builder, h.home_cover_bottom_right, home_layout.collage_small_center, true);

        create_home_stat_card(builder, home_content,
                              {home_layout.stat_card_x,
                               home_layout.stat_card_y,
                               home_layout.stat_card_w,
                               home_layout.stat_card_h},
                              "Daily MIX", "Fresh picks from your favorites", "24 tracks");
        create_home_stat_card(builder, home_content,
                              {home_layout.stat_card_x,
                               home_layout.stat_card_y + home_layout.stat_card_h + home_layout.stat_card_gap,
                               home_layout.stat_card_w,
                               home_layout.stat_card_h},
                              "Recently Played", "Resume albums and mixes you touched", "2 hours");
        create_home_stat_card(builder, home_content,
                              {home_layout.stat_card_x,
                               home_layout.stat_card_y + (home_layout.stat_card_h + home_layout.stat_card_gap) * 2,
                               home_layout.stat_card_w,
                               home_layout.stat_card_h},
                              "Listening stats", "Weekly minutes, streak and top genres", "7 days");

        const WidgetHandle home_toolbar = builder.create_container();
        const Rect top_bar_rect{layout.top_bar_x, layout.top_bar_y, layout.top_bar_w, layout.top_bar_h};
        anchor_rect(builder, home_toolbar, top_bar_rect);
        const WidgetHandle home_settings = create_top_icon_button(builder,
            ::ui::scene::page_header_right_rect(top_bar_rect,
                                                {.width = home_layout.toolbar_button_size,
                                                 .height = home_layout.toolbar_button_size,
                                                 .top_gap = 0},
                                                home_layout.toolbar_button_size + 4),
            icons.settings, home_layout.toolbar_icon_px);
        const WidgetHandle home_more = create_top_icon_button(builder,
            ::ui::scene::page_header_right_rect(top_bar_rect,
                                                {.width = home_layout.toolbar_button_size,
                                                 .height = home_layout.toolbar_button_size,
                                                 .top_gap = 0},
                                                home_layout.toolbar_button_size + home_layout.toolbar_button_gap + 4),
            icons.more, home_layout.toolbar_icon_px);

        builder.link(home, h.home_backdrop);
        builder.link(home, home_scroll);
        builder.link(home_scroll, home_content);
        builder.link(home_content, h.home_title_top);
        builder.link(home_content, h.home_title_bottom);
        builder.link(home_content, h.home_subtitle);
        builder.link(home_content, h.home_play);
        builder.link(home_content, h.home_cover_big);
        builder.link(home_content, h.home_cover_left);
        builder.link(home_content, h.home_cover_right);
        builder.link(home_content, h.home_cover_small);
        builder.link(home_content, h.home_cover_bottom_left);
        builder.link(home_content, h.home_cover_bottom_right);
        builder.link(home_content, home_toolbar);
        builder.link(home_toolbar, home_settings);
        builder.link(home_toolbar, home_more);
    }
