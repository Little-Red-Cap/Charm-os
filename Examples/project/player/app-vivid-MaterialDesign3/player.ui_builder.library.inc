    static void build_library(::ui::scene::SceneBuilder& builder, UiHandles& h,
                              const UiLayout& layout, const PlayerIconIds& icons) {
        constexpr int kTabsPadding = 0;
        constexpr int kShufflePadding = 0;
        constexpr int kNavPadding = 0;
        h.list_title = builder.create_label_static("");
        anchor_rect(builder, h.list_title, {0, 0, 0, 0});
        h.list_path = builder.create_label_static("");
        anchor_rect(builder, h.list_path, {0, 0, 0, 0});
        h.list_sort = builder.create_button_static("Sort");
        anchor_rect(builder, h.list_sort, {0, 0, 0, 0});

        h.list = builder.create_list_view();
        anchor_rect(builder, h.list, {0, 0, 0, 0});
        builder.set_list_row_height(h.list, 60);
        builder.set_scroll_step(h.list, 60);

        h.list_scroll = builder.create_scrollbar_for(h.list);
        anchor_rect(builder, h.list_scroll, {0, 0, 0, 0});
        builder.set_scrollbar_orientation(h.list_scroll, ScrollBarOrientation::Vertical);

        h.list_hint = builder.create_label_static("");
        anchor_rect(builder, h.list_hint, {0, 0, 0, 0});

        h.nav_bar = {};
        h.nav_home = {};
        h.nav_search = {};
        h.nav_library = {};

        const WidgetHandle library = builder.create_container();
        anchor_rect(builder, library, {0, 0, screen_width, screen_height});
        h.page_library = library;

        const WidgetHandle lib_top = builder.create_container();
        anchor_rect(builder, lib_top, {layout.top_bar_x, layout.top_bar_y, layout.top_bar_w, layout.top_bar_h});
        const WidgetHandle lib_title = builder.create_label_static("Library");
        anchor_rect(builder, lib_title, {layout.top_bar_x + 8, layout.top_bar_y + 8,
                                         layout.top_bar_w - 56, 24});
        const WidgetHandle lib_settings = create_top_icon_button(builder,
            {layout.top_bar_x + layout.top_bar_w - 44, layout.top_bar_y, 40, 40},
            icons.settings, 14);

        const WidgetHandle tab_songs = builder.create_button_static("Songs");
        const WidgetHandle tab_albums = builder.create_button_static("Albums");
        const WidgetHandle tab_artist = builder.create_button_static("Artist");

        const WidgetHandle shuffle_btn = builder.create_button_static("Shuffle");
        const int tab_radius = layout.tabs_h / 2;
        apply_tab_base_style(builder, tab_songs, tab_radius);
        apply_tab_base_style(builder, tab_albums, tab_radius);
        apply_tab_base_style(builder, tab_artist, tab_radius);
        apply_tab_base_style(builder, shuffle_btn, tab_radius);
        apply_tab_active_style(builder, tab_songs);
        apply_shuffle_shadow_style(builder, shuffle_btn);

        ::ui::scene::RowBuilder tabs_row{builder,
                                         {kUiPadding, layout.tabs_y,
                                          screen_width - kUiPadding * 2, layout.tabs_h},
                                         layout.tabs_gap, kTabsPadding,
                                         ::ui::scene::LayoutAlign::Center};
        const WidgetHandle tabs_root = tabs_row.root();
        tabs_row.add(tab_songs, layout.tab_w, layout.tabs_h);
        tabs_row.add(tab_albums, layout.tab_w, layout.tabs_h);
        tabs_row.add(tab_artist, layout.tab_w, layout.tabs_h);

        ::ui::scene::RowBuilder shuffle_row{builder,
                                            {kUiPadding, layout.shuffle_y, 120, layout.shuffle_h},
                                            0, kShufflePadding,
                                            ::ui::scene::LayoutAlign::Center};
        const WidgetHandle shuffle_root = shuffle_row.root();
        shuffle_row.add(shuffle_btn, 120, layout.shuffle_h);

        const Rect list_card_rect{kUiPadding, layout.list_card_y,
                                  screen_width - kUiPadding * 2, layout.list_card_h};
        ::ui::scene::CardBuilder list_card{builder, list_card_rect, 0, 0,
                                           ::ui::scene::LayoutAlign::Start,
                                           static_cast<StyleClassId>(PlayerStyleClass::ListCard),
                                           true};
        const WidgetHandle list_card_root = list_card.root();

        builder.link(h.root, library);
        builder.link(library, lib_top);
        builder.link(lib_top, lib_title);
        builder.link(lib_top, lib_settings);
        builder.link(library, tabs_root);
        builder.link(library, shuffle_root);
        builder.link(library, list_card_root);
        const int list_header_h = 76;
        const int list_title_h = 18;
        const int list_path_h = 16;
        const int list_title_y = list_card_rect.y + 10;
        const int list_path_y = list_title_y + list_title_h + 6;
        anchor_rect(builder, h.list_title, {list_card_rect.x + 12, list_title_y,
                                            list_card_rect.w - 84, list_title_h});
        anchor_rect(builder, h.list_sort, {list_card_rect.x + list_card_rect.w - 66, list_title_y - 6,
                                           54, 28});
        const Rect list_path_rect{list_card_rect.x + 12, list_path_y,
                                  list_card_rect.w - 24, list_path_h + 8};
        const WidgetHandle list_path_bg = builder.create_container();
        anchor_rect(builder, list_path_bg, list_path_rect);
        {
            StylePatch patch{};
            patch.has_bg_color = true;
            patch.bg_color = kUiButtonBg;
            patch.has_border_color = true;
            patch.border_color = kUiButtonBorder;
            patch.has_corner_radius = true;
            patch.corner_radius = 10;
            builder.set_style_override(list_path_bg, patch);
        }
        const WidgetHandle list_path_icon = builder.create_button_static("");
        anchor_rect(builder, list_path_icon, {list_path_rect.x + 6, list_path_rect.y + 2,
                                              list_path_h + 8, list_path_h + 8});
        builder.set_button_icon(list_path_icon, icons.folder);
        builder.set_button_icon_size(list_path_icon, 12);
        apply_tab_base_style(builder, list_path_icon, 10);
        anchor_rect(builder, h.list_path, {list_path_rect.x + 28, list_path_rect.y + 4,
                                           list_path_rect.w - 36, list_path_h});
        apply_tab_base_style(builder, h.list_sort, 12);
        builder.set_label_align(h.list_sort, ::ui::scene::TextAlignH::Center, ::ui::scene::TextAlignV::Center);
        apply_time_label_style(builder, h.list_path);
        builder.set_label_align(h.list_path, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        const int list_y = list_card_rect.y + list_header_h;
        const int list_h = list_card_rect.h - list_header_h - 18;
        anchor_rect(builder, h.list, {list_card_rect.x + 8,
                                      list_y,
                                      list_card_rect.w - 16,
                                      list_h});
        anchor_rect(builder, h.list_scroll, {list_card_rect.x + list_card_rect.w - 10,
                                             list_y, 8,
                                             list_h});
        anchor_rect(builder, h.list_hint, {list_card_rect.x + 12,
                                           list_card_rect.y + list_card_rect.h - 22,
                                           list_card_rect.w - 24, 18});

        builder.link(list_card_root, h.list);
        builder.link(list_card_root, h.list_scroll);
        builder.link(list_card_root, h.list_title);
        builder.link(list_card_root, h.list_sort);
        builder.link(list_card_root, list_path_bg);
        builder.link(list_card_root, h.list_path);
        builder.link(list_card_root, h.list_hint);
        builder.link(list_card_root, list_path_icon);

        build_bottom_chrome(builder, h, layout, icons);
    }
