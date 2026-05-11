    static void build_library(::ui::scene::SceneBuilder& builder, UiHandles& h,
                              const UiLayout& layout, const PlayerIconIds& icons) {
        const LibraryLayout lib_layout = make_library_layout();
        h.list_title = builder.create_label_static("");
        anchor_rect(builder, h.list_title, {0, 0, 0, 0});
        h.list_path = builder.create_label_static("");
        anchor_rect(builder, h.list_path, {0, 0, 0, 0});
        h.list_sort = builder.create_button_static("A-Z");
        anchor_rect(builder, h.list_sort, {0, 0, 0, 0});

        h.list = builder.create_list_view();
        anchor_rect(builder, h.list, {0, 0, 0, 0});
        builder.set_list_row_height(h.list, lib_layout.list_row_h);
        builder.set_scroll_step(h.list, lib_layout.list_row_h);

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
        const auto library_layers = ::ui::scene::create_page_layers(builder, library, {0, 0, screen_width, screen_height});

        const WidgetHandle library_bg_top = builder.create_container();
        anchor_rect(builder, library_bg_top, {0, 0, screen_width, screen_height / 2});
        {
            StylePatch patch{};
            patch.has_gradient_enabled = true;
            patch.gradient_enabled = true;
            patch.has_gradient_start = true;
            patch.gradient_start = kUiLibraryHeroTopStart;
            patch.has_gradient_end = true;
            patch.gradient_end = kUiLibraryHeroTopEnd;
            patch.has_gradient_direction = true;
            patch.gradient_direction = 0;
            patch.has_border_color = true;
            patch.border_color = {0, 0, 0, 0};
            builder.set_style_patch(library_bg_top, patch);
            builder.set_hit_testable(library_bg_top, false);
        }

        const WidgetHandle library_bg_bottom = builder.create_container();
        anchor_rect(builder, library_bg_bottom, {0, screen_height / 3, screen_width, screen_height - screen_height / 3});
        {
            StylePatch patch{};
            patch.has_gradient_enabled = true;
            patch.gradient_enabled = true;
            patch.has_gradient_start = true;
            patch.gradient_start = kUiLibraryHeroBottomStart;
            patch.has_gradient_end = true;
            patch.gradient_end = kUiLibraryHeroBottomEnd;
            patch.has_gradient_direction = true;
            patch.gradient_direction = 0;
            patch.has_border_color = true;
            patch.border_color = {0, 0, 0, 0};
            builder.set_style_patch(library_bg_bottom, patch);
            builder.set_hit_testable(library_bg_bottom, false);
        }

        const int library_top_y = layout.top_bar_y + 4;
        const int library_top_h = 144;
        const int library_tabs_y = library_top_y + library_top_h + 6;
        const int library_tabs_h = 54;
        const int library_tabs_gap = 14;
        const int library_side_pad = 14;
        const int library_tab_w = (screen_width - library_side_pad * 2 - library_tabs_gap * 2) / 3;
        const int library_shuffle_h = 42;
        const int library_shuffle_y = library_tabs_y + library_tabs_h + 16;
        const int library_list_card_y = library_shuffle_y + library_shuffle_h + 12;
        const int library_list_card_h = std::max(240, layout.bottom_bar_y - library_list_card_y - 20);
        const int library_surface_y = library_tabs_y + library_tabs_h + 10;
        const int library_surface_h = layout.bottom_bar_y - library_surface_y - 8;
        const Rect library_controls_rect{
            0,
            library_surface_y,
            screen_width,
            library_surface_h
        };
        const WidgetHandle library_controls = builder.create_container();
        anchor_rect(builder, library_controls, library_controls_rect);
        {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_corner_radius = true,
                .bg_color = kUiLibrarySurfaceBg,
                .border_color = {0, 0, 0, 0},
                .corner_radius = 34,
            });
            patch.has_gradient_enabled = true;
            patch.gradient_enabled = true;
            patch.has_gradient_start = true;
            patch.gradient_start = kUiLibraryControlsTop;
            patch.has_gradient_end = true;
            patch.gradient_end = kUiLibraryControlsBottom;
            patch.has_gradient_direction = true;
            patch.gradient_direction = 0;
            builder.set_style_override(library_controls, patch);
            builder.set_hit_testable(library_controls, false);
        }

        const WidgetHandle lib_top = builder.create_container();
        const Rect top_bar_rect{layout.top_bar_x, library_top_y, layout.top_bar_w, library_top_h};
        const Rect lib_settings_rect{
            top_bar_rect.w - 52,
            6,
            52,
            52
        };
        const Rect lib_title_rect{
            2,
            4,
            lib_settings_rect.x - 14,
            92
        };
        anchor_rect(builder, lib_top, top_bar_rect);
        h.library_hero_title = builder.create_label_static("Library");
        anchor_rect(builder, h.library_hero_title, lib_title_rect);
        apply_library_hero_title_style(builder, h.library_hero_title, kUiOk);
        builder.set_label_align(h.library_hero_title, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        builder.set_hit_testable(h.library_hero_title, false);
        const WidgetHandle lib_subtitle = builder.create_label_static("Browse songs, albums, and artists");
        anchor_rect(builder, lib_subtitle, {4, 86, lib_settings_rect.x - 20, 18});
        apply_player_text_style(builder, lib_subtitle, PlayerTextRole::MetaText, kUiSubtitle);
        {
            StylePatch patch{};
            patch.has_font = true;
            patch.font = &get_player_font_px(15, FontWeight::Medium);
            builder.set_style_patch(lib_subtitle, patch);
        }
        builder.set_label_align(lib_subtitle, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        builder.set_hit_testable(lib_subtitle, false);
        const WidgetHandle lib_settings = create_top_icon_button(builder,
            lib_settings_rect,
            icons.settings, 20);
        {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_corner_radius = true,
                .bg_color = {74, 96, 152, 214},
                .border_color = {0, 0, 0, 0},
                .corner_radius = 26,
            });
            patch.has_font_color = true;
            patch.font_color = {246, 248, 255, 255};
            builder.set_style_override(lib_settings, patch);
        }

        h.list_tab_songs = builder.create_button_static("SONGS");
        h.list_tab_albums = builder.create_button_static("ALBUMS");
        h.list_tab_artist = builder.create_button_static("ARTISTS");

        h.list_shuffle = builder.create_button_static("Shuffle");
        const int tab_radius = library_tabs_h / 2;
        apply_tab_base_style(builder, h.list_tab_songs, tab_radius);
        apply_tab_base_style(builder, h.list_tab_albums, tab_radius);
        apply_tab_base_style(builder, h.list_tab_artist, tab_radius);
        apply_tab_base_style(builder, h.list_shuffle, tab_radius);
        apply_tab_active_style(builder, h.list_tab_songs);
        builder.set_button_icon(h.list_shuffle, icons.shuffle);
        builder.set_button_icon_size(h.list_shuffle, lib_layout.shuffle_icon_px);

        const WidgetHandle tabs_plate = builder.create_container();
        anchor_rect(builder, tabs_plate,
                    {library_side_pad - 2, library_tabs_y - 4,
                     screen_width - (library_side_pad - 2) * 2, library_tabs_h + 8});
        {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_corner_radius = true,
                .bg_color = {kUiLibraryHeaderPlate.r, kUiLibraryHeaderPlate.g, kUiLibraryHeaderPlate.b, 152},
                .border_color = {0, 0, 0, 0},
                .corner_radius = 31,
            });
            patch.has_gradient_enabled = true;
            patch.gradient_enabled = true;
            patch.has_gradient_start = true;
            patch.gradient_start = {42, 51, 72, 166};
            patch.has_gradient_end = true;
            patch.gradient_end = {24, 30, 42, 146};
            patch.has_gradient_direction = true;
            patch.gradient_direction = 0;
            builder.set_style_override(tabs_plate, patch);
            builder.set_hit_testable(tabs_plate, false);
        }
        ::ui::scene::RowBuilder tabs_row{builder,
                                         {library_side_pad, library_tabs_y,
                                          screen_width - library_side_pad * 2, library_tabs_h},
                                         library_tabs_gap, lib_layout.tabs_padding,
                                         ::ui::scene::LayoutAlign::Center};
        const WidgetHandle tabs_root = tabs_row.root();
        tabs_row.add(h.list_tab_songs, library_tab_w, library_tabs_h);
        tabs_row.add(h.list_tab_albums, library_tab_w, library_tabs_h);
        tabs_row.add(h.list_tab_artist, library_tab_w, library_tabs_h);
        h.list_tab_indicator = builder.create_container();
        anchor_rect(builder, h.list_tab_indicator,
                    {library_side_pad + (library_tab_w - 56) / 2,
                     library_tabs_y + library_tabs_h + 5,
                     52,
                     4});
        {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_corner_radius = true,
                .bg_color = {kUiLibraryTabActive.r, kUiLibraryTabActive.g, kUiLibraryTabActive.b, 164},
                .border_color = {0, 0, 0, 0},
                .corner_radius = 3,
            });
            builder.set_style_override(h.list_tab_indicator, patch);
            builder.set_hit_testable(h.list_tab_indicator, false);
        }

        ::ui::scene::RowBuilder shuffle_row{builder,
                                            {library_side_pad, library_shuffle_y, screen_width - library_side_pad * 2, library_shuffle_h},
                                            0, lib_layout.shuffle_padding,
                                            ::ui::scene::LayoutAlign::Start};
        const WidgetHandle shuffle_root = shuffle_row.root();
        shuffle_row.add(h.list_shuffle, lib_layout.shuffle_w, library_shuffle_h);
        const WidgetHandle action_row_plate = builder.create_container();
        anchor_rect(builder, action_row_plate,
                    {library_side_pad, library_shuffle_y - 4,
                     screen_width - library_side_pad * 2, library_shuffle_h + 8});
        {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_corner_radius = true,
                .bg_color = {kUiLibraryHeaderPlate.r, kUiLibraryHeaderPlate.g, kUiLibraryHeaderPlate.b, 170},
                .border_color = {0, 0, 0, 0},
                .corner_radius = 26,
            });
            patch.has_gradient_enabled = true;
            patch.gradient_enabled = true;
            patch.has_gradient_start = true;
            patch.gradient_start = {44, 52, 72, 176};
            patch.has_gradient_end = true;
            patch.gradient_end = {28, 33, 46, 170};
            patch.has_gradient_direction = true;
            patch.gradient_direction = 0;
            builder.set_style_override(action_row_plate, patch);
            builder.set_hit_testable(action_row_plate, false);
        }
        const Rect sort_action_rect{
            screen_width - library_side_pad - lib_layout.list_sort_w,
            library_shuffle_y,
            lib_layout.list_sort_w,
            library_shuffle_h
        };
        const Rect list_card_rect{library_side_pad, library_list_card_y,
                                  screen_width - library_side_pad * 2, library_list_card_h};
        const Rect list_card_local_rect{0, 0, list_card_rect.w, list_card_rect.h};
        const int list_body_top =
            lib_layout.list_title_top_gap
            + lib_layout.list_title_h
            + lib_layout.list_path_top_gap
            + lib_layout.list_path_h
            + lib_layout.list_path_bg_h_extra
            + lib_layout.list_body_top_gap;
        const ::ui::scene::ListCardHeaderLayout list_header_layout =
            ::ui::scene::make_list_card_header_layout(
                list_card_local_rect,
                {
                    .title_left_gap = lib_layout.list_title_left_gap,
                    .title_top_gap = lib_layout.list_title_top_gap,
                    .title_w_reduce = lib_layout.list_title_right_gap,
                    .title_h = lib_layout.list_title_h,
                    .action_w = lib_layout.list_sort_w,
                    .action_h = lib_layout.list_sort_h,
                    .action_right_gap = lib_layout.list_sort_right_gap,
                    .action_top_offset = lib_layout.list_sort_top_offset,
                    .path_top_gap = lib_layout.list_path_top_gap,
                    .path_side_gap = lib_layout.list_path_side_gap,
                    .path_h = lib_layout.list_path_h,
                    .path_bg_h_extra = lib_layout.list_path_bg_h_extra,
                    .body_top_gap = list_body_top,
                    .body_side_gap = lib_layout.list_body_side_gap,
                    .body_bottom_gap = lib_layout.list_body_bottom_gap,
                    .scroll_w = lib_layout.list_scroll_w,
                    .scroll_right_gap = lib_layout.list_scroll_right_gap,
                    .hint_left_gap = lib_layout.list_hint_left_gap,
                    .hint_right_gap = lib_layout.list_hint_right_gap,
                    .hint_bottom_gap = lib_layout.list_hint_bottom_gap,
                    .hint_h = lib_layout.list_hint_h,
                });
        ::ui::scene::CardBuilder list_card{builder, list_card_rect, 0, 0,
                                           ::ui::scene::LayoutAlign::Start,
                                           static_cast<StyleClassId>(PlayerStyleClass::LibraryListCard),
                                           true};
        const WidgetHandle list_card_root = list_card.root();
        const WidgetHandle list_header_plate = builder.create_container();
        anchor_rect(builder, list_header_plate, {14, 14, list_card_rect.w - 28, lib_layout.list_header_h - 14});
        {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_corner_radius = true,
                .bg_color = {kUiLibraryHeaderPlate.r, kUiLibraryHeaderPlate.g, kUiLibraryHeaderPlate.b, 214},
                .border_color = {0, 0, 0, 0},
                .corner_radius = 24,
            });
            patch.has_gradient_enabled = true;
            patch.gradient_enabled = true;
            patch.has_gradient_start = true;
            patch.gradient_start = {42, 50, 69, 222};
            patch.has_gradient_end = true;
            patch.gradient_end = {28, 33, 46, 214};
            patch.has_gradient_direction = true;
            patch.gradient_direction = 0;
            builder.set_style_override(list_header_plate, patch);
            builder.set_hit_testable(list_header_plate, false);
        }
        const int list_context_tray_h = 46;
        const int list_context_tray_gap = 8;
        const int list_context_tray_side_gap = lib_layout.list_body_side_gap + 2;
        const int list_context_tray_y = list_header_layout.body_rect.y + 2;
        const int list_context_tray_w = list_card_rect.w - list_context_tray_side_gap * 2;
        const int list_context_icon_size = 18;
        const int list_context_icon_x = list_context_tray_side_gap + 14;
        const int list_context_icon_y = list_context_tray_y + (list_context_tray_h - list_context_icon_size) / 2;
        const int list_body_plate_y = list_header_layout.body_rect.y - 4;
        Rect list_body_rect = list_header_layout.body_rect;
        list_body_rect.y += list_context_tray_h + list_context_tray_gap;
        list_body_rect.h -= list_context_tray_h + list_context_tray_gap;
        Rect list_scroll_rect = list_header_layout.scroll_rect;
        list_scroll_rect.y += list_context_tray_h + list_context_tray_gap;
        list_scroll_rect.h -= list_context_tray_h + list_context_tray_gap;
        const WidgetHandle list_body_plate = builder.create_container();
        anchor_rect(builder, list_body_plate,
                    {lib_layout.list_body_side_gap - 6, list_body_plate_y,
                     list_card_rect.w - (lib_layout.list_body_side_gap - 6) * 2,
                     list_card_rect.h - list_body_plate_y - 10});
        {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_corner_radius = true,
                .bg_color = {kUiLibraryBodyPlate.r, kUiLibraryBodyPlate.g, kUiLibraryBodyPlate.b, 232},
                .border_color = {0, 0, 0, 0},
                .corner_radius = 22,
            });
            patch.has_gradient_enabled = true;
            patch.gradient_enabled = true;
            patch.has_gradient_start = true;
            patch.gradient_start = {25, 30, 43, 238};
            patch.has_gradient_end = true;
            patch.gradient_end = {15, 19, 28, 232};
            patch.has_gradient_direction = true;
            patch.gradient_direction = 0;
            builder.set_style_override(list_body_plate, patch);
            builder.set_hit_testable(list_body_plate, false);
        }
        h.list_context_root = builder.create_container();
        anchor_rect(builder, h.list_context_root,
                    {0, 0,
                     list_card_rect.w, list_context_tray_y + list_context_tray_h + 8});
        builder.set_hit_testable(h.list_context_root, false);
        const WidgetHandle list_context_tray = builder.create_container();
        anchor_rect(builder, list_context_tray,
                    {list_context_tray_side_gap, list_context_tray_y,
                     list_context_tray_w, list_context_tray_h});
        {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_corner_radius = true,
                .bg_color = {kUiLibraryHeaderPlate.r, kUiLibraryHeaderPlate.g, kUiLibraryHeaderPlate.b, 182},
                .border_color = {0, 0, 0, 0},
                .corner_radius = 22,
            });
            patch.has_gradient_enabled = true;
            patch.gradient_enabled = true;
            patch.has_gradient_start = true;
            patch.gradient_start = {34, 40, 56, 194};
            patch.has_gradient_end = true;
            patch.gradient_end = {22, 28, 40, 186};
            patch.has_gradient_direction = true;
            patch.gradient_direction = 0;
            builder.set_style_override(list_context_tray, patch);
            builder.set_hit_testable(list_context_tray, false);
        }
        const WidgetHandle list_context_tray_divider = builder.create_container();
        anchor_rect(builder, list_context_tray_divider,
                    {list_context_tray_side_gap + 10,
                     list_context_tray_y + list_context_tray_h + 4,
                     list_context_tray_w - 20, 2});
        {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_corner_radius = true,
                .bg_color = {kUiLibraryChipTextMuted.r, kUiLibraryChipTextMuted.g, kUiLibraryChipTextMuted.b, 52},
                .border_color = {0, 0, 0, 0},
                .corner_radius = 1,
            });
            builder.set_style_override(list_context_tray_divider, patch);
            builder.set_hit_testable(list_context_tray_divider, false);
        }
        h.list_context_cover_placeholder = builder.create_button_static("");
        anchor_rect(builder, h.list_context_cover_placeholder,
                    {list_context_icon_x, list_context_icon_y,
                     list_context_icon_size, list_context_icon_size});
        builder.set_button_icon(h.list_context_cover_placeholder, icons.folder);
        builder.set_button_icon_size(h.list_context_cover_placeholder, 16);
        {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_corner_radius = true,
                .bg_color = {0, 0, 0, 0},
                .border_color = {0, 0, 0, 0},
                .corner_radius = 10,
            });
            patch.has_font_color = true;
            patch.font_color = {kUiLibraryChipText.r, kUiLibraryChipText.g, kUiLibraryChipText.b, 172};
            builder.set_style_override(h.list_context_cover_placeholder, patch);
            builder.set_hit_testable(h.list_context_cover_placeholder, false);
        }
        const int list_context_text_x = list_context_icon_x + list_context_icon_size + 12;
        const int list_context_text_w = list_context_tray_side_gap + list_context_tray_w - list_context_text_x - 16;
        h.list_context_title = builder.create_label_static("");
        anchor_rect(builder, h.list_context_title,
                    {list_context_text_x, list_context_tray_y + 11,
                     list_context_text_w, 18});
        apply_player_text_style(builder, h.list_context_title, PlayerTextRole::CardTitle, kUiTitle);
        {
            StylePatch patch{};
            patch.has_font = true;
            patch.font = &get_player_font_px(17, FontWeight::Bold);
            builder.set_style_patch(h.list_context_title, patch);
        }
        builder.set_label_align(h.list_context_title, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        builder.set_hit_testable(h.list_context_title, false);
        h.list_context_subtitle = builder.create_label_static("");
        anchor_rect(builder, h.list_context_subtitle,
                    {list_context_text_x, list_context_tray_y + 31,
                     list_context_text_w, 14});
        apply_player_text_style(builder, h.list_context_subtitle, PlayerTextRole::MetaText, kUiSubtitle);
        {
            StylePatch patch{};
            patch.has_font = true;
            patch.font = &get_player_font_px(12, FontWeight::Medium);
            builder.set_style_patch(h.list_context_subtitle, patch);
        }
        builder.set_label_align(h.list_context_subtitle, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        builder.set_hit_testable(h.list_context_subtitle, false);
        builder.link(h.root, library);
        build_bottom_chrome(builder, h, layout, icons);
        builder.link(library_layers.backdrop, library_bg_top);
        builder.link(library_layers.backdrop, library_bg_bottom);
        builder.link(library_layers.chrome, lib_top);
        builder.link(lib_top, h.library_hero_title);
        builder.link(lib_top, lib_subtitle);
        builder.link(lib_top, lib_settings);
        builder.link(library_layers.content, library_controls);
        builder.link(library_layers.content, tabs_plate);
        builder.link(library_layers.content, tabs_root);
        builder.link(library_layers.content, h.list_tab_indicator);
        builder.link(library_layers.content, action_row_plate);
        builder.link(library_layers.content, shuffle_root);
        builder.link(library_layers.content, list_card_root);
        builder.link(library_layers.content, h.list_sort);
        builder.link(list_card_root, list_body_plate);
        builder.link(list_card_root, h.list_context_root);
        builder.link(h.list_context_root, list_context_tray);
        builder.link(h.list_context_root, list_context_tray_divider);
        builder.link(h.list_context_root, h.list_context_cover_placeholder);
        builder.link(h.list_context_root, h.list_context_title);
        builder.link(h.list_context_root, h.list_context_subtitle);
        builder.link(list_card_root, list_header_plate);
        Rect list_title_rect = list_header_layout.title_rect;
        list_title_rect.y += 2;
        anchor_rect(builder, h.list_title, list_title_rect);
        apply_player_text_style(builder, h.list_title, PlayerTextRole::CardTitle, kUiTitle);
        builder.set_label_align(h.list_title, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        builder.set_hit_testable(h.list_title, false);
        anchor_rect(builder, h.list_sort, sort_action_rect);
        const Rect list_path_rect = list_header_layout.path_rect;
        h.list_path_bg = builder.create_container();
        anchor_rect(builder, h.list_path_bg, list_path_rect);
        {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_corner_radius = true,
                .bg_color = kUiLibraryPathIdle,
                .border_color = {0, 0, 0, 0},
                .corner_radius = lib_layout.list_path_bg_radius,
            });
            builder.set_style_override(h.list_path_bg, patch);
            builder.set_hit_testable(h.list_path_bg, false);
        }
        const WidgetHandle list_path_icon = builder.create_button_static("");
        const ::ui::scene::PathBarSpec list_path_spec{
            .left_gap = lib_layout.list_path_icon_left_gap,
            .right_gap = lib_layout.list_path_text_right_gap,
            .icon_top_gap = lib_layout.list_path_icon_top_gap,
            .label_top_gap = lib_layout.list_path_text_top,
            .icon_size = lib_layout.list_path_icon_size,
            .icon_gap = lib_layout.list_path_text_left - lib_layout.list_path_icon_left_gap - lib_layout.list_path_icon_size,
            .content_h = lib_layout.list_path_h,
        };
        const ::ui::scene::PathBarLayout list_path_layout =
            ::ui::scene::make_path_bar_layout(list_path_rect, list_path_spec);
        anchor_rect(builder, list_path_icon, list_path_layout.icon_rect);
        builder.set_button_icon(list_path_icon, icons.folder);
        builder.set_button_icon_size(list_path_icon, lib_layout.list_path_icon_px);
        apply_tab_base_style(builder, list_path_icon, lib_layout.list_path_icon_radius);
        {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_corner_radius = false,
                .bg_color = {0, 0, 0, 0},
                .border_color = {0, 0, 0, 0},
            });
            builder.set_style_override(list_path_icon, patch);
        }
        builder.set_hit_testable(list_path_icon, false);
        anchor_rect(builder, h.list_path, list_path_layout.label_rect);
        builder.set_hit_testable(h.list_path, false);
        h.list_path_hit = builder.create_button_static("");
        anchor_rect(builder, h.list_path_hit, list_path_rect);
        {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_corner_radius = false,
                .bg_color = {0, 0, 0, 0},
                .border_color = {0, 0, 0, 0},
            });
            patch.has_font_color = true;
            patch.font_color = {0, 0, 0, 0};
            builder.set_style_override(h.list_path_hit, patch);
        }
        apply_tab_base_style(builder, h.list_sort, lib_layout.list_sort_radius);
        builder.set_button_icon(h.list_sort, icons.down);
        builder.set_button_icon_size(h.list_sort, lib_layout.list_sort_icon_px);
        apply_player_text_style(builder, h.list_sort, PlayerTextRole::MetaText, kUiLibraryChipText);
        builder.set_label_align(h.list_sort, ::ui::scene::TextAlignH::Center, ::ui::scene::TextAlignV::Center);
        apply_player_text_style(builder, h.list_path, PlayerTextRole::MetaText, kUiLibraryPathText);
        builder.set_label_align(h.list_path, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, h.list, list_body_rect);
        {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_corner_radius = false,
                .bg_color = kUiLibrarySurfaceBg,
                .border_color = {0, 0, 0, 0},
            });
            patch.has_border_focus = true;
            patch.border_focus = {0, 0, 0, 0};
            builder.set_style_override(h.list, patch);
        }
        const WidgetHandle list_scroll_rail = builder.create_container();
        anchor_rect(builder, list_scroll_rail,
                    {list_scroll_rect.x + 1, list_scroll_rect.y - 2,
                     std::max(2, list_scroll_rect.w - 2), list_scroll_rect.h + 4});
        {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_corner_radius = true,
                .bg_color = {kUiLibraryChipTextMuted.r, kUiLibraryChipTextMuted.g, kUiLibraryChipTextMuted.b, 44},
                .border_color = {0, 0, 0, 0},
                .corner_radius = 5,
            });
            builder.set_style_override(list_scroll_rail, patch);
            builder.set_hit_testable(list_scroll_rail, false);
        }
        anchor_rect(builder, h.list_scroll, list_scroll_rect);
        const WidgetHandle list_hint_plate = builder.create_container();
        anchor_rect(builder, list_hint_plate,
                    {list_header_layout.hint_rect.x - 4, list_header_layout.hint_rect.y - 4,
                     list_header_layout.hint_rect.w + 8, list_header_layout.hint_rect.h + 8});
        {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_corner_radius = true,
                .bg_color = {kUiLibraryPathIdle.r, kUiLibraryPathIdle.g, kUiLibraryPathIdle.b, 188},
                .border_color = {0, 0, 0, 0},
                .corner_radius = 13,
            });
            builder.set_style_override(list_hint_plate, patch);
            builder.set_hit_testable(list_hint_plate, false);
        }
        anchor_rect(builder, h.list_hint, list_header_layout.hint_rect);
        apply_player_text_style(builder, h.list_hint, PlayerTextRole::MetaText, kUiSubtitle);
        builder.set_label_align(h.list_hint, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        builder.set_hit_testable(h.list_hint, false);

        builder.link(list_card_root, h.list);
        builder.link(list_card_root, list_scroll_rail);
        builder.link(list_card_root, h.list_scroll);
        builder.link(list_card_root, h.list_title);
        builder.link(list_card_root, h.list_path_bg);
        builder.link(list_card_root, h.list_path);
        builder.link(list_card_root, list_hint_plate);
        builder.link(list_card_root, h.list_hint);
        builder.link(list_card_root, list_path_icon);
        builder.link(list_card_root, h.list_path_hit);

        const ::ui::scene::AnchoredMenuSpec list_action_menu_spec{
            .width = 236,
            .padding_left = 36,
            .padding_right = 14,
            .padding_top = 12,
            .padding_bottom = 14,
            .title_height = 16,
            .title_bottom_gap = 10,
            .item_height = 38,
            .item_gap = 8,
            .anchor_right_inset = 10,
            .anchor_top_offset = 6,
            .viewport_margin = 12,
            .card_style_class = static_cast<StyleClassId>(PlayerStyleClass::LibraryListCard),
            .item_style_class = static_cast<StyleClassId>(PlayerStyleClass::TopBarButton),
        };
        const auto list_action_menu = ::ui::scene::build_anchored_menu(
            builder,
            library_layers.popup,
            3,
            list_action_menu_spec);
        h.list_action_scrim = list_action_menu.scrim;
        h.list_action_card = list_action_menu.card;
        h.list_action_title = list_action_menu.title;
        h.list_action_items[0] = list_action_menu.item(0);
        h.list_action_items[1] = list_action_menu.item(1);
        h.list_action_items[2] = list_action_menu.item(2);
        {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_border_width = true,
                .apply_corner_radius = true,
                .bg_color = {kUiLibraryBodyPlate.r, kUiLibraryBodyPlate.g, kUiLibraryBodyPlate.b, 248},
                .border_color = {kUiLibraryHeaderBorder.r, kUiLibraryHeaderBorder.g, kUiLibraryHeaderBorder.b, 52},
                .border_width = 1,
                .corner_radius = 24,
            });
            patch.has_gradient_enabled = true;
            patch.gradient_enabled = true;
            patch.has_gradient_start = true;
            patch.gradient_start = {40, 46, 66, 248};
            patch.has_gradient_end = true;
            patch.gradient_end = {16, 20, 30, 246};
            patch.has_gradient_direction = true;
            patch.gradient_direction = 0;
            builder.set_style_override(h.list_action_card, patch);
        }
        const WidgetHandle list_action_header_band = builder.create_container();
        anchor_rect(builder, list_action_header_band, {10, 10, list_action_menu_spec.width - 20, 32});
        {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_border_width = true,
                .apply_corner_radius = true,
                .bg_color = {kUiLibraryHeaderPlate.r, kUiLibraryHeaderPlate.g, kUiLibraryHeaderPlate.b, 182},
                .border_color = {kUiLibraryChipTextMuted.r, kUiLibraryChipTextMuted.g, kUiLibraryChipTextMuted.b, 44},
                .border_width = 1,
                .corner_radius = 16,
            });
            patch.has_gradient_enabled = true;
            patch.gradient_enabled = true;
            patch.has_gradient_start = true;
            patch.gradient_start = {52, 60, 84, 182};
            patch.has_gradient_end = true;
            patch.gradient_end = {30, 36, 50, 174};
            patch.has_gradient_direction = true;
            patch.gradient_direction = 0;
            builder.set_style_override(list_action_header_band, patch);
            builder.set_hit_testable(list_action_header_band, false);
        }
        const WidgetHandle list_action_header_icon = builder.create_button_static("");
        anchor_rect(builder, list_action_header_icon, {16, 18, 16, 16});
        builder.set_button_icon(list_action_header_icon, icons.more);
        builder.set_button_icon_size(list_action_header_icon, 14);
        {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_corner_radius = false,
                .bg_color = {0, 0, 0, 0},
                .border_color = {0, 0, 0, 0},
            });
            patch.has_font_color = true;
            patch.font_color = {kUiTimeSoft.r, kUiTimeSoft.g, kUiTimeSoft.b, 188};
            builder.set_style_override(list_action_header_icon, patch);
            builder.set_hit_testable(list_action_header_icon, false);
        }
        apply_player_text_style(builder, h.list_action_title, PlayerTextRole::MetaText, kUiTimeSoft);
        {
            StylePatch patch{};
            patch.has_font = true;
            patch.font = &get_player_font_px(12, FontWeight::Medium);
            builder.set_style_patch(h.list_action_title, patch);
        }
        builder.set_label_align(h.list_action_title,
                                ::ui::scene::TextAlignH::Center,
                                ::ui::scene::TextAlignV::Center);
        for (auto item : h.list_action_items) {
            if (!item) continue;
            {
                StylePatch patch = ::ui::scene::make_clean_surface_patch({
                    .apply_bg_color = true,
                    .apply_border_color = true,
                    .apply_border_width = true,
                    .apply_corner_radius = true,
                    .bg_color = {kUiLibraryChipIdle.r, kUiLibraryChipIdle.g, kUiLibraryChipIdle.b, 230},
                    .border_color = {kUiLibraryHeaderBorder.r, kUiLibraryHeaderBorder.g, kUiLibraryHeaderBorder.b, 34},
                    .border_width = 1,
                    .corner_radius = 18,
                });
                patch.has_font_color = true;
                patch.font_color = kUiLibraryChipText;
                patch.has_border_focus = true;
                patch.border_focus = {kUiLibraryPathTextActive.r,
                                      kUiLibraryPathTextActive.g,
                                      kUiLibraryPathTextActive.b,
                                      192};
                builder.set_style_override(item, patch);
            }
            StylePatch patch{};
            patch.has_font_role = true;
            patch.font_role = FontId::Small;
            patch.has_font_weight = true;
            patch.font_weight = FontWeight::Medium;
            patch.has_padding = true;
            patch.padding = 12;
            builder.set_style_patch(item, patch);
        }
        builder.link(h.list_action_card, list_action_header_band);
        builder.link(h.list_action_card, list_action_header_icon);

        const int info_sheet_side_pad = 12;
        const int info_sheet_bottom_gap = 14;
        const int info_sheet_h = 308;
        const int info_sheet_min_y = layout.top_bar_y + layout.top_bar_h + 24;
        const int info_sheet_dock_y =
            std::max(info_sheet_min_y, layout.bottom_bar_y - info_sheet_h - info_sheet_bottom_gap);
        const Rect info_sheet_rect{
            info_sheet_side_pad,
            info_sheet_dock_y,
            screen_width - info_sheet_side_pad * 2,
            info_sheet_h
        };
        const Rect info_sheet_local{0, 0, info_sheet_rect.w, info_sheet_rect.h};
        const int info_sheet_cover_x = 18;
        const int info_sheet_cover_y = 22;
        const int info_sheet_cover_size = 88;
        const int info_sheet_right_x = info_sheet_cover_x + info_sheet_cover_size + 16;
        const int info_sheet_close_w = 64;
        const int info_sheet_close_h = 30;
        const int info_sheet_close_x = info_sheet_local.w - 18 - info_sheet_close_w;
        h.list_info_scrim = builder.create_button_static("");
        anchor_rect(builder, h.list_info_scrim, {0, 0, screen_width, screen_height});
        {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_corner_radius = false,
                .bg_color = {0, 0, 0, 164},
                .border_color = {0, 0, 0, 0},
            });
            patch.has_font_color = true;
            patch.font_color = {0, 0, 0, 0};
            builder.set_style_override(h.list_info_scrim, patch);
        }
        h.list_info_card = builder.create_container();
        anchor_rect(builder, h.list_info_card, info_sheet_rect);
        {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_border_width = true,
                .apply_corner_radius = true,
                .bg_color = {kUiLibraryBodyPlate.r, kUiLibraryBodyPlate.g, kUiLibraryBodyPlate.b, 246},
                .border_color = {kUiLibraryHeaderBorder.r, kUiLibraryHeaderBorder.g, kUiLibraryHeaderBorder.b, 46},
                .border_width = 1,
                .corner_radius = 30,
            });
            patch.has_gradient_enabled = true;
            patch.gradient_enabled = true;
            patch.has_gradient_start = true;
            patch.gradient_start = {36, 42, 60, 248};
            patch.has_gradient_end = true;
            patch.gradient_end = {18, 22, 32, 246};
            patch.has_gradient_direction = true;
            patch.gradient_direction = 0;
            builder.set_style_override(h.list_info_card, patch);
            builder.set_hit_testable(h.list_info_card, true);
        }
        const WidgetHandle info_sheet_drag_handle = builder.create_container();
        anchor_rect(builder, info_sheet_drag_handle,
                    {(info_sheet_local.w - 42) / 2, 8, 42, 4});
        {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_corner_radius = true,
                .bg_color = {kUiLibraryChipText.r, kUiLibraryChipText.g, kUiLibraryChipText.b, 112},
                .border_color = {0, 0, 0, 0},
                .corner_radius = 2,
            });
            builder.set_style_override(info_sheet_drag_handle, patch);
            builder.set_hit_testable(info_sheet_drag_handle, false);
        }
        const WidgetHandle info_sheet_header_band = builder.create_container();
        anchor_rect(builder, info_sheet_header_band,
                    {info_sheet_right_x - 10, 18,
                     info_sheet_local.w - info_sheet_right_x - 8, 94});
        {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_border_width = true,
                .apply_corner_radius = true,
                .bg_color = {kUiLibraryHeaderPlate.r, kUiLibraryHeaderPlate.g, kUiLibraryHeaderPlate.b, 178},
                .border_color = {kUiLibraryChipTextMuted.r, kUiLibraryChipTextMuted.g, kUiLibraryChipTextMuted.b, 42},
                .border_width = 1,
                .corner_radius = 22,
            });
            patch.has_gradient_enabled = true;
            patch.gradient_enabled = true;
            patch.has_gradient_start = true;
            patch.gradient_start = {48, 56, 78, 176};
            patch.has_gradient_end = true;
            patch.gradient_end = {28, 34, 48, 164};
            patch.has_gradient_direction = true;
            patch.gradient_direction = 0;
            builder.set_style_override(info_sheet_header_band, patch);
            builder.set_hit_testable(info_sheet_header_band, false);
        }
        h.list_info_cover_plate = builder.create_container();
        anchor_rect(builder, h.list_info_cover_plate,
                    {info_sheet_cover_x, info_sheet_cover_y,
                     info_sheet_cover_size, info_sheet_cover_size});
        {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_border_width = true,
                .apply_corner_radius = true,
                .bg_color = {24, 26, 38, 255},
                .border_color = {kUiLibraryPathTextActive.r, kUiLibraryPathTextActive.g, kUiLibraryPathTextActive.b, 54},
                .border_width = 1,
                .corner_radius = 22,
            });
            patch.has_gradient_enabled = true;
            patch.gradient_enabled = true;
            patch.has_gradient_start = true;
            patch.gradient_start = {32, 38, 54, 255};
            patch.has_gradient_end = true;
            patch.gradient_end = {18, 22, 32, 255};
            patch.has_gradient_direction = true;
            patch.gradient_direction = 0;
            builder.set_style_override(h.list_info_cover_plate, patch);
            builder.set_hit_testable(h.list_info_cover_plate, false);
        }
        const WidgetHandle info_sheet_cover_placeholder = builder.create_container();
        anchor_rect(builder, info_sheet_cover_placeholder,
                    {info_sheet_cover_x + 12, info_sheet_cover_y + 12,
                     info_sheet_cover_size - 24, info_sheet_cover_size - 24});
        {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_corner_radius = true,
                .bg_color = {kUiLibraryChipIdle.r, kUiLibraryChipIdle.g, kUiLibraryChipIdle.b, 92},
                .border_color = {0, 0, 0, 0},
                .corner_radius = 16,
            });
            builder.set_style_override(info_sheet_cover_placeholder, patch);
            builder.set_hit_testable(info_sheet_cover_placeholder, false);
        }
        const WidgetHandle info_sheet_cover_icon = builder.create_button_static("");
        anchor_rect(builder, info_sheet_cover_icon,
                    {info_sheet_cover_x + (info_sheet_cover_size - 20) / 2,
                     info_sheet_cover_y + 20,
                     20, 20});
        builder.set_button_icon(info_sheet_cover_icon, icons.folder);
        builder.set_button_icon_size(info_sheet_cover_icon, 16);
        {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_corner_radius = false,
                .bg_color = {0, 0, 0, 0},
                .border_color = {0, 0, 0, 0},
            });
            patch.has_font_color = true;
            patch.font_color = {kUiLibraryChipText.r, kUiLibraryChipText.g, kUiLibraryChipText.b, 176};
            builder.set_style_override(info_sheet_cover_icon, patch);
            builder.set_hit_testable(info_sheet_cover_icon, false);
        }
        const WidgetHandle info_sheet_cover_label = builder.create_label_static("No cover");
        anchor_rect(builder, info_sheet_cover_label,
                    {info_sheet_cover_x + 10,
                     info_sheet_cover_y + 54,
                     info_sheet_cover_size - 20,
                     14});
        apply_player_text_style(builder, info_sheet_cover_label, PlayerTextRole::MetaText, kUiTimeSoft);
        {
            StylePatch patch{};
            patch.has_font = true;
            patch.font = &get_player_font_px(11, FontWeight::Medium);
            builder.set_style_patch(info_sheet_cover_label, patch);
        }
        builder.set_label_align(info_sheet_cover_label, ::ui::scene::TextAlignH::Center, ::ui::scene::TextAlignV::Center);
        builder.set_hit_testable(info_sheet_cover_label, false);
        h.list_info_cover = builder.create_image();
        anchor_rect(builder, h.list_info_cover,
                    {info_sheet_cover_x, info_sheet_cover_y,
                     info_sheet_cover_size, info_sheet_cover_size});
        ::ui::scene::configure_image_surface(builder, h.list_info_cover, 20);
        h.list_info_eyebrow = builder.create_label_static("Track info");
        anchor_rect(builder, h.list_info_eyebrow, {info_sheet_right_x, 28, 120, 14});
        apply_player_text_style(builder, h.list_info_eyebrow, PlayerTextRole::MetaText, kUiOk);
        {
            StylePatch patch{};
            patch.has_font = true;
            patch.font = &get_player_font_px(12, FontWeight::Medium);
            builder.set_style_patch(h.list_info_eyebrow, patch);
        }
        builder.set_label_align(h.list_info_eyebrow, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        builder.set_hit_testable(h.list_info_eyebrow, false);
        h.list_info_close = builder.create_button_static("Close");
        anchor_rect(builder, h.list_info_close,
                    {info_sheet_close_x, 22, info_sheet_close_w, info_sheet_close_h});
        apply_tab_base_style(builder, h.list_info_close, 16);
        {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_corner_radius = true,
                .bg_color = kUiLibraryChipIdle,
                .border_color = {0, 0, 0, 0},
                .corner_radius = 16,
            });
            patch.has_font_color = true;
            patch.font_color = kUiLibraryChipText;
            patch.has_font_role = true;
            patch.font_role = FontId::Small;
            patch.has_font_weight = true;
            patch.font_weight = FontWeight::Regular;
            patch.has_border_focus = true;
            patch.border_focus = {kUiLibraryPathTextActive.r,
                                  kUiLibraryPathTextActive.g,
                                  kUiLibraryPathTextActive.b,
                                  188};
            builder.set_style_override(h.list_info_close, patch);
        }
        builder.set_label_align(h.list_info_close, ::ui::scene::TextAlignH::Center, ::ui::scene::TextAlignV::Center);
        h.list_info_title = builder.create_label_static("");
        anchor_rect(builder, h.list_info_title,
                    {info_sheet_right_x, 48,
                     info_sheet_close_x - info_sheet_right_x - 12, 26});
        apply_player_text_style(builder, h.list_info_title, PlayerTextRole::CardTitle, kUiTitle);
        {
            StylePatch patch{};
            patch.has_font = true;
            patch.font = &get_player_font_px(19, FontWeight::Bold);
            builder.set_style_patch(h.list_info_title, patch);
        }
        builder.set_label_align(h.list_info_title, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        builder.set_hit_testable(h.list_info_title, false);
        h.list_info_subtitle = builder.create_label_static("");
        anchor_rect(builder, h.list_info_subtitle,
                    {info_sheet_right_x, 78,
                     info_sheet_local.w - info_sheet_right_x - 18, 18});
        apply_player_text_style(builder, h.list_info_subtitle, PlayerTextRole::MetaText, kUiSubtitle);
        {
            StylePatch patch{};
            patch.has_font = true;
            patch.font = &get_player_font_px(13, FontWeight::Regular);
            builder.set_style_patch(h.list_info_subtitle, patch);
        }
        builder.set_label_align(h.list_info_subtitle, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        builder.set_hit_testable(h.list_info_subtitle, false);
        const WidgetHandle info_sheet_details_marker = builder.create_container();
        anchor_rect(builder, info_sheet_details_marker, {20, 127, 12, 8});
        {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_corner_radius = true,
                .bg_color = {kUiOk.r, kUiOk.g, kUiOk.b, 214},
                .border_color = {0, 0, 0, 0},
                .corner_radius = 4,
            });
            builder.set_style_override(info_sheet_details_marker, patch);
            builder.set_hit_testable(info_sheet_details_marker, false);
        }
        const WidgetHandle info_sheet_details_title = builder.create_label_static("Track details");
        anchor_rect(builder, info_sheet_details_title, {40, 124, 104, 14});
        apply_player_text_style(builder, info_sheet_details_title, PlayerTextRole::MetaText, kUiTime);
        {
            StylePatch patch{};
            patch.has_font = true;
            patch.font = &get_player_font_px(12, FontWeight::Medium);
            builder.set_style_patch(info_sheet_details_title, patch);
        }
        builder.set_label_align(info_sheet_details_title, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        builder.set_hit_testable(info_sheet_details_title, false);
        const WidgetHandle info_sheet_meta_plate = builder.create_container();
        anchor_rect(builder, info_sheet_meta_plate, {18, 142, info_sheet_local.w - 36, 36});
        {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_border_width = true,
                .apply_corner_radius = true,
                .bg_color = {kUiLibraryChipIdle.r, kUiLibraryChipIdle.g, kUiLibraryChipIdle.b, 218},
                .border_color = {kUiLibraryHeaderBorder.r, kUiLibraryHeaderBorder.g, kUiLibraryHeaderBorder.b, 34},
                .border_width = 1,
                .corner_radius = 16,
            });
            patch.has_gradient_enabled = true;
            patch.gradient_enabled = true;
            patch.has_gradient_start = true;
            patch.gradient_start = {38, 44, 60, 222};
            patch.has_gradient_end = true;
            patch.gradient_end = {24, 28, 40, 216};
            patch.has_gradient_direction = true;
            patch.gradient_direction = 0;
            builder.set_style_override(info_sheet_meta_plate, patch);
            builder.set_hit_testable(info_sheet_meta_plate, false);
        }
        const WidgetHandle info_sheet_meta_dot = builder.create_container();
        anchor_rect(builder, info_sheet_meta_dot, {30, 155, 10, 10});
        {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_corner_radius = true,
                .bg_color = {kUiLibraryPathTextActive.r, kUiLibraryPathTextActive.g, kUiLibraryPathTextActive.b, 212},
                .border_color = {0, 0, 0, 0},
                .corner_radius = 5,
            });
            builder.set_style_override(info_sheet_meta_dot, patch);
            builder.set_hit_testable(info_sheet_meta_dot, false);
        }
        h.list_info_meta = builder.create_label_static("");
        anchor_rect(builder, h.list_info_meta, {48, 151, info_sheet_local.w - 76, 18});
        apply_player_text_style(builder, h.list_info_meta, PlayerTextRole::MetaText, kUiLibraryChipText);
        {
            StylePatch patch{};
            patch.has_font = true;
            patch.font = &get_player_font_px(12, FontWeight::Medium);
            builder.set_style_patch(h.list_info_meta, patch);
        }
        builder.set_label_align(h.list_info_meta, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        builder.set_hit_testable(h.list_info_meta, false);
        h.list_info_path_plate = builder.create_container();
        anchor_rect(builder, h.list_info_path_plate, {18, 192, info_sheet_local.w - 36, 86});
        {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_border_width = true,
                .apply_corner_radius = true,
                .bg_color = {kUiLibraryPathIdle.r, kUiLibraryPathIdle.g, kUiLibraryPathIdle.b, 244},
                .border_color = {kUiLibraryPathTextActive.r, kUiLibraryPathTextActive.g, kUiLibraryPathTextActive.b, 38},
                .border_width = 1,
                .corner_radius = 20,
            });
            patch.has_gradient_enabled = true;
            patch.gradient_enabled = true;
            patch.has_gradient_start = true;
            patch.gradient_start = {42, 48, 66, 244};
            patch.has_gradient_end = true;
            patch.gradient_end = {26, 32, 46, 242};
            patch.has_gradient_direction = true;
            patch.gradient_direction = 0;
            builder.set_style_override(h.list_info_path_plate, patch);
            builder.set_hit_testable(h.list_info_path_plate, false);
        }
        const WidgetHandle info_sheet_path_header_band = builder.create_container();
        anchor_rect(builder, info_sheet_path_header_band, {24, 196, info_sheet_local.w - 48, 22});
        {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_corner_radius = true,
                .bg_color = {kUiLibraryPathTextActive.r, kUiLibraryPathTextActive.g, kUiLibraryPathTextActive.b, 36},
                .border_color = {0, 0, 0, 0},
                .corner_radius = 11,
            });
            builder.set_style_override(info_sheet_path_header_band, patch);
            builder.set_hit_testable(info_sheet_path_header_band, false);
        }
        const WidgetHandle info_sheet_path_icon = builder.create_button_static("");
        anchor_rect(builder, info_sheet_path_icon, {30, 198, 18, 18});
        builder.set_button_icon(info_sheet_path_icon, icons.folder);
        builder.set_button_icon_size(info_sheet_path_icon, 16);
        {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_corner_radius = false,
                .bg_color = {0, 0, 0, 0},
                .border_color = {0, 0, 0, 0},
            });
            patch.has_font_color = true;
            patch.font_color = {kUiLibraryPathText.r, kUiLibraryPathText.g, kUiLibraryPathText.b, 180};
            builder.set_style_override(info_sheet_path_icon, patch);
            builder.set_hit_testable(info_sheet_path_icon, false);
        }
        h.list_info_path_title = builder.create_label_static("File location");
        anchor_rect(builder, h.list_info_path_title, {52, 200, 128, 14});
        apply_player_text_style(builder, h.list_info_path_title, PlayerTextRole::MetaText, kUiTime);
        {
            StylePatch patch{};
            patch.has_font = true;
            patch.font = &get_player_font_px(12, FontWeight::Medium);
            builder.set_style_patch(h.list_info_path_title, patch);
        }
        builder.set_label_align(h.list_info_path_title, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        builder.set_hit_testable(h.list_info_path_title, false);
        const WidgetHandle info_sheet_path_trail_a = builder.create_container();
        anchor_rect(builder, info_sheet_path_trail_a, {info_sheet_local.w - 68, 205, 20, 5});
        {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_corner_radius = true,
                .bg_color = {kUiLibraryPathText.r, kUiLibraryPathText.g, kUiLibraryPathText.b, 94},
                .border_color = {0, 0, 0, 0},
                .corner_radius = 3,
            });
            builder.set_style_override(info_sheet_path_trail_a, patch);
            builder.set_hit_testable(info_sheet_path_trail_a, false);
        }
        const WidgetHandle info_sheet_path_trail_b = builder.create_container();
        anchor_rect(builder, info_sheet_path_trail_b, {info_sheet_local.w - 40, 205, 8, 5});
        {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_corner_radius = true,
                .bg_color = {kUiLibraryPathTextActive.r, kUiLibraryPathTextActive.g, kUiLibraryPathTextActive.b, 156},
                .border_color = {0, 0, 0, 0},
                .corner_radius = 3,
            });
            builder.set_style_override(info_sheet_path_trail_b, patch);
            builder.set_hit_testable(info_sheet_path_trail_b, false);
        }
        const WidgetHandle info_sheet_section_divider = builder.create_container();
        anchor_rect(builder, info_sheet_section_divider, {26, 184, info_sheet_local.w - 52, 2});
        {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_corner_radius = true,
                .bg_color = {kUiLibraryChipTextMuted.r, kUiLibraryChipTextMuted.g, kUiLibraryChipTextMuted.b, 44},
                .border_color = {0, 0, 0, 0},
                .corner_radius = 1,
            });
            builder.set_style_override(info_sheet_section_divider, patch);
            builder.set_hit_testable(info_sheet_section_divider, false);
        }
        h.list_info_path = builder.create_label_static("");
        anchor_rect(builder, h.list_info_path, {28, 224, info_sheet_local.w - 56, 18});
        apply_player_text_style(builder, h.list_info_path, PlayerTextRole::MetaText, kUiLibraryPathTextActive);
        {
            StylePatch patch{};
            patch.has_font = true;
            patch.font = &get_player_font_px(13, FontWeight::Medium);
            builder.set_style_patch(h.list_info_path, patch);
        }
        builder.set_label_align(h.list_info_path, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        builder.set_hit_testable(h.list_info_path, false);
        h.list_info_path_detail = builder.create_label_static("");
        anchor_rect(builder, h.list_info_path_detail, {28, 247, info_sheet_local.w - 56, 14});
        apply_player_text_style(builder, h.list_info_path_detail, PlayerTextRole::MetaText, kUiLibraryPathText);
        {
            StylePatch patch{};
            patch.has_font = true;
            patch.font = &get_player_font_px(11, FontWeight::Regular);
            builder.set_style_patch(h.list_info_path_detail, patch);
        }
        builder.set_label_align(h.list_info_path_detail, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        builder.set_hit_testable(h.list_info_path_detail, false);
        h.list_info_hint = builder.create_label_static("Tap outside or press Back");
        anchor_rect(builder, h.list_info_hint,
                    {18, info_sheet_local.h - 24, info_sheet_local.w - 36, 14});
        apply_player_text_style(builder, h.list_info_hint, PlayerTextRole::MetaText, kUiTimeSoft);
        {
            StylePatch patch{};
            patch.has_font = true;
            patch.font = &get_player_font_px(11, FontWeight::Medium);
            builder.set_style_patch(h.list_info_hint, patch);
        }
        builder.set_label_align(h.list_info_hint, ::ui::scene::TextAlignH::Right, ::ui::scene::TextAlignV::Center);
        builder.set_hit_testable(h.list_info_hint, false);
        const WidgetHandle info_sheet_footer_marker = builder.create_container();
        anchor_rect(builder, info_sheet_footer_marker, {18, info_sheet_local.h - 20, 18, 4});
        {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_corner_radius = true,
                .bg_color = {kUiLibraryPathTextActive.r, kUiLibraryPathTextActive.g, kUiLibraryPathTextActive.b, 136},
                .border_color = {0, 0, 0, 0},
                .corner_radius = 2,
            });
            builder.set_style_override(info_sheet_footer_marker, patch);
            builder.set_hit_testable(info_sheet_footer_marker, false);
        }
        const WidgetHandle info_sheet_footer_marker_soft = builder.create_container();
        anchor_rect(builder, info_sheet_footer_marker_soft, {40, info_sheet_local.h - 20, 8, 4});
        {
            StylePatch patch = ::ui::scene::make_clean_surface_patch({
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_corner_radius = true,
                .bg_color = {kUiLibraryChipTextMuted.r, kUiLibraryChipTextMuted.g, kUiLibraryChipTextMuted.b, 88},
                .border_color = {0, 0, 0, 0},
                .corner_radius = 2,
            });
            builder.set_style_override(info_sheet_footer_marker_soft, patch);
            builder.set_hit_testable(info_sheet_footer_marker_soft, false);
        }

        builder.link(library_layers.popup, h.list_info_scrim);
        builder.link(library_layers.popup, h.list_info_card);
        builder.link(h.list_info_card, info_sheet_drag_handle);
        builder.link(h.list_info_card, info_sheet_header_band);
        builder.link(h.list_info_card, h.list_info_cover_plate);
        builder.link(h.list_info_card, info_sheet_cover_placeholder);
        builder.link(h.list_info_card, info_sheet_cover_icon);
        builder.link(h.list_info_card, info_sheet_cover_label);
        builder.link(h.list_info_card, h.list_info_cover);
        builder.link(h.list_info_card, h.list_info_eyebrow);
        builder.link(h.list_info_card, h.list_info_close);
        builder.link(h.list_info_card, h.list_info_title);
        builder.link(h.list_info_card, h.list_info_subtitle);
        builder.link(h.list_info_card, info_sheet_details_marker);
        builder.link(h.list_info_card, info_sheet_details_title);
        builder.link(h.list_info_card, info_sheet_meta_plate);
        builder.link(h.list_info_card, info_sheet_meta_dot);
        builder.link(h.list_info_card, h.list_info_meta);
        builder.link(h.list_info_card, info_sheet_section_divider);
        builder.link(h.list_info_card, h.list_info_path_plate);
        builder.link(h.list_info_card, info_sheet_path_header_band);
        builder.link(h.list_info_card, info_sheet_path_icon);
        builder.link(h.list_info_card, h.list_info_path_title);
        builder.link(h.list_info_card, info_sheet_path_trail_a);
        builder.link(h.list_info_card, info_sheet_path_trail_b);
        builder.link(h.list_info_card, h.list_info_path);
        builder.link(h.list_info_card, h.list_info_path_detail);
        builder.link(h.list_info_card, info_sheet_footer_marker);
        builder.link(h.list_info_card, info_sheet_footer_marker_soft);
        builder.link(h.list_info_card, h.list_info_hint);
    }
