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

    struct HomeLayout {
        int scroll_bottom_gap{};
        int scroll_step{};
        int scroll_extra{};
        int title_x{};
        int title_y{};
        int title_w{};
        int title_line_h{};
        int title_line_gap{};
        int subtitle_y{};
        int subtitle_h{};
        int play_size{};
        int play_icon_px{};
        int play_y{};
        int collage_x{};
        int collage_y{};
        int collage_big{};
        int collage_small_left{};
        int collage_small_right{};
        int collage_small_center{};
        int stat_card_x{};
        int stat_card_y{};
        int stat_card_w{};
        int stat_card_h{};
        int stat_card_gap{};
        int stat_probe_card_h{};
        int toolbar_button_size{};
        int toolbar_icon_px{};
        int toolbar_button_gap{};
    };

    struct NowPlayingLayout {
        int text_padding{};
        int controls_padding{};
        int time_padding{};
        int progress_section_pad_x{};
        int progress_section_pad_y{};
        int progress_section_gap{};
        int progress_section_radius{};
        int progress_section_bg_alpha{};
        int progress_section_border_alpha{};
        int progress_visual_h{};
        int time_row_radius{};
        int time_row_bg_alpha{};
        int time_row_border_alpha{};
        int time_label_inner_pad{};
        int info_tag_radius{};
        int cover_corner_radius{};
        int collage_small{};
        int collage_tiny{};
        int collage_left_dx{};
        int collage_left_dy{};
        int collage_right_dx{};
        int collage_right_dy{};
        int collage_corner_radius{};
        int collage_shadow_alpha{};
        int collage_shadow_offset_y{};
        int collage_shadow_spread{};
        int collage_shadow_radius{};
        int control_side{};
        int control_play{};
        int control_gap{};
        int control_side_icon{};
        int control_play_icon{};
        int time_side_w{};
        int time_min_info_w{};
        int time_inner_gap{};
        int top_button_size{};
        int top_back_icon{};
        int top_more_icon{};
        int top_title_x{};
        int top_title_y{};
        int top_title_h{};
        int top_title_side_gap{};
        int lyrics_button_w{};
        int cover_debug_gap{};
        int cover_debug_h{};
    };

    struct LibraryLayout {
        int tabs_padding{};
        int shuffle_padding{};
        int nav_padding{};
        int top_title_x{};
        int top_title_y{};
        int top_title_h{};
        int top_title_right_gap{};
        int top_button_size{};
        int top_icon_px{};
        int top_button_right_gap{};
        int shuffle_w{};
        int list_row_h{};
        int list_header_h{};
        int list_title_h{};
        int list_path_h{};
        int list_title_top_gap{};
        int list_title_left_gap{};
        int list_title_right_gap{};
        int list_sort_w{};
        int list_sort_h{};
        int list_sort_top_offset{};
        int list_sort_right_gap{};
        int list_path_top_gap{};
        int list_path_side_gap{};
        int list_path_bg_h_extra{};
        int list_path_bg_radius{};
        int list_path_icon_left_gap{};
        int list_path_icon_top_gap{};
        int list_path_icon_size{};
        int list_path_icon_px{};
        int list_path_text_left{};
        int list_path_text_top{};
        int list_path_text_right_gap{};
        int list_sort_radius{};
        int list_path_icon_radius{};
        int list_body_top_gap{};
        int list_body_bottom_gap{};
        int list_body_side_gap{};
        int list_scroll_w{};
        int list_scroll_right_gap{};
        int list_hint_left_gap{};
        int list_hint_bottom_gap{};
        int list_hint_h{};
        int list_hint_right_gap{};
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

        const int tabs_y = top_bar_y + top_bar_h + 14;
        const int tabs_h = 38;
        const int tabs_gap = 12;
        const int tabs_w = screen_width - kUiPadding * 2;
        const int tab_w = (tabs_w - tabs_gap * 2) / 3;

        const int shuffle_y = tabs_y + tabs_h + 14;
        const int shuffle_h = 36;
        const int list_card_y = shuffle_y + shuffle_h + 10;
        const int list_card_h = std::max(240, bottom_bar_y - list_card_y - 22);

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
        const int progress_h = 12;
        const int time_row_h = 24;
        const int text_gap = 6;
        const int header_top = (layout.top_bar_y + layout.top_bar_h + 12) + kCoverSize;
        const int text_col_y = header_top + 12;
        const int text_col_h = title_h + subtitle_h + text_gap;
        const int time_row_y = text_col_y + text_col_h + 44;

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

    static HomeLayout make_home_layout(const UiLayout& layout) noexcept {
        const int title_y = layout.top_bar_y + 8;
        const int collage_big = 286;
        return HomeLayout{
            .scroll_bottom_gap = 8,
            .scroll_step = 32,
            .scroll_extra = 560,
            .title_x = kUiPadding,
            .title_y = title_y,
            .title_w = screen_width - kUiPadding * 2 - 96,
            .title_line_h = 102,
            .title_line_gap = 86,
            .subtitle_y = title_y + 194,
            .subtitle_h = 30,
            .play_size = 78,
            .play_icon_px = 28,
            .play_y = title_y + 78,
            .collage_x = (screen_width - collage_big) / 2,
            .collage_y = title_y + 230,
            .collage_big = collage_big,
            .collage_small_left = 88,
            .collage_small_right = 76,
            .collage_small_center = 70,
            .stat_card_x = kUiPadding,
            .stat_card_y = title_y + 230 + collage_big + 118,
            .stat_card_w = screen_width - kUiPadding * 2,
            .stat_card_h = 76,
            .stat_card_gap = 14,
            .stat_probe_card_h = 248,
            .toolbar_button_size = 40,
            .toolbar_icon_px = 14,
            .toolbar_button_gap = 48,
        };
    }

    static NowPlayingLayout make_now_playing_layout() noexcept {
        return NowPlayingLayout{
            .text_padding = 0,
            .controls_padding = 0,
            .time_padding = 0,
            .progress_section_pad_x = 14,
            .progress_section_pad_y = 12,
            .progress_section_gap = 8,
            .progress_section_radius = 22,
            .progress_section_bg_alpha = 76,
            .progress_section_border_alpha = 100,
            .progress_visual_h = 16,
            .time_row_radius = 18,
            .time_row_bg_alpha = 52,
            .time_row_border_alpha = 84,
            .time_label_inner_pad = 6,
            .info_tag_radius = 14,
            .cover_corner_radius = 28,
            .collage_small = 64,
            .collage_tiny = 52,
            .collage_left_dx = -18,
            .collage_left_dy = -8,
            .collage_right_dx = 18,
            .collage_right_dy = 28,
            .collage_corner_radius = 12,
            .collage_shadow_alpha = 80,
            .collage_shadow_offset_y = 2,
            .collage_shadow_spread = 2,
            .collage_shadow_radius = 10,
            .control_side = 96,
            .control_play = 116,
            .control_gap = 14,
            .control_side_icon = 24,
            .control_play_icon = 30,
            .time_side_w = 52,
            .time_min_info_w = 170,
            .time_inner_gap = 16,
            .top_button_size = 40,
            .top_back_icon = 20,
            .top_more_icon = 14,
            .top_title_x = 48,
            .top_title_y = 8,
            .top_title_h = 24,
            .top_title_side_gap = 96,
            .lyrics_button_w = 40,
            .cover_debug_gap = 4,
            .cover_debug_h = 12,
        };
    }

    static LibraryLayout make_library_layout() noexcept {
        return LibraryLayout{
            .tabs_padding = 0,
            .shuffle_padding = 0,
            .nav_padding = 0,
            .top_title_x = 8,
            .top_title_y = 8,
            .top_title_h = 24,
            .top_title_right_gap = 56,
            .top_button_size = 40,
            .top_icon_px = 14,
            .top_button_right_gap = 44,
            .shuffle_w = 120,
            .list_row_h = 60,
            .list_header_h = 82,
            .list_title_h = 18,
            .list_path_h = 16,
            .list_title_top_gap = 12,
            .list_title_left_gap = 14,
            .list_title_right_gap = 112,
            .list_sort_w = 76,
            .list_sort_h = 28,
            .list_sort_top_offset = -6,
            .list_sort_right_gap = 88,
            .list_path_top_gap = 8,
            .list_path_side_gap = 14,
            .list_path_bg_h_extra = 8,
            .list_path_bg_radius = 12,
            .list_path_icon_left_gap = 8,
            .list_path_icon_top_gap = 2,
            .list_path_icon_size = 24,
            .list_path_icon_px = 12,
            .list_path_text_left = 30,
            .list_path_text_top = 4,
            .list_path_text_right_gap = 36,
            .list_sort_radius = 14,
            .list_path_icon_radius = 12,
            .list_body_top_gap = 20,
            .list_body_bottom_gap = 18,
            .list_body_side_gap = 10,
            .list_scroll_w = 10,
            .list_scroll_right_gap = 12,
            .list_hint_left_gap = 14,
            .list_hint_bottom_gap = 22,
            .list_hint_h = 18,
            .list_hint_right_gap = 24,
        };
    }

    enum class PlayerTextRole : std::uint8_t {
        HeroTitle,
        HeroSubtitle,
        PageTitle,
        PageSubtitle,
        CardTitle,
        CardBody,
        MetaText,
    };

    static StylePatch make_player_text_patch(PlayerTextRole role, const rgba& color) {
        ::ui::scene::TextStyleSpec spec{};
        spec.color = color;
        switch (role) {
        case PlayerTextRole::HeroTitle:
            spec.explicit_font = &get_player_font_px(72, FontWeight::Bold);
            spec.emphasis = ::ui::scene::TextEmphasis::Emphasis;
            break;
        case PlayerTextRole::HeroSubtitle:
            spec.font_role = FontId::Normal;
            spec.font_weight = FontWeight::Regular;
            break;
        case PlayerTextRole::PageTitle:
            spec.font_role = FontId::Normal;
            spec.font_weight = FontWeight::Bold;
            spec.emphasis = ::ui::scene::TextEmphasis::Emphasis;
            break;
        case PlayerTextRole::PageSubtitle:
            spec.font_role = FontId::Small;
            spec.font_weight = FontWeight::Regular;
            break;
        case PlayerTextRole::CardTitle:
            spec.font_role = FontId::Normal;
            spec.font_weight = FontWeight::Bold;
            spec.emphasis = ::ui::scene::TextEmphasis::Emphasis;
            break;
        case PlayerTextRole::CardBody:
            spec.font_role = FontId::Small;
            spec.font_weight = FontWeight::Regular;
            break;
        case PlayerTextRole::MetaText:
            spec.font_role = FontId::Small;
            spec.font_weight = FontWeight::Regular;
            spec.emphasis = ::ui::scene::TextEmphasis::Meta;
            break;
        }
        return ::ui::scene::make_text_style_patch(spec);
    }

    static void apply_player_text_style(::ui::scene::SceneBuilder& builder,
                                        WidgetHandle h,
                                        PlayerTextRole role,
                                        const rgba& color) {
        builder.set_style_override(h, make_player_text_patch(role, color));
    }

    static void apply_text_label_style(::ui::scene::SceneBuilder& builder, WidgetHandle h, const rgba& color,
                                       const Font& font) {
        StylePatch patch = make_player_text_patch(PlayerTextRole::CardBody, color);
        patch.has_font = true;
        patch.font = &font;
        builder.set_style_override(h, patch);
    }

    static void apply_text_label_role_style(::ui::scene::SceneBuilder& builder,
                                            WidgetHandle h,
                                            const rgba& color,
                                            FontId role,
                                            FontWeight weight = FontWeight::Regular) {
        StylePatch patch = make_player_text_patch(PlayerTextRole::CardBody, color);
        patch.has_font_role = true;
        patch.font_role = role;
        patch.has_font_weight = true;
        patch.font_weight = weight;
        builder.set_style_override(h, patch);
    }

    static void apply_time_label_style(::ui::scene::SceneBuilder& builder, WidgetHandle h) {
        apply_player_text_style(builder, h, PlayerTextRole::MetaText, kUiTime);
    }

    static void apply_now_time_row_style(::ui::scene::SceneBuilder& builder,
                                         WidgetHandle h,
                                         const NowPlayingLayout& layout) {
        StylePatch patch = ::ui::scene::make_pill_surface_patch({
            .bg_color = {kUiProgressBg.r, kUiProgressBg.g, kUiProgressBg.b,
                         static_cast<std::uint8_t>(layout.time_row_bg_alpha)},
            .border_color = {kUiInfoTagBorderSoft.r, kUiInfoTagBorderSoft.g, kUiInfoTagBorderSoft.b,
                             static_cast<std::uint8_t>(layout.time_row_border_alpha)},
            .corner_radius = layout.time_row_radius,
        });
        builder.set_style_override(h, patch);
    }

    static void apply_now_progress_section_style(::ui::scene::SceneBuilder& builder,
                                                 WidgetHandle h,
                                                 const NowPlayingLayout& layout) {
        StylePatch patch = ::ui::scene::make_pill_surface_patch({
            .bg_color = {kUiInfoTagBg.r, kUiInfoTagBg.g, kUiInfoTagBg.b,
                         static_cast<std::uint8_t>(layout.progress_section_bg_alpha)},
            .border_color = {kUiInfoTagBorderSoft.r, kUiInfoTagBorderSoft.g, kUiInfoTagBorderSoft.b,
                             static_cast<std::uint8_t>(layout.progress_section_border_alpha)},
            .corner_radius = layout.progress_section_radius,
        });
        builder.set_style_override(h, patch);
    }

    static void apply_now_progress_visual_style(::ui::scene::SceneBuilder& builder, WidgetHandle h) {
        StylePatch patch = ::ui::scene::make_seek_bar_style_patch({
            .bg_color = kUiProgressBg,
            .track_color = kUiProgressBorder,
            .fill_color = kUiSwitchOn,
            .padding = 2,
            .corner_radius = 6,
        });
        builder.set_style_override(h, patch);
    }

    static void apply_now_time_side_style(::ui::scene::SceneBuilder& builder,
                                          WidgetHandle h,
                                          const NowPlayingLayout& layout,
                                          bool align_right = false) {
        StylePatch patch = make_player_text_patch(PlayerTextRole::MetaText, kUiTime);
        patch.has_padding = true;
        patch.padding = layout.time_label_inner_pad;
        builder.set_style_override(h, patch);
        builder.set_label_align(h,
                                align_right ? ::ui::scene::TextAlignH::Right : ::ui::scene::TextAlignH::Left,
                                ::ui::scene::TextAlignV::Center);
    }

    static void apply_info_tag_style(::ui::scene::SceneBuilder& builder, WidgetHandle h) {
        builder.set_style_class(h, static_cast<StyleClassId>(PlayerStyleClass::InfoTag));
    }

    static void apply_now_info_tag_style(::ui::scene::SceneBuilder& builder,
                                         WidgetHandle h,
                                         const NowPlayingLayout& layout) {
        StylePatch patch = ::ui::scene::make_pill_surface_patch({
            .bg_color = {kUiInfoTagBg.r, kUiInfoTagBg.g, kUiInfoTagBg.b, 255},
            .border_color = {kUiSwitchOn.r, kUiSwitchOn.g, kUiSwitchOn.b, 108},
            .corner_radius = layout.info_tag_radius,
        });
        patch.has_font_color = true;
        patch.font_color = kUiTitle;
        patch.has_font_role = true;
        patch.font_role = FontId::Small;
        builder.set_style_override(h, patch);
    }

    static void apply_home_cover_style(::ui::scene::SceneBuilder& builder,
                                       WidgetHandle h,
                                       int size,
                                       bool with_shadow) {
        StylePatch patch{};
        patch.has_corner_radius = true;
        patch.corner_radius = size / 2;
        if (with_shadow) {
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
        }
        builder.set_style_override(h, patch);
    }

    static void apply_now_cover_style(::ui::scene::SceneBuilder& builder,
                                      WidgetHandle h,
                                      int corner_radius,
                                      bool with_shadow,
                                      std::uint8_t shadow_alpha,
                                      int shadow_offset_y,
                                      int shadow_spread,
                                      int shadow_radius) {
        StylePatch patch{};
        patch.has_corner_radius = true;
        patch.corner_radius = corner_radius;
        if (with_shadow) {
            patch.has_shadow_enabled = true;
            patch.shadow_enabled = true;
            patch.has_shadow_color = true;
            patch.shadow_color = {0, 0, 0, shadow_alpha};
            patch.has_shadow_offset_x = true;
            patch.shadow_offset_x = 0;
            patch.has_shadow_offset_y = true;
            patch.shadow_offset_y = shadow_offset_y;
            patch.has_shadow_spread = true;
            patch.shadow_spread = shadow_spread;
            patch.has_shadow_radius = true;
            patch.shadow_radius = shadow_radius;
        }
        builder.set_style_override(h, patch);
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
        StylePatch patch = ::ui::scene::make_pill_surface_patch({
            .bg_color = kUiTabActive,
            .border_color = kUiTabActive,
            .corner_radius = 0,
            .shadow = {
                .enabled = true,
                .color = kUiCardShadow,
                .offset_x = 0,
                .offset_y = 2,
                .spread = 2,
                .radius = 10,
            },
            .inner_stroke_enabled = true,
            .inner_stroke_color = kUiButtonHover,
            .inner_stroke_width = 1,
        });
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
        apply_player_text_style(builder, h, PlayerTextRole::MetaText, kUiTitle);
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
        StylePatch patch = make_player_text_patch(PlayerTextRole::MetaText, kUiTitle);
        patch.has_padding = true;
        patch.padding = padding;
        builder.set_style_override(h, patch);
    }

    static void apply_nav_indicator_style(::ui::scene::SceneBuilder& builder, WidgetHandle h) {
        StylePatch patch{};
        patch.has_bg_color = true;
        patch.bg_color = {0, 0, 0, 0};
        patch.has_border_color = true;
        patch.border_color = {0, 0, 0, 0};
        patch.has_corner_radius = true;
        patch.corner_radius = 4;
        builder.set_style_patch(h, patch);
        builder.set_hit_testable(h, false);
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
        apply_player_text_style(builder, title_label, PlayerTextRole::CardTitle, kUiTitle);
        builder.set_label_align(title_label, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, title_label, {16, 12, rect.w - 120, 24});
        builder.link(root, title_label);

        const WidgetHandle body_label = builder.create_label_static(body.data());
        apply_player_text_style(builder, body_label, PlayerTextRole::CardBody, kUiSubtitle);
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

    static WidgetHandle create_home_stats_probe_card(::ui::scene::SceneBuilder& builder,
                                                     UiHandles& h,
                                                     WidgetHandle parent,
                                                     const Rect& rect,
                                                     const PlayerIconIds& icons) {
        ::ui::scene::CardBuilder card{builder, rect, 0, 0,
                                      ::ui::scene::LayoutAlign::Start,
                                      static_cast<StyleClassId>(PlayerStyleClass::ListCard),
                                      true};
        const WidgetHandle root = card.root();

        {
            StylePatch patch{};
            patch.has_bg_color = true;
            patch.bg_color = {52, 60, 88, 228};
            patch.has_border_color = true;
            patch.border_color = {0, 0, 0, 0};
            patch.has_border_width = true;
            patch.border_width = 0;
            patch.has_corner_radius = true;
            patch.corner_radius = 16;
            builder.set_style_override(root, patch);
        }

        {
            const WidgetHandle header_band = builder.create_container();
            anchor_rect(builder, header_band, {0, 0, rect.w, 56});
            StylePatch patch{};
            patch.has_bg_color = true;
            patch.bg_color = {38, 46, 68, 222};
            patch.has_border_color = true;
            patch.border_color = {0, 0, 0, 0};
            patch.has_border_width = true;
            patch.border_width = 0;
            patch.has_corner_radius = true;
            patch.corner_radius = 18;
            builder.set_style_override(header_band, patch);
            builder.set_hit_testable(header_band, false);
            builder.link(root, header_band);
        }

        const WidgetHandle title_label = builder.create_label_static("Listening stats");
        apply_player_text_style(builder, title_label, PlayerTextRole::CardTitle, kUiTitle);
        builder.set_label_align(title_label, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, title_label, {18, 10, rect.w - 110, 24});
        builder.link(root, title_label);

        const WidgetHandle subtitle_label = builder.create_label_static("This Week");
        apply_player_text_style(builder, subtitle_label, PlayerTextRole::MetaText, kUiSubtitle);
        builder.set_label_align(subtitle_label, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, subtitle_label, {18, 31, rect.w - 110, 16});
        builder.link(root, subtitle_label);

        const WidgetHandle action_button = builder.create_button_static("");
        anchor_rect(builder, action_button, {rect.w - 58, 9, 40, 40});
        builder.set_button_icon(action_button, icons.next);
        builder.set_button_icon_size(action_button, 14);
        apply_top_bar_button_style(builder, action_button);
        builder.set_hit_testable(action_button, false);
        builder.link(root, action_button);

        const WidgetHandle value_label = builder.create_label_static("0 s");
        apply_text_label_style(builder, value_label, kUiTitle, get_player_font_px(34, FontWeight::Bold));
        builder.set_label_align(value_label, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, value_label, {18, 70, rect.w - 36, 36});
        builder.link(root, value_label);
        h.home_stats_total = value_label;

        const int metrics_y = 118;
        const int metrics_w = (rect.w - 18 * 2 - 14) / 2;

        const WidgetHandle plays_label = builder.create_label_static("Total plays");
        apply_player_text_style(builder, plays_label, PlayerTextRole::MetaText, kUiSubtitle);
        builder.set_label_align(plays_label, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, plays_label, {18, metrics_y, metrics_w, 16});
        builder.link(root, plays_label);

        const WidgetHandle plays_value = builder.create_label_static("38");
        apply_text_label_style(builder, plays_value, kUiTitle, get_player_font_px(22, FontWeight::Medium));
        builder.set_label_align(plays_value, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, plays_value, {18, metrics_y + 18, metrics_w, 24});
        builder.link(root, plays_value);
        h.home_stats_plays = plays_value;

        const int right_x = 18 + metrics_w + 14;
        const WidgetHandle avg_label = builder.create_label_static("Avg per day");
        apply_player_text_style(builder, avg_label, PlayerTextRole::MetaText, kUiSubtitle);
        builder.set_label_align(avg_label, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, avg_label, {right_x, metrics_y, metrics_w, 16});
        builder.link(root, avg_label);

        const WidgetHandle avg_value = builder.create_label_static("43m");
        apply_text_label_style(builder, avg_value, kUiTitle, get_player_font_px(22, FontWeight::Medium));
        builder.set_label_align(avg_value, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, avg_value, {right_x, metrics_y + 18, metrics_w, 24});
        builder.link(root, avg_value);
        h.home_stats_avg = avg_value;

        constexpr std::array<const char*, 7> kDays{"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
        constexpr std::array<int, 7> kBarHeights{20, 35, 28, 48, 58, 38, 23};
        const int chart_x = 14;
        const int chart_y = 170;
        const int chart_w = rect.w - 28;
        const int chart_h = 54;
        const int label_y = chart_y + chart_h + 5;
        const int cell_w = chart_w / 7;
        const int bar_w = 22;
        for (int i = 0; i < 7; ++i) {
            const int x = chart_x + cell_w * i + (cell_w - bar_w) / 2;

            const int fill_h = kBarHeights[static_cast<std::size_t>(i)];
            const WidgetHandle fill = builder.create_container();
            anchor_rect(builder, fill, {x, chart_y + chart_h - fill_h, bar_w, fill_h});
            {
                StylePatch patch{};
                patch.has_bg_color = true;
                patch.bg_color = (i == 4)
                    ? rgba{kUiSwitchOn.r, kUiSwitchOn.g, kUiSwitchOn.b, 255}
                    : rgba{kUiSwitchOn.r, kUiSwitchOn.g, kUiSwitchOn.b, 210};
                patch.has_border_color = true;
                patch.border_color = {0, 0, 0, 0};
                patch.has_border_width = true;
                patch.border_width = 0;
                patch.has_corner_radius = true;
                patch.corner_radius = bar_w / 2;
                builder.set_style_override(fill, patch);
            }
            builder.set_hit_testable(fill, false);
            builder.link(root, fill);
            h.home_stats_bars[static_cast<std::size_t>(i)] = fill;
            h.home_stats_bar_slots[static_cast<std::size_t>(i)] = {x, chart_y, bar_w, chart_h};

            const WidgetHandle day_label = builder.create_label_static(kDays[static_cast<std::size_t>(i)]);
            apply_player_text_style(builder, day_label, PlayerTextRole::MetaText, kUiTime);
            builder.set_label_align(day_label, ::ui::scene::TextAlignH::Center, ::ui::scene::TextAlignV::Center);
            anchor_rect(builder, day_label, {chart_x + cell_w * i, label_y, cell_w, 14});
            builder.link(root, day_label);
        }

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
        const int indicator_w = 36;
        const int indicator_h = 4;
        const int indicator_y = layout.nav_h - 8;

        h.nav_home = builder.create_button_static("Home");
        h.nav_search = builder.create_button_static("Search");
        h.nav_library = builder.create_button_static("Library");
        h.nav_home_indicator = builder.create_container();
        h.nav_search_indicator = builder.create_container();
        h.nav_library_indicator = builder.create_container();
        anchor_rect(builder, h.nav_home, {0, 0, nav_btn_w, layout.nav_h});
        anchor_rect(builder, h.nav_search, {nav_btn_w + layout.nav_gap, 0, nav_btn_w, layout.nav_h});
        anchor_rect(builder, h.nav_library, {(nav_btn_w + layout.nav_gap) * 2, 0, nav_btn_w, layout.nav_h});
        anchor_rect(builder, h.nav_home_indicator, {(nav_btn_w - indicator_w) / 2, indicator_y, indicator_w, indicator_h});
        anchor_rect(builder, h.nav_search_indicator, {nav_btn_w + layout.nav_gap + (nav_btn_w - indicator_w) / 2, indicator_y, indicator_w, indicator_h});
        anchor_rect(builder, h.nav_library_indicator, {(nav_btn_w + layout.nav_gap) * 2 + (nav_btn_w - indicator_w) / 2, indicator_y, indicator_w, indicator_h});
        apply_nav_hit_style(builder, h.nav_home, nav_icon_pad);
        apply_nav_hit_style(builder, h.nav_search, nav_icon_pad);
        apply_nav_hit_style(builder, h.nav_library, nav_icon_pad);
        apply_nav_indicator_style(builder, h.nav_home_indicator);
        apply_nav_indicator_style(builder, h.nav_search_indicator);
        apply_nav_indicator_style(builder, h.nav_library_indicator);

        builder.set_button_icon(h.nav_home, icons.home);
        builder.set_button_icon_size(h.nav_home, nav_icon);
        builder.set_button_icon(h.nav_search, icons.search);
        builder.set_button_icon_size(h.nav_search, nav_icon);
        builder.set_button_icon(h.nav_library, icons.folder);
        builder.set_button_icon_size(h.nav_library, nav_icon);

        builder.link(h.nav_bar, h.nav_home);
        builder.link(h.nav_bar, h.nav_search);
        builder.link(h.nav_bar, h.nav_library);
        builder.link(h.nav_bar, h.nav_home_indicator);
        builder.link(h.nav_bar, h.nav_search_indicator);
        builder.link(h.nav_bar, h.nav_library_indicator);

        builder.link(h.root, h.bottom_bar);
        builder.set_hit_testable(h.bottom_bar, true);
        builder.link(h.root, h.nav_bar);
    }
