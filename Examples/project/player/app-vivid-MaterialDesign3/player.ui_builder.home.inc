    static void build_home(::ui::scene::SceneBuilder& builder, UiHandles& h,
                           const UiLayout& layout, const PlayerIconIds& icons) {
        const WidgetHandle home = builder.create_container();
        anchor_rect(builder, home, {0, 0, screen_width, screen_height});
        h.page_home = home;

        const WidgetHandle home_scroll = builder.create_scroll_container();
        anchor_rect(builder, home_scroll, {0, 0, screen_width, layout.bottom_bar_y - 8});
        builder.set_scroll_step(home_scroll, 32);

        const int scroll_extra = 560;
        const WidgetHandle home_content = builder.create_container();
        anchor_rect(builder, home_content, {0, 0, screen_width, screen_height + scroll_extra});

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

        const int title_x = kUiPadding;
        const int title_y = layout.top_bar_y + 8;
        const int title_w = screen_width - kUiPadding * 2 - 96;
        const int title_line_h = 102;
        h.home_title_top = builder.create_label_static("Your");
        apply_text_label_style(builder, h.home_title_top, kUiTitle,
                               get_player_font_px(72, FontWeight::Bold));
        builder.set_label_align(h.home_title_top, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, h.home_title_top, {title_x, title_y, title_w, title_line_h});

        h.home_title_bottom = builder.create_label_static("Mix");
        apply_text_label_style(builder, h.home_title_bottom, kUiTitle,
                               get_player_font_px(72, FontWeight::Bold));
        builder.set_label_align(h.home_title_bottom, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, h.home_title_bottom, {title_x, title_y + 86, title_w, title_line_h});

        h.home_subtitle = builder.create_label_static("Today's Mix for you");
        apply_text_label_style(builder, h.home_subtitle, kUiSubtitle,
                               get_player_font_px(18, FontWeight::Regular));
        builder.set_label_align(h.home_subtitle, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, h.home_subtitle, {title_x, title_y + 194, title_w, 30});

        h.home_play = builder.create_button_static("");
        const int play_size = 78;
        const int play_x = screen_width - kUiPadding - play_size;
        const int play_y = title_y + 78;
        anchor_rect(builder, h.home_play, {play_x, play_y, play_size, play_size});
        builder.set_button_icon(h.home_play, icons.shuffle);
        builder.set_button_icon_size(h.home_play, 28);
        {
            StylePatch patch{};
            patch.has_bg_color = true;
            patch.bg_color = kUiTabActive;
            patch.has_border_color = true;
            patch.border_color = kUiTabActive;
            patch.has_corner_radius = true;
            patch.corner_radius = play_size / 2;
            builder.set_style_override(h.home_play, patch);
        }

        const int collage_big = 286;
        const int collage_small = 88;
        const int collage_small2 = 76;
        const int collage_small3 = 70;
        const int collage_y = title_y + 230;
        const int collage_x = (screen_width - collage_big) / 2;
        h.home_cover_big = builder.create_image();
        anchor_rect(builder, h.home_cover_big, {collage_x, collage_y, collage_big, collage_big});
        {
            StylePatch patch{};
            patch.has_corner_radius = true;
            patch.corner_radius = collage_big / 2;
            builder.set_style_override(h.home_cover_big, patch);
        }

        h.home_cover_left = builder.create_image();
        anchor_rect(builder, h.home_cover_left, {collage_x - 26,
                                                 collage_y + collage_big - collage_small + 20,
                                                 collage_small, collage_small});
        {
            StylePatch patch{};
            patch.has_corner_radius = true;
            patch.corner_radius = collage_small / 2;
            patch.has_shadow_enabled = true;
            patch.shadow_enabled = true;
            patch.has_shadow_color = true;
            patch.shadow_color = kUiCardShadow;
            patch.has_shadow_offset_y = true;
            patch.shadow_offset_y = 6;
            patch.has_shadow_spread = true;
            patch.shadow_spread = 2;
            patch.has_shadow_radius = true;
            patch.shadow_radius = 16;
            builder.set_style_override(h.home_cover_left, patch);
        }

        h.home_cover_right = builder.create_image();
        anchor_rect(builder, h.home_cover_right, {collage_x + collage_big - collage_small2 + 12,
                                                  collage_y + collage_big / 2 + 18,
                                                  collage_small2, collage_small2});
        {
            StylePatch patch{};
            patch.has_corner_radius = true;
            patch.corner_radius = collage_small2 / 2;
            patch.has_shadow_enabled = true;
            patch.shadow_enabled = true;
            patch.has_shadow_color = true;
            patch.shadow_color = kUiCardShadow;
            patch.has_shadow_offset_y = true;
            patch.shadow_offset_y = 6;
            patch.has_shadow_spread = true;
            patch.shadow_spread = 2;
            patch.has_shadow_radius = true;
            patch.shadow_radius = 16;
            builder.set_style_override(h.home_cover_right, patch);
        }

        h.home_cover_small = builder.create_image();
        anchor_rect(builder, h.home_cover_small, {collage_x + collage_big / 2 - collage_small3 / 2,
                                                  collage_y + collage_big - collage_small3 + 8,
                                                  collage_small3, collage_small3});
        {
            StylePatch patch{};
            patch.has_corner_radius = true;
            patch.corner_radius = collage_small3 / 2;
            patch.has_shadow_enabled = true;
            patch.shadow_enabled = true;
            patch.has_shadow_color = true;
            patch.shadow_color = kUiCardShadow;
            patch.has_shadow_offset_y = true;
            patch.shadow_offset_y = 6;
            patch.has_shadow_spread = true;
            patch.shadow_spread = 2;
            patch.has_shadow_radius = true;
            patch.shadow_radius = 16;
            builder.set_style_override(h.home_cover_small, patch);
        }

        h.home_cover_bottom_left = builder.create_image();
        anchor_rect(builder, h.home_cover_bottom_left, {collage_x + collage_big / 2 - collage_small3 - 44,
                                                        collage_y + collage_big - collage_small3 + 28,
                                                        collage_small3, collage_small3});
        {
            StylePatch patch{};
            patch.has_corner_radius = true;
            patch.corner_radius = collage_small3 / 2;
            patch.has_shadow_enabled = true;
            patch.shadow_enabled = true;
            patch.has_shadow_color = true;
            patch.shadow_color = kUiCardShadow;
            patch.has_shadow_offset_y = true;
            patch.shadow_offset_y = 6;
            patch.has_shadow_spread = true;
            patch.shadow_spread = 2;
            patch.has_shadow_radius = true;
            patch.shadow_radius = 16;
            builder.set_style_override(h.home_cover_bottom_left, patch);
        }

        h.home_cover_bottom_right = builder.create_image();
        anchor_rect(builder, h.home_cover_bottom_right, {collage_x + collage_big / 2 + 38,
                                                         collage_y + collage_big - collage_small3 + 28,
                                                         collage_small3, collage_small3});
        {
            StylePatch patch{};
            patch.has_corner_radius = true;
            patch.corner_radius = collage_small3 / 2;
            patch.has_shadow_enabled = true;
            patch.shadow_enabled = true;
            patch.has_shadow_color = true;
            patch.shadow_color = kUiCardShadow;
            patch.has_shadow_offset_y = true;
            patch.shadow_offset_y = 6;
            patch.has_shadow_spread = true;
            patch.shadow_spread = 2;
            patch.has_shadow_radius = true;
            patch.shadow_radius = 16;
            builder.set_style_override(h.home_cover_bottom_right, patch);
        }

        const int stat_card_h = 76;
        const int stat_card_gap = 14;
        const int stat_card_w = screen_width - kUiPadding * 2;
        const int stat_card_x = kUiPadding;
        const int stat_card_y = collage_y + collage_big + 118;
        create_home_stat_card(builder, home_content,
                              {stat_card_x, stat_card_y, stat_card_w, stat_card_h},
                              "Daily MIX", "Fresh picks from your favorites", "24 tracks");
        create_home_stat_card(builder, home_content,
                              {stat_card_x, stat_card_y + stat_card_h + stat_card_gap, stat_card_w, stat_card_h},
                              "Recently Played", "Resume albums and mixes you touched", "2 hours");
        create_home_stat_card(builder, home_content,
                              {stat_card_x, stat_card_y + (stat_card_h + stat_card_gap) * 2, stat_card_w, stat_card_h},
                              "Listening stats", "Weekly minutes, streak and top genres", "7 days");

        const WidgetHandle home_toolbar = builder.create_container();
        anchor_rect(builder, home_toolbar, {layout.top_bar_x, layout.top_bar_y,
                                            layout.top_bar_w, layout.top_bar_h});
        const WidgetHandle home_settings = create_top_icon_button(builder,
            {layout.top_bar_x + layout.top_bar_w - 44, layout.top_bar_y, 40, 40},
            icons.settings, 14);
        const WidgetHandle home_more = create_top_icon_button(builder,
            {layout.top_bar_x + layout.top_bar_w - 92, layout.top_bar_y, 40, 40},
            icons.more, 14);

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

