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
        const int library_top_h = 138;
        const int library_tabs_y = library_top_y + library_top_h + 16;
        const int library_tabs_h = 54;
        const int library_tabs_gap = 14;
        const int library_tab_w = (screen_width - kUiPadding * 2 - library_tabs_gap * 2) / 3;
        const int library_shuffle_h = 40;
        const int library_shuffle_y = library_tabs_y + library_tabs_h + 16;
        const int library_list_card_y = library_shuffle_y + library_shuffle_h + 14;
        const int library_list_card_h = std::max(240, layout.bottom_bar_y - library_list_card_y - 20);
        const int controls_inset = kUiPadding - 2;
        const Rect library_controls_rect{
            controls_inset,
            library_tabs_y - 14,
            screen_width - controls_inset * 2,
            (library_shuffle_y + library_shuffle_h) - (library_tabs_y - 14) + 12
        };
        const WidgetHandle library_controls = builder.create_container();
        anchor_rect(builder, library_controls, library_controls_rect);
        {
            StylePatch patch{};
            patch.has_gradient_enabled = true;
            patch.gradient_enabled = true;
            patch.has_gradient_start = true;
            patch.gradient_start = kUiLibraryControlsTop;
            patch.has_gradient_end = true;
            patch.gradient_end = kUiLibraryControlsBottom;
            patch.has_gradient_direction = true;
            patch.gradient_direction = 0;
            patch.has_border_color = true;
            patch.border_color = kUiLibraryControlsBorder;
            patch.has_border_width = true;
            patch.border_width = 1;
            patch.has_corner_radius = true;
            patch.corner_radius = 30;
            patch.has_shadow_enabled = true;
            patch.shadow_enabled = true;
            patch.has_shadow_color = true;
            patch.shadow_color = {0, 0, 0, 34};
            patch.has_shadow_offset_x = true;
            patch.shadow_offset_x = 0;
            patch.has_shadow_offset_y = true;
            patch.shadow_offset_y = 2;
            patch.has_shadow_spread = true;
            patch.shadow_spread = 0;
            patch.has_shadow_radius = true;
            patch.shadow_radius = 12;
            builder.set_style_override(library_controls, patch);
            builder.set_hit_testable(library_controls, false);
        }

        const WidgetHandle lib_top = builder.create_container();
        const Rect top_bar_rect{layout.top_bar_x, library_top_y, layout.top_bar_w, library_top_h};
        const ::ui::scene::TopBarTitleSpec lib_title_spec{
            .left_gap = 2,
            .top_gap = 0,
            .right_gap = 144,
            .height = 110,
        };
        const ::ui::scene::TopBarButtonSpec lib_right_spec{
            .width = 60,
            .height = 60,
            .top_gap = 18,
        };
        const ::ui::scene::TopBarLayout lib_top_layout = ::ui::scene::make_top_bar_layout(
            top_bar_rect,
            nullptr,
            &lib_title_spec,
            &lib_right_spec,
            68);
        anchor_rect(builder, lib_top, top_bar_rect);
        const WidgetHandle lib_title = builder.create_label_static("Library");
        anchor_rect(builder, lib_title, lib_top_layout.title_rect);
        {
            // TODO(player/ui): Promote Library top chrome to a dedicated hero header widget.
            // TODO(player/ui): Fold hero/title variation-axis tuning into reusable text presets after the Library pass stabilizes.
            StylePatch patch = make_player_text_patch(PlayerTextRole::HeroTitle, kUiTitle);
            patch.has_font = true;
            patch.font = &get_player_font_px_variant(92, FontWeight::Bold,
                "ROND100_wdth92_XTRA520_YOPQ90_YTLC505");
            builder.set_style_override(lib_title, patch);
        }
        builder.set_label_align(lib_title, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Top);
        builder.set_hit_testable(lib_title, false);
        const WidgetHandle lib_settings = create_top_icon_button(builder,
            lib_top_layout.right_primary_rect,
            icons.settings, 20);
        {
            StylePatch patch = ::ui::scene::make_pill_surface_patch({
                .bg_color = {20, 86, 212, 244},
                .border_color = {154, 190, 255, 120},
                .corner_radius = 30,
                .shadow = {
                    .enabled = true,
                    .color = {0, 0, 0, 46},
                    .offset_x = 0,
                    .offset_y = 5,
                    .spread = 0,
                    .radius = 16,
                },
            });
            patch.has_font_color = true;
            patch.font_color = {246, 248, 255, 255};
            builder.set_style_override(lib_settings, patch);
        }

        h.list_tab_songs = builder.create_button_static("SONGS");
        h.list_tab_albums = builder.create_button_static("ALBUMS");
        h.list_tab_artist = builder.create_button_static("ARTISTS");

        h.list_shuffle = builder.create_button_static("Shuffle All");
        const int tab_radius = library_tabs_h / 2;
        apply_tab_base_style(builder, h.list_tab_songs, tab_radius);
        apply_tab_base_style(builder, h.list_tab_albums, tab_radius);
        apply_tab_base_style(builder, h.list_tab_artist, tab_radius);
        apply_tab_base_style(builder, h.list_shuffle, tab_radius);
        apply_tab_active_style(builder, h.list_tab_songs);
        apply_shuffle_shadow_style(builder, h.list_shuffle);
        builder.set_button_icon(h.list_shuffle, icons.shuffle);
        builder.set_button_icon_size(h.list_shuffle, lib_layout.shuffle_icon_px);

        ::ui::scene::RowBuilder tabs_row{builder,
                                         {kUiPadding, library_tabs_y,
                                          screen_width - kUiPadding * 2, library_tabs_h},
                                         library_tabs_gap, lib_layout.tabs_padding,
                                         ::ui::scene::LayoutAlign::Center};
        const WidgetHandle tabs_root = tabs_row.root();
        tabs_row.add(h.list_tab_songs, library_tab_w, library_tabs_h);
        tabs_row.add(h.list_tab_albums, library_tab_w, library_tabs_h);
        tabs_row.add(h.list_tab_artist, library_tab_w, library_tabs_h);
        h.list_tab_indicator = builder.create_container();
        anchor_rect(builder, h.list_tab_indicator,
                    {kUiPadding + (library_tab_w - 56) / 2,
                     library_tabs_y + library_tabs_h + 6,
                     56,
                     6});
        {
            StylePatch patch{};
            patch.has_bg_color = true;
            patch.bg_color = {kUiLibraryTabActive.r, kUiLibraryTabActive.g, kUiLibraryTabActive.b, 232};
            patch.has_border_color = true;
            patch.border_color = {kUiLibraryTabBorderActive.r, kUiLibraryTabBorderActive.g, kUiLibraryTabBorderActive.b, 220};
            patch.has_border_width = true;
            patch.border_width = 1;
            patch.has_corner_radius = true;
            patch.corner_radius = 4;
            patch.has_shadow_enabled = true;
            patch.shadow_enabled = true;
            patch.has_shadow_color = true;
            patch.shadow_color = {0, 0, 0, 42};
            patch.has_shadow_offset_y = true;
            patch.shadow_offset_y = 1;
            patch.has_shadow_radius = true;
            patch.shadow_radius = 6;
            builder.set_style_override(h.list_tab_indicator, patch);
            builder.set_hit_testable(h.list_tab_indicator, false);
        }

        ::ui::scene::RowBuilder shuffle_row{builder,
                                            {kUiPadding, library_shuffle_y, screen_width - kUiPadding * 2, library_shuffle_h},
                                            0, lib_layout.shuffle_padding,
                                            ::ui::scene::LayoutAlign::Start};
        const WidgetHandle shuffle_root = shuffle_row.root();
        shuffle_row.add(h.list_shuffle, lib_layout.shuffle_w, library_shuffle_h);
        const Rect sort_action_rect{
            screen_width - kUiPadding - lib_layout.list_sort_w,
            library_shuffle_y,
            lib_layout.list_sort_w,
            library_shuffle_h
        };

        const Rect list_card_rect{kUiPadding, library_list_card_y,
                                  screen_width - kUiPadding * 2, library_list_card_h};
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
        anchor_rect(builder, list_header_plate, {16, 16, list_card_rect.w - 32, lib_layout.list_header_h - 24});
        {
            StylePatch patch{};
            patch.has_bg_color = true;
            patch.bg_color = kUiLibraryHeaderPlate;
            patch.has_border_color = true;
            patch.border_color = kUiLibraryHeaderBorder;
            patch.has_border_width = true;
            patch.border_width = 1;
            patch.has_corner_radius = true;
            patch.corner_radius = 22;
            patch.has_shadow_enabled = true;
            patch.shadow_enabled = true;
            patch.has_shadow_color = true;
            patch.shadow_color = {0, 0, 0, 18};
            patch.has_shadow_offset_y = true;
            patch.shadow_offset_y = 1;
            patch.has_shadow_radius = true;
            patch.shadow_radius = 5;
            builder.set_style_override(list_header_plate, patch);
            builder.set_hit_testable(list_header_plate, false);
        }
        const int list_body_plate_y = list_header_layout.body_rect.y - 2;
        const WidgetHandle list_body_plate = builder.create_container();
        anchor_rect(builder, list_body_plate,
                    {lib_layout.list_body_side_gap - 4, list_body_plate_y,
                     list_card_rect.w - (lib_layout.list_body_side_gap - 4) * 2,
                     list_card_rect.h - list_body_plate_y - 12});
        {
            StylePatch patch{};
            patch.has_gradient_enabled = true;
            patch.gradient_enabled = true;
            patch.has_gradient_start = true;
            patch.gradient_start = {kUiLibraryBodyPlate.r, kUiLibraryBodyPlate.g, kUiLibraryBodyPlate.b, 112};
            patch.has_gradient_end = true;
            patch.gradient_end = {4, 8, 20, 172};
            patch.has_gradient_direction = true;
            patch.gradient_direction = 0;
            patch.has_border_color = true;
            patch.border_color = kUiLibraryBodyBorder;
            patch.has_border_width = true;
            patch.border_width = 1;
            patch.has_corner_radius = true;
            patch.corner_radius = 20;
            builder.set_style_override(list_body_plate, patch);
            builder.set_hit_testable(list_body_plate, false);
        }
        h.list_context_cover = builder.create_image();
        anchor_rect(builder, h.list_context_cover,
                    {list_card_rect.w - 212, list_card_rect.h - 284, 156, 156});
        apply_home_cover_style(builder, h.list_context_cover, 28);
        builder.set_hit_testable(h.list_context_cover, false);
        h.list_context_cover_scrim = builder.create_container();
        anchor_rect(builder, h.list_context_cover_scrim,
                    {list_card_rect.w - 216, list_card_rect.h - 288, 164, 164});
        {
            StylePatch patch{};
            patch.has_bg_color = true;
            patch.bg_color = {8, 14, 30, 176};
            patch.has_border_color = true;
            patch.border_color = {kUiLibraryPathBorderActive.r, kUiLibraryPathBorderActive.g, kUiLibraryPathBorderActive.b, 18};
            patch.has_border_width = true;
            patch.border_width = 1;
            patch.has_corner_radius = true;
            patch.corner_radius = 30;
            builder.set_style_override(h.list_context_cover_scrim, patch);
            builder.set_hit_testable(h.list_context_cover_scrim, false);
        }
        builder.link(h.root, library);
        builder.link(library_layers.backdrop, library_bg_top);
        builder.link(library_layers.backdrop, library_bg_bottom);
        builder.link(library_layers.chrome, lib_top);
        builder.link(lib_top, lib_title);
        builder.link(lib_top, lib_settings);
        builder.link(library_layers.content, library_controls);
        builder.link(library_layers.content, tabs_root);
        builder.link(library_layers.content, h.list_tab_indicator);
        builder.link(library_layers.content, shuffle_root);
        builder.link(library_layers.content, h.list_sort);
        builder.link(library_layers.content, list_card_root);
        builder.link(list_card_root, list_body_plate);
        builder.link(list_card_root, h.list_context_cover);
        builder.link(list_card_root, h.list_context_cover_scrim);
        builder.link(list_card_root, list_header_plate);
        anchor_rect(builder, h.list_title, list_header_layout.title_rect);
        apply_player_text_style(builder, h.list_title, PlayerTextRole::CardTitle, kUiTitle);
        builder.set_label_align(h.list_title, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        builder.set_hit_testable(h.list_title, false);
        anchor_rect(builder, h.list_sort, sort_action_rect);
        const Rect list_path_rect = list_header_layout.path_rect;
        h.list_path_bg = builder.create_container();
        anchor_rect(builder, h.list_path_bg, list_path_rect);
        {
            StylePatch patch = ::ui::scene::make_pill_surface_patch({
                .bg_color = kUiLibraryPathIdle,
                .border_color = kUiLibraryChipBorder,
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
            StylePatch patch{};
            patch.has_bg_color = true;
            patch.bg_color = {0, 0, 0, 0};
            patch.has_border_color = true;
            patch.border_color = {0, 0, 0, 0};
            patch.has_shadow_enabled = true;
            patch.shadow_enabled = false;
            builder.set_style_override(list_path_icon, patch);
        }
        builder.set_hit_testable(list_path_icon, false);
        anchor_rect(builder, h.list_path, list_path_layout.label_rect);
        builder.set_hit_testable(h.list_path, false);
        h.list_path_hit = builder.create_button_static("");
        anchor_rect(builder, h.list_path_hit, list_path_rect);
        {
            StylePatch patch{};
            patch.has_bg_color = true;
            patch.bg_color = {0, 0, 0, 0};
            patch.has_border_color = true;
            patch.border_color = {0, 0, 0, 0};
            patch.has_font_color = true;
            patch.font_color = {0, 0, 0, 0};
            patch.has_shadow_enabled = true;
            patch.shadow_enabled = false;
            builder.set_style_override(h.list_path_hit, patch);
        }
        apply_tab_base_style(builder, h.list_sort, lib_layout.list_sort_radius);
        builder.set_button_icon(h.list_sort, icons.down);
        builder.set_button_icon_size(h.list_sort, lib_layout.list_sort_icon_px);
        apply_player_text_style(builder, h.list_sort, PlayerTextRole::MetaText, kUiLibraryChipText);
        builder.set_label_align(h.list_sort, ::ui::scene::TextAlignH::Center, ::ui::scene::TextAlignV::Center);
        apply_player_text_style(builder, h.list_path, PlayerTextRole::MetaText, kUiLibraryPathText);
        builder.set_label_align(h.list_path, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, h.list, list_header_layout.body_rect);
        anchor_rect(builder, h.list_scroll, list_header_layout.scroll_rect);
        anchor_rect(builder, h.list_hint, list_header_layout.hint_rect);
        apply_player_text_style(builder, h.list_hint, PlayerTextRole::MetaText, kUiSubtitle);
        builder.set_label_align(h.list_hint, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        builder.set_hit_testable(h.list_hint, false);
        {
            StylePatch patch{};
            patch.has_bg_color = true;
            patch.bg_color = {0, 0, 0, 0};
            patch.has_border_color = true;
            patch.border_color = {0, 0, 0, 0};
            builder.set_style_override(h.list, patch);
        }

        builder.link(list_card_root, h.list);
        builder.link(list_card_root, h.list_scroll);
        builder.link(list_card_root, h.list_title);
        builder.link(list_card_root, h.list_path_bg);
        builder.link(list_card_root, h.list_path);
        builder.link(list_card_root, h.list_hint);
        builder.link(list_card_root, list_path_icon);
        builder.link(list_card_root, h.list_path_hit);

        const auto list_action_menu = ::ui::scene::build_anchored_menu(
            builder,
            library_layers.popup,
            3,
            {
                .card_style_class = static_cast<StyleClassId>(PlayerStyleClass::LibraryListCard),
                .item_style_class = static_cast<StyleClassId>(PlayerStyleClass::TopBarButton),
            });
        h.list_action_scrim = list_action_menu.scrim;
        h.list_action_card = list_action_menu.card;
        h.list_action_title = list_action_menu.title;
        h.list_action_items[0] = list_action_menu.item(0);
        h.list_action_items[1] = list_action_menu.item(1);
        h.list_action_items[2] = list_action_menu.item(2);
        apply_time_label_style(builder, h.list_action_title);

        build_bottom_chrome(builder, h, layout, icons);
    }
