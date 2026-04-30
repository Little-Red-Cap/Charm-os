    static void apply_home_backdrop_style(::ui::scene::SceneBuilder& builder, WidgetHandle h) {
        StylePatch patch = ::ui::scene::make_clean_surface_patch({
            .apply_bg_color = true,
            .apply_border_color = true,
            .apply_corner_radius = true,
            .bg_color = kUiBackdropBase,
            .border_color = {0, 0, 0, 0},
            .corner_radius = 0,
        });
        builder.set_style_override(h, patch);
    }

    static WidgetHandle create_home_hero_title_label(::ui::scene::SceneBuilder& builder,
                                                     const char* text,
                                                     const Rect& rect) {
        const WidgetHandle label = builder.create_label_static(text);
        apply_home_hero_title_style(builder, label, kUiTitle);
        builder.set_label_align(label, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, label, rect);
        return label;
    }

    static WidgetHandle create_home_hero_subtitle_label(::ui::scene::SceneBuilder& builder,
                                                        const char* text,
                                                        const Rect& rect) {
        const WidgetHandle label = builder.create_label_static(text);
        apply_player_text_style(builder, label, PlayerTextRole::HeroSubtitle, kUiSubtitle);
        builder.set_label_align(label, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, label, rect);
        return label;
    }

    static WidgetHandle create_home_shuffle_button(::ui::scene::SceneBuilder& builder,
                                                   const Rect& rect,
                                                   ::ui::gfx::ImageId icon,
                                                   int icon_px) {
        const WidgetHandle button = builder.create_button_static("");
        anchor_rect(builder, button, rect);
        builder.set_button_icon(button, icon);
        builder.set_button_icon_size(button, icon_px);
        apply_home_action_button_style(builder, button, rect.h / 2);
        return button;
    }

    static void link_home_collage_cover_slot(::ui::scene::SceneBuilder& builder,
                                             WidgetHandle collage_root,
                                             WidgetHandle& out_image,
                                             const Rect& image_rect,
                                             int image_radius,
                                             int plate_radius,
                                             bool emphasize = false,
                                             ::ui::scene::ImageShapeKind image_shape = ::ui::scene::ImageShapeKind::RoundRect,
                                             std::uint8_t shape_extent = 0,
                                             std::int16_t rotation_deg = 0) {
        const auto slot = ::ui::scene::create_image_slot(
            builder,
            collage_root,
            {
                .image_rect = image_rect,
                .image_corner_radius = image_radius,
                .plate_pad = 3,
                .image_shape = image_shape,
                .shape_extent = shape_extent,
                .rotation_deg = rotation_deg,
                .create_plate = false,
            },
            [&](::ui::scene::SceneBuilder& slot_builder, WidgetHandle plate) {
                apply_home_cover_plate_style(slot_builder, plate, plate_radius, emphasize);
            });
        out_image = slot.image;
    }

    static void build_home_collage(::ui::scene::SceneBuilder& builder,
                                   UiHandles& h,
                                   WidgetHandle parent,
                                   const HomeLayout& home_layout) {
        const WidgetHandle collage_root = builder.create_container();
        anchor_rect(builder, collage_root,
                    {home_layout.collage_x,
                     home_layout.collage_y,
                     home_layout.collage_w,
                     home_layout.collage_h});

        // Follow Pixel Player's default collage rhythm: a dominant rotated capsule in the
        // upper field, then smaller orbit pieces and a denser bottom row.
        const int top_h = home_layout.collage_h * 67 / 100;
        const int bottom_y = top_h - 8;
        const int orbit_size = std::clamp(home_layout.collage_w / 4 + 4, 96, 116);
        const int big_w = std::clamp(home_layout.collage_w / 2, 212, 244);
        const int big_h = std::clamp(home_layout.collage_h * 74 / 100, 286, 326);
        const int right_orbit_size = std::clamp(home_layout.collage_w / 4 + 14, 104, 124);
        const int bottom_left_w = std::clamp(home_layout.collage_w / 3 + 18, 134, 162);
        const int bottom_left_h = std::clamp(home_layout.collage_h / 4 + 18, 120, 144);
        const int bottom_mid_w = std::clamp(home_layout.collage_w / 4 + 34, 126, 150);
        const int bottom_mid_h = std::clamp(home_layout.collage_h / 6 + 16, 76, 94);
        const int bottom_right_w = std::clamp(home_layout.collage_w / 4 + 10, 98, 118);
        const int bottom_right_h = std::clamp(home_layout.collage_h / 3 + 6, 120, 144);
        const auto half_min_extent = [](const Rect& rect) noexcept -> std::uint8_t {
            return clamp_u8(((rect.w < rect.h) ? rect.w : rect.h) / 2);
        };

        const Rect big_cover_rect{
            (home_layout.collage_w - big_w) / 2 - 12,
            std::max(0, (top_h - big_h) / 2 + 16),
            big_w,
            big_h,
        };
        const Rect left_cover_rect{
            8,
            big_cover_rect.y + 12,
            orbit_size,
            orbit_size,
        };
        const Rect right_cover_rect{
            home_layout.collage_w - right_orbit_size - 8,
            std::max(8, big_cover_rect.y + 18),
            right_orbit_size,
            right_orbit_size,
        };
        const Rect center_cover_rect{
            (home_layout.collage_w - bottom_mid_w) / 2 - 14,
            home_layout.collage_h - bottom_mid_h - 18,
            bottom_mid_w,
            bottom_mid_h,
        };
        const Rect bottom_left_cover_rect{
            14,
            bottom_y + 12,
            bottom_left_w,
            bottom_left_h,
        };
        const Rect bottom_right_cover_rect{
            home_layout.collage_w - bottom_right_w - 10,
            home_layout.collage_h - bottom_right_h - 8,
            bottom_right_w,
            bottom_right_h,
        };

        link_home_collage_cover_slot(builder,
                                     collage_root,
                                     h.home_cover_big,
                                     big_cover_rect,
                                     big_w / 2,
                                     big_w / 2 + 4,
                                     true,
                                     ::ui::scene::ImageShapeKind::RoundRect,
                                     half_min_extent(big_cover_rect),
                                     20);
        link_home_collage_cover_slot(builder,
                                     collage_root,
                                     h.home_cover_left,
                                     left_cover_rect,
                                     left_cover_rect.w / 2,
                                     left_cover_rect.w / 2,
                                     false,
                                     ::ui::scene::ImageShapeKind::RoundRect,
                                     half_min_extent(left_cover_rect),
                                     -6);
        link_home_collage_cover_slot(builder,
                                     collage_root,
                                     h.home_cover_right,
                                     right_cover_rect,
                                     right_cover_rect.h / 2,
                                     right_cover_rect.h / 2 + 3,
                                     true,
                                     ::ui::scene::ImageShapeKind::RoundRect,
                                     half_min_extent(right_cover_rect),
                                     8);
        link_home_collage_cover_slot(builder,
                                     collage_root,
                                     h.home_cover_small,
                                     center_cover_rect,
                                     center_cover_rect.h / 2,
                                     center_cover_rect.h / 2 + 3,
                                     false,
                                     ::ui::scene::ImageShapeKind::RoundRect,
                                     half_min_extent(center_cover_rect),
                                     7);
        link_home_collage_cover_slot(builder,
                                     collage_root,
                                     h.home_cover_bottom_left,
                                     bottom_left_cover_rect,
                                     24,
                                     28,
                                     false,
                                     ::ui::scene::ImageShapeKind::RoundRect,
                                     24,
                                     -18);
        link_home_collage_cover_slot(builder,
                                     collage_root,
                                     h.home_cover_bottom_right,
                                     bottom_right_cover_rect,
                                     20,
                                     24,
                                     false,
                                     ::ui::scene::ImageShapeKind::CutCorner,
                                     16,
                                     14);

        builder.link(parent, collage_root);
    }

    static void build_home_cards(::ui::scene::SceneBuilder& builder,
                                 UiHandles& h,
                                 WidgetHandle parent,
                                 const HomeLayout& home_layout,
                                 const PlayerIconIds& icons) {
        create_home_daily_mix_card(builder, h, parent,
                                   {home_layout.stat_card_x,
                                    home_layout.stat_card_y,
                                    home_layout.stat_card_w,
                                    home_layout.stat_card_h},
                                   "Fresh picks from your favorites",
                                   "24 tracks");
        create_home_recently_played_card(builder, h, parent,
                                         {home_layout.stat_card_x,
                                          home_layout.stat_card_y + home_layout.stat_card_h + home_layout.stat_card_gap,
                                          home_layout.stat_card_w,
                                          home_layout.stat_card_h},
                                         icons,
                                         "Resume albums and mixes you touched",
                                         "2 hours");
        create_home_stats_probe_card(builder, h, parent,
                                     {home_layout.stat_card_x,
                                      home_layout.stat_card_y
                                          + (home_layout.stat_card_h + home_layout.stat_card_gap) * 2,
                                      home_layout.stat_card_w,
                                      home_layout.stat_probe_card_h},
                                     icons);
    }

    static WidgetHandle create_home_toolbar_icon_button(::ui::scene::SceneBuilder& builder,
                                                        const Rect& rect,
                                                        ::ui::gfx::ImageId icon,
                                                        int icon_px,
                                                        int button_size) {
        const WidgetHandle button = create_top_icon_button(builder, rect, icon, icon_px);
        apply_home_toolbar_button_style(builder, button, button_size / 2);
        return button;
    }

    static void build_home_toolbar(::ui::scene::SceneBuilder& builder,
                                   WidgetHandle parent,
                                   const UiLayout& layout,
                                   const HomeLayout& home_layout,
                                   const PlayerIconIds& icons) {
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

        const WidgetHandle home_settings = create_home_toolbar_icon_button(
            builder,
            home_top_layout.right_primary_rect,
            icons.settings,
            home_layout.toolbar_icon_px + 2,
            home_layout.toolbar_button_size);
        const WidgetHandle home_more = create_home_toolbar_icon_button(
            builder,
            home_top_layout.right_secondary_rect,
            icons.more,
            home_layout.toolbar_icon_px,
            home_layout.toolbar_button_size);

        builder.link(parent, home_toolbar);
        builder.link(home_toolbar, home_settings);
        builder.link(home_toolbar, home_more);
    }

    static void build_home_hero(::ui::scene::SceneBuilder& builder,
                                UiHandles& h,
                                WidgetHandle parent,
                                const HomeLayout& home_layout,
                                const PlayerIconIds& icons) {
        h.home_title_top = create_home_hero_title_label(
            builder,
            "Your",
            {home_layout.title_x, home_layout.title_y, home_layout.title_w, home_layout.title_line_h});
        h.home_title_bottom = create_home_hero_title_label(
            builder,
            "Mix",
            {home_layout.title_x,
             home_layout.title_y + home_layout.title_line_gap,
             home_layout.title_w,
             home_layout.title_line_h});
        h.home_subtitle = create_home_hero_subtitle_label(
            builder,
            "Today's Mix for you",
            {home_layout.title_x, home_layout.subtitle_y, home_layout.title_w, home_layout.subtitle_h});
        h.home_play = create_home_shuffle_button(
            builder,
            {screen_width - kUiPadding - home_layout.play_size,
             home_layout.play_y,
             home_layout.play_size,
             home_layout.play_size},
            icons.shuffle,
            home_layout.play_icon_px);

        builder.link(parent, h.home_title_top);
        builder.link(parent, h.home_title_bottom);
        builder.link(parent, h.home_subtitle);
        builder.link(parent, h.home_play);
    }

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
        apply_home_backdrop_style(builder, h.home_backdrop);

        build_home_hero(builder, h, home_content, home_layout, icons);
        build_home_collage(builder, h, home_content, home_layout);
        build_home_cards(builder, h, home_content, home_layout, icons);
        build_home_toolbar(builder, home_layers.chrome, layout, home_layout, icons);

        builder.link(home_layers.content, home_scroll);
        builder.link(home_scroll, home_content);
    }
