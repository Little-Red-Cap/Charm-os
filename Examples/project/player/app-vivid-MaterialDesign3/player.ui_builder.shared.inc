    using namespace player::ui;

    struct UiLayout {
        int top_bar_x{};
        int top_bar_y{};
        int top_bar_w{};
        int top_bar_h{};
        int nav_y{};
        int nav_h{};
        int nav_gap{};
        int bottom_bar_y{};
        int bottom_bar_h{};
        int cover_x{};
        int cover_y{};
        int title_y{};
        int subtitle_y{};
        int progress_y{};
        int time_y{};
        int controls_x{};
        int controls_y{};
        int controls_w{};
        int tabs_y{};
        int tabs_h{};
        int tabs_gap{};
        int tab_w{};
        int shuffle_y{};
        int shuffle_h{};
        int list_card_y{};
        int list_card_h{};
    };

    struct NowTextLayout {
        int text_col_y{};
        int text_col_h{};
        int title_h{};
        int subtitle_h{};
        int progress_h{};
        int time_row_h{};
        int time_row_y{};
        int text_gap{};
    };

    static void anchor_rect(::ui::scene::SceneBuilder& builder, WidgetHandle h, const Rect& r) noexcept {
        builder.set_rect(h, r);
    }

    static UiLayout make_layout() noexcept {
        const int top_bar_h = 44;
        const int top_bar_y = kUiPadding;
        const int top_bar_x = kUiPadding;
        const int top_bar_w = screen_width - kUiPadding * 2;

        const int nav_h = 56;
        const int nav_gap = 12;
        const int bottom_bar_h = 72;
        const int bottom_bar_gap = 12;
        const int nav_y = screen_height - kUiPadding - nav_h;
        const int bottom_bar_y = nav_y - bottom_bar_gap - bottom_bar_h;

        const int cover_top = top_bar_y + top_bar_h + 12;
        const int cover_x = (screen_width - kCoverSize) / 2;
        const int cover_y = cover_top;
        const int header_top = cover_top + kCoverSize;
        const int text_gap = 6;
        const int title_h = 28;
        const int subtitle_h = 14;
        const int progress_h = 3;
        const int text_col_y = header_top + 12;
        const int text_col_h = title_h + subtitle_h + progress_h + text_gap * 2;
        const int controls_y = text_col_y + text_col_h + 64;
        const int control_side = 96;
        const int control_play = 116;
        const int control_gap = 14;
        const int controls_w = control_side * 2 + control_play + control_gap * 2;
        const int controls_x = (screen_width - controls_w) / 2;

        const int tabs_y = top_bar_y + top_bar_h + 10;
        const int tabs_h = 38;
        const int tabs_gap = 12;
        const int tabs_w = screen_width - kUiPadding * 2;
        const int tab_w = (tabs_w - tabs_gap * 2) / 3;

        const int shuffle_y = tabs_y + tabs_h + 10;
        const int shuffle_h = 36;
        const int list_card_y = shuffle_y + shuffle_h + 6;
        const int list_card_h = std::max(240, bottom_bar_y - list_card_y - 18);

        return UiLayout{
            .top_bar_x = top_bar_x,
            .top_bar_y = top_bar_y,
            .top_bar_w = top_bar_w,
            .top_bar_h = top_bar_h,
            .nav_y = nav_y,
            .nav_h = nav_h,
            .nav_gap = nav_gap,
            .bottom_bar_y = bottom_bar_y,
            .bottom_bar_h = bottom_bar_h,
            .cover_x = cover_x,
            .cover_y = cover_y,
            .title_y = text_col_y,
            .subtitle_y = text_col_y,
            .progress_y = text_col_y,
            .time_y = text_col_y,
            .controls_x = controls_x,
            .controls_y = controls_y,
            .controls_w = controls_w,
            .tabs_y = tabs_y,
            .tabs_h = tabs_h,
            .tabs_gap = tabs_gap,
            .tab_w = tab_w,
            .shuffle_y = shuffle_y,
            .shuffle_h = shuffle_h,
            .list_card_y = list_card_y,
            .list_card_h = list_card_h,
        };
    }

    static NowTextLayout make_now_text_layout(const UiLayout& layout) noexcept {
        const int title_h = 28;
        const int subtitle_h = 14;
        const int progress_h = 3;
        const int time_row_h = 16;
        const int text_gap = 6;
        const int header_top = (layout.top_bar_y + layout.top_bar_h + 12) + kCoverSize;
        const int text_col_y = header_top + 12;
        const int text_col_h = title_h + subtitle_h + progress_h + text_gap * 2;
        const int time_row_y = text_col_y + title_h + text_gap + subtitle_h + text_gap + progress_h + text_gap + 2;

        return NowTextLayout{
            .text_col_y = text_col_y,
            .text_col_h = text_col_h,
            .title_h = title_h,
            .subtitle_h = subtitle_h,
            .progress_h = progress_h,
            .time_row_h = time_row_h,
            .time_row_y = time_row_y,
            .text_gap = text_gap,
        };
    }

    static void apply_text_label_style(::ui::scene::SceneBuilder& builder, WidgetHandle h, const rgba& color,
                                       const Font& font) {
        StylePatch patch{};
        patch.has_font_color = true;
        patch.font_color = color;
        patch.has_font = true;
        patch.font = &font;
        patch.has_bg_color = true;
        patch.bg_color = {0, 0, 0, 0};
        patch.has_border_color = true;
        patch.border_color = {0, 0, 0, 0};
        patch.has_border_width = true;
        patch.border_width = 0;
        patch.has_padding = true;
        patch.padding = 0;
        patch.has_corner_radius = true;
        patch.corner_radius = 0;
        builder.set_style_override(h, patch);
    }

    static void apply_time_label_style(::ui::scene::SceneBuilder& builder, WidgetHandle h) {
        StylePatch patch{};
        patch.has_font_color = true;
        patch.font_color = kUiTime;
        patch.has_font = true;
        patch.font = &get_font_weighted(FontId::Small, FontWeight::Regular);
        builder.set_style_override(h, patch);
    }

    static void apply_info_tag_style(::ui::scene::SceneBuilder& builder, WidgetHandle h) {
        builder.set_style_class(h, static_cast<StyleClassId>(PlayerStyleClass::InfoTag));
    }

    static void apply_top_bar_button_style(::ui::scene::SceneBuilder& builder, WidgetHandle h) {
        builder.set_style_class(h, static_cast<StyleClassId>(PlayerStyleClass::TopBarButton));
    }

    static void apply_tab_base_style(::ui::scene::SceneBuilder& builder, WidgetHandle h, int radius) {
        builder.set_style_class(h, static_cast<StyleClassId>(PlayerStyleClass::TabBase));
        StylePatch patch{};
        patch.has_corner_radius = true;
        patch.corner_radius = radius;
        builder.set_style_patch(h, patch);
    }

    static void apply_tab_active_style(::ui::scene::SceneBuilder& builder, WidgetHandle h) {
        StylePatch patch{};
        patch.has_bg_color = true;
        patch.bg_color = kUiTabActive;
        patch.has_border_color = true;
        patch.border_color = kUiTabActive;
        patch.has_shadow_enabled = true;
        patch.shadow_enabled = true;
        patch.has_shadow_color = true;
        patch.shadow_color = kUiCardShadow;
        patch.has_shadow_offset_x = true;
        patch.shadow_offset_x = 0;
        patch.has_shadow_offset_y = true;
        patch.shadow_offset_y = 2;
        patch.has_shadow_spread = true;
        patch.shadow_spread = 2;
        patch.has_shadow_radius = true;
        patch.shadow_radius = 10;
        patch.has_inner_stroke_enabled = true;
        patch.inner_stroke_enabled = true;
        patch.has_inner_stroke_color = true;
        patch.inner_stroke_color = kUiButtonHover;
        patch.has_inner_stroke_width = true;
        patch.inner_stroke_width = 1;
        builder.set_style_override(h, patch);
    }

    static void apply_shuffle_shadow_style(::ui::scene::SceneBuilder& builder, WidgetHandle h) {
        builder.set_style_class(h, static_cast<StyleClassId>(PlayerStyleClass::ShuffleShadow));
    }

    static void apply_list_card_style(::ui::scene::SceneBuilder& builder, WidgetHandle h) {
        builder.set_style_class(h, static_cast<StyleClassId>(PlayerStyleClass::ListCard));
    }
    static void apply_control_side_style(::ui::scene::SceneBuilder& builder, WidgetHandle h,
                                         int btn_size, int icon_px) {
        builder.set_style_class(h, static_cast<StyleClassId>(PlayerStyleClass::ControlSide));
        StylePatch patch{};
        patch.has_padding = true;
        patch.padding = (btn_size - icon_px) / 2;
        builder.set_style_patch(h, patch);
    }

    static void apply_control_play_style(::ui::scene::SceneBuilder& builder, WidgetHandle h,
                                         int btn_size, int icon_px) {
        builder.set_style_class(h, static_cast<StyleClassId>(PlayerStyleClass::ControlPlay));
        StylePatch patch{};
        patch.has_padding = true;
        patch.padding = (btn_size - icon_px) / 2;
        builder.set_style_patch(h, patch);
    }

    static void apply_bottom_bar_style(::ui::scene::SceneBuilder& builder, WidgetHandle h) {
        builder.set_style_class(h, static_cast<StyleClassId>(PlayerStyleClass::BottomBar));
    }

    static void apply_bottom_button_style(::ui::scene::SceneBuilder& builder, WidgetHandle h,
                                          int btn_size, int icon_px) {
        builder.set_style_class(h, static_cast<StyleClassId>(PlayerStyleClass::BottomButton));
        StylePatch patch{};
        patch.has_padding = true;
        patch.padding = (btn_size - icon_px) / 2;
        builder.set_style_patch(h, patch);
    }

    static void apply_bottom_play_style(::ui::scene::SceneBuilder& builder, WidgetHandle h,
                                        int btn_size, int icon_px) {
        builder.set_style_class(h, static_cast<StyleClassId>(PlayerStyleClass::BottomPlay));
        StylePatch patch{};
        patch.has_padding = true;
        patch.padding = (btn_size - icon_px) / 2;
        builder.set_style_patch(h, patch);
    }

    static void apply_nav_label_style(::ui::scene::SceneBuilder& builder, WidgetHandle h) {
        StylePatch patch{};
        patch.has_font_color = true;
        patch.font_color = kUiTitle;
        patch.has_font = true;
        patch.font = &get_font_weighted(FontId::Small, FontWeight::Regular);
        patch.has_bg_color = true;
        patch.bg_color = {0, 0, 0, 0};
        patch.has_border_color = true;
        patch.border_color = {0, 0, 0, 0};
        patch.has_border_width = true;
        patch.border_width = 0;
        patch.has_padding = true;
        patch.padding = 0;
        patch.has_corner_radius = true;
        patch.corner_radius = 0;
        builder.set_style_override(h, patch);
        builder.set_hit_testable(h, false);
    }

    static void apply_nav_bar_style(::ui::scene::SceneBuilder& builder, WidgetHandle h, int radius) {
        StylePatch patch{};
        patch.has_bg_color = true;
        patch.bg_color = {0, 0, 0, 0};
        patch.has_border_color = true;
        patch.border_color = {0, 0, 0, 0};
        patch.has_border_width = true;
        patch.border_width = 0;
        patch.has_corner_radius = true;
        patch.corner_radius = radius;
        builder.set_style_override(h, patch);
    }

    static void apply_nav_hit_style(::ui::scene::SceneBuilder& builder, WidgetHandle h, int padding) {
        StylePatch patch{};
        patch.has_bg_color = true;
        patch.bg_color = {0, 0, 0, 0};
        patch.has_border_color = true;
        patch.border_color = {0, 0, 0, 0};
        patch.has_border_width = true;
        patch.border_width = 0;
        patch.has_padding = true;
        patch.padding = padding;
        patch.has_corner_radius = true;
        patch.corner_radius = 0;
        patch.has_font_color = true;
        patch.font_color = kUiTitle;
        patch.has_font = true;
        patch.font = &get_font_weighted(FontId::Small, FontWeight::Regular);
        builder.set_style_override(h, patch);
    }

    static WidgetHandle create_top_icon_button(::ui::scene::SceneBuilder& builder,
                                               const Rect& rect,
                                               ::ui::gfx::ImageId icon,
                                               int icon_px) {
        const WidgetHandle button = builder.create_button_static("");
        anchor_rect(builder, button, rect);
        builder.set_button_icon(button, icon);
        builder.set_button_icon_size(button, icon_px);
        apply_top_bar_button_style(builder, button);
        return button;
    }

    static WidgetHandle create_home_stat_card(::ui::scene::SceneBuilder& builder,
                                              WidgetHandle parent,
                                              const Rect& rect,
                                              std::string_view title,
                                              std::string_view body,
                                              std::string_view meta) {
        ::ui::scene::CardBuilder card{builder, rect, 0, 0,
                                      ::ui::scene::LayoutAlign::Start,
                                      static_cast<StyleClassId>(PlayerStyleClass::ListCard),
                                      true};
        const WidgetHandle root = card.root();

        const WidgetHandle title_label = builder.create_label_static(title.data());
        apply_text_label_style(builder, title_label, kUiTitle,
                               get_font_weighted(FontId::Normal, FontWeight::Bold));
        builder.set_label_align(title_label, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, title_label, {16, 12, rect.w - 120, 24});
        builder.link(root, title_label);

        const WidgetHandle body_label = builder.create_label_static(body.data());
        apply_text_label_style(builder, body_label, kUiSubtitle,
                               get_font_weighted(FontId::Small, FontWeight::Regular));
        builder.set_label_align(body_label, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, body_label, {16, 38, rect.w - 120, 18});
        builder.link(root, body_label);

        const WidgetHandle meta_chip = builder.create_label_static(meta.data());
        apply_time_label_style(builder, meta_chip);
        builder.set_label_align(meta_chip, ::ui::scene::TextAlignH::Center, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, meta_chip, {rect.w - 92, 22, 72, 24});
        apply_info_tag_style(builder, meta_chip);
        builder.link(root, meta_chip);

        builder.link(parent, root);
        return root;
    }

    static void build_bottom_chrome(::ui::scene::SceneBuilder& builder,
                                    UiHandles& h,
                                    const UiLayout& layout,
                                    const PlayerIconIds& icons) {
        const Rect bottom_bar_rect{kUiPadding, layout.bottom_bar_y,
                                   screen_width - kUiPadding * 2, layout.bottom_bar_h};
        const int bar_gap = 12;
        const int bar_pad = 12;
        const int play_size = 48;
        const int next_size = 36;
        ::ui::scene::RowBuilder bottom_row{builder, bottom_bar_rect, bar_gap, bar_pad,
                                           ::ui::scene::LayoutAlign::Center};
        h.bottom_bar = bottom_row.root();
        apply_bottom_bar_style(builder, h.bottom_bar);

        const Rect content = bottom_row.content_rect();
        const int tile_w = std::max(0, content.w - play_size - next_size - bar_gap * 2);
        const int tile_h = content.h;
        const Rect tile_rect = bottom_row.next_rect(tile_w, tile_h);
        ::ui::scene::TileBuilder bottom_tile{builder, tile_rect, 48, 8, false,
                                             kStyleClassInvalid, true, ""};
        h.bottom_cover = bottom_tile.image();
        h.bottom_title = bottom_tile.label();
        builder.set_label_align(h.bottom_title, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        bottom_row.add_at(bottom_tile.root(), tile_rect);

        h.bottom_subtitle = builder.create_label_static("");
        anchor_rect(builder, h.bottom_subtitle, {tile_rect.x + 56, tile_rect.y + 22,
                                                 tile_rect.w - 56, 18});
        builder.set_label_align(h.bottom_subtitle, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);

        h.bottom_hit = builder.create_button_static("");
        anchor_rect(builder, h.bottom_hit, {0, 0, bottom_bar_rect.w, bottom_bar_rect.h});
        {
            StylePatch patch{};
            patch.has_bg_color = true;
            patch.bg_color = {0, 0, 0, 0};
            patch.has_border_color = true;
            patch.border_color = {0, 0, 0, 0};
            builder.set_style_override(h.bottom_hit, patch);
        }
        builder.set_hit_testable(h.bottom_hit, true);
        builder.link(h.bottom_bar, h.bottom_hit);

        h.bottom_play = builder.create_button_static("");
        bottom_row.add(h.bottom_play, play_size, play_size);
        builder.set_button_icon(h.bottom_play, icons.play);
        builder.set_button_icon_size(h.bottom_play, 18);
        apply_bottom_play_style(builder, h.bottom_play, play_size, 18);
        builder.link(h.bottom_bar, h.bottom_subtitle);

        h.bottom_next = builder.create_button_static("");
        bottom_row.add(h.bottom_next, next_size, next_size);
        builder.set_button_icon(h.bottom_next, icons.next);
        builder.set_button_icon_size(h.bottom_next, 16);
        apply_bottom_button_style(builder, h.bottom_next, next_size, 16);

        const Rect nav_rect{kUiPadding, layout.nav_y,
                            screen_width - kUiPadding * 2, layout.nav_h};
        h.nav_bar = builder.create_container();
        anchor_rect(builder, h.nav_bar, nav_rect);
        apply_nav_bar_style(builder, h.nav_bar, nav_rect.h / 2);
        const int nav_btn_w = (nav_rect.w - layout.nav_gap * 2) / 3;
        const int nav_icon = 22;
        const int nav_icon_pad = 12;

        h.nav_home = builder.create_button_static("Home");
        h.nav_search = builder.create_button_static("Search");
        h.nav_library = builder.create_button_static("Library");
        anchor_rect(builder, h.nav_home, {0, 0, nav_btn_w, layout.nav_h});
        anchor_rect(builder, h.nav_search, {nav_btn_w + layout.nav_gap, 0, nav_btn_w, layout.nav_h});
        anchor_rect(builder, h.nav_library, {(nav_btn_w + layout.nav_gap) * 2, 0, nav_btn_w, layout.nav_h});
        apply_nav_hit_style(builder, h.nav_home, nav_icon_pad);
        apply_nav_hit_style(builder, h.nav_search, nav_icon_pad);
        apply_nav_hit_style(builder, h.nav_library, nav_icon_pad);

        builder.set_button_icon(h.nav_home, icons.home);
        builder.set_button_icon_size(h.nav_home, nav_icon);
        builder.set_button_icon(h.nav_search, icons.search);
        builder.set_button_icon_size(h.nav_search, nav_icon);
        builder.set_button_icon(h.nav_library, icons.folder);
        builder.set_button_icon_size(h.nav_library, nav_icon);

        builder.link(h.nav_bar, h.nav_home);
        builder.link(h.nav_bar, h.nav_search);
        builder.link(h.nav_bar, h.nav_library);

        builder.link(h.root, h.bottom_bar);
        builder.set_hit_testable(h.bottom_bar, true);
        builder.link(h.root, h.nav_bar);
    }
