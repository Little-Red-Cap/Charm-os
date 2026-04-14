    static void build_library(::ui::scene::SceneBuilder& builder, UiHandles& h,
                              const UiLayout& layout, const PlayerIconIds& icons) {
        const LibraryLayout lib_layout = make_library_layout();
        h.list_title = builder.create_label_static("");
        anchor_rect(builder, h.list_title, {0, 0, 0, 0});
        h.list_path = builder.create_label_static("");
        anchor_rect(builder, h.list_path, {0, 0, 0, 0});
        h.list_sort = builder.create_button_static("Sort");
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

        const WidgetHandle library_bg_top = builder.create_container();
        anchor_rect(builder, library_bg_top, {0, 0, screen_width, screen_height / 2});
        {
            StylePatch patch{};
            patch.has_gradient_enabled = true;
            patch.gradient_enabled = true;
            patch.has_gradient_start = true;
            patch.gradient_start = {112, 166, 222, 190};
            patch.has_gradient_end = true;
            patch.gradient_end = {42, 78, 132, 78};
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
            patch.gradient_start = {24, 52, 96, 108};
            patch.has_gradient_end = true;
            patch.gradient_end = {6, 16, 42, 212};
            patch.has_gradient_direction = true;
            patch.gradient_direction = 0;
            patch.has_border_color = true;
            patch.border_color = {0, 0, 0, 0};
            builder.set_style_patch(library_bg_bottom, patch);
            builder.set_hit_testable(library_bg_bottom, false);
        }

        const WidgetHandle lib_top = builder.create_container();
        const Rect top_bar_rect{layout.top_bar_x, layout.top_bar_y, layout.top_bar_w, layout.top_bar_h};
        const ::ui::scene::TopBarTitleSpec lib_title_spec{
            .left_gap = lib_layout.top_title_x,
            .top_gap = lib_layout.top_title_y,
            .right_gap = lib_layout.top_title_right_gap,
            .height = lib_layout.top_title_h,
        };
        const ::ui::scene::TopBarButtonSpec lib_right_spec{
            .width = lib_layout.top_button_size,
            .height = lib_layout.top_button_size,
            .top_gap = 0,
        };
        const ::ui::scene::TopBarLayout lib_top_layout = ::ui::scene::make_top_bar_layout(
            top_bar_rect,
            nullptr,
            &lib_title_spec,
            &lib_right_spec,
            lib_layout.top_button_right_gap);
        anchor_rect(builder, lib_top, top_bar_rect);
        const WidgetHandle lib_title = builder.create_label_static("Library");
        anchor_rect(builder, lib_title, lib_top_layout.title_rect);
        const WidgetHandle lib_settings = create_top_icon_button(builder,
            lib_top_layout.right_primary_rect,
            icons.settings, lib_layout.top_icon_px);

        h.list_tab_songs = builder.create_button_static("Songs");
        h.list_tab_albums = builder.create_button_static("Albums");
        h.list_tab_artist = builder.create_button_static("Artists");

        h.list_shuffle = builder.create_button_static("Shuffle");
        const int tab_radius = layout.tabs_h / 2;
        apply_tab_base_style(builder, h.list_tab_songs, tab_radius);
        apply_tab_base_style(builder, h.list_tab_albums, tab_radius);
        apply_tab_base_style(builder, h.list_tab_artist, tab_radius);
        apply_tab_base_style(builder, h.list_shuffle, tab_radius);
        apply_tab_active_style(builder, h.list_tab_songs);
        apply_shuffle_shadow_style(builder, h.list_shuffle);

        ::ui::scene::RowBuilder tabs_row{builder,
                                         {kUiPadding, layout.tabs_y,
                                          screen_width - kUiPadding * 2, layout.tabs_h},
                                         layout.tabs_gap, lib_layout.tabs_padding,
                                         ::ui::scene::LayoutAlign::Center};
        const WidgetHandle tabs_root = tabs_row.root();
        tabs_row.add(h.list_tab_songs, layout.tab_w, layout.tabs_h);
        tabs_row.add(h.list_tab_albums, layout.tab_w, layout.tabs_h);
        tabs_row.add(h.list_tab_artist, layout.tab_w, layout.tabs_h);

        ::ui::scene::RowBuilder shuffle_row{builder,
                                            {kUiPadding, layout.shuffle_y, lib_layout.shuffle_w, layout.shuffle_h},
                                            0, lib_layout.shuffle_padding,
                                            ::ui::scene::LayoutAlign::Center};
        const WidgetHandle shuffle_root = shuffle_row.root();
        shuffle_row.add(h.list_shuffle, lib_layout.shuffle_w, layout.shuffle_h);

        const Rect list_card_rect{kUiPadding, layout.list_card_y,
                                  screen_width - kUiPadding * 2, layout.list_card_h};
        const ::ui::scene::ListCardHeaderLayout list_header_layout =
            ::ui::scene::make_list_card_header_layout(
                list_card_rect,
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
                    .body_top_gap = lib_layout.list_header_h,
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

        builder.link(h.root, library);
        builder.link(library, library_bg_top);
        builder.link(library, library_bg_bottom);
        builder.link(library, lib_top);
        builder.link(lib_top, lib_title);
        builder.link(lib_top, lib_settings);
        builder.link(library, tabs_root);
        builder.link(library, shuffle_root);
        builder.link(library, list_card_root);
        anchor_rect(builder, h.list_title, list_header_layout.title_rect);
        anchor_rect(builder, h.list_sort, list_header_layout.action_rect);
        const Rect list_path_rect = list_header_layout.path_rect;
        h.list_path_bg = builder.create_container();
        anchor_rect(builder, h.list_path_bg, list_path_rect);
        {
            StylePatch patch = ::ui::scene::make_pill_surface_patch({
                .bg_color = kUiButtonBg,
                .border_color = kUiButtonBorder,
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
        builder.set_label_align(h.list_sort, ::ui::scene::TextAlignH::Center, ::ui::scene::TextAlignV::Center);
        apply_time_label_style(builder, h.list_path);
        builder.set_label_align(h.list_path, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, h.list, list_header_layout.body_rect);
        anchor_rect(builder, h.list_scroll, list_header_layout.scroll_rect);
        anchor_rect(builder, h.list_hint, list_header_layout.hint_rect);
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
        builder.link(list_card_root, h.list_sort);
        builder.link(list_card_root, h.list_path_bg);
        builder.link(list_card_root, h.list_path);
        builder.link(list_card_root, h.list_hint);
        builder.link(list_card_root, list_path_icon);
        builder.link(list_card_root, h.list_path_hit);

        const auto list_action_menu = ::ui::scene::build_anchored_menu(
            builder,
            library,
            3,
            {
                .card_style_class = static_cast<StyleClassId>(PlayerStyleClass::LibraryListCard),
                .item_style_class = static_cast<StyleClassId>(PlayerStyleClass::TopBarButton),
            });
        h.list_action_scrim = list_action_menu.scrim;
        h.list_action_card = list_action_menu.card;
        h.list_action_title = list_action_menu.title;
        h.list_action_primary = list_action_menu.item(0);
        h.list_action_secondary = list_action_menu.item(1);
        h.list_action_tertiary = list_action_menu.item(2);
        apply_time_label_style(builder, h.list_action_title);

        build_bottom_chrome(builder, h, layout, icons);
    }
