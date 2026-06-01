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

    static void anchor_rect(::ui::scene::SceneBuilder& builder, WidgetHandle h, const Rect& r) noexcept {
        builder.set_rect(h, r);
    }

    enum class PlayerDrawScopeId : std::uint16_t {
        Default = 0,
        HomeHero = 1,
        HomeCards = 2,
        HomeCollage = 3,
        NowCover = 4,
        NowText = 5,
        NowProgress = 6,
        NowControls = 7,
        NowTransition = 8,
        LibraryChrome = 9,
        LibraryList = 10,
        LibraryOverlay = 11,
        BottomBar = 12,
    };

    static void mark_draw_scope(::ui::scene::SceneBuilder& builder,
                                WidgetHandle h,
                                PlayerDrawScopeId id) noexcept {
        builder.set_draw_scope(h, ::ui::scene::DrawScope{static_cast<std::uint16_t>(id)});
    }

    static WidgetHandle create_label_text(::ui::scene::SceneBuilder& builder,
                                          std::string_view text) {
        char buffer[192]{};
        std::size_t len = text.size();
        if (len >= sizeof(buffer)) {
            len = sizeof(buffer) - 1U;
        }
        for (std::size_t i = 0; i < len; ++i) {
            buffer[i] = text[i];
        }
        buffer[len] = '\0';
        return builder.create_label(buffer);
    }

    static UiLayout make_layout() noexcept {
        const int top_bar_h = 44;
        const int top_bar_y = kUiPadding;
        const int top_bar_x = kUiPadding;
        const int top_bar_w = screen_width - kUiPadding * 2;

        const int nav_h = 72;
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

        const int tabs_y = top_bar_y + top_bar_h + 18;
        const int tabs_h = 40;
        const int tabs_gap = 10;
        const int tabs_w = screen_width - kUiPadding * 2;
        const int tab_w = (tabs_w - tabs_gap * 2) / 3;

        const int shuffle_y = tabs_y + tabs_h + 12;
        const int shuffle_h = 34;
        const int list_card_y = shuffle_y + shuffle_h + 12;
        const int list_card_h = std::max(240, bottom_bar_y - list_card_y - 20);

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

#include "player.ui_builder.home_layout.inc"
#include "player.ui_builder.now_playing_layout.inc"
#include "player.ui_builder.library_layout.inc"

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
            spec.explicit_font = &get_player_font_px_variant(
                64, FontWeight::Regular, "wght636_wdth152_ROND50_XTRA520_YOPQ90_YTLC505");
            spec.emphasis = ::ui::scene::TextEmphasis::Emphasis;
            break;
        case PlayerTextRole::HeroSubtitle:
            spec.explicit_font = &get_player_font_px_variant(14, FontWeight::Regular, "ROND100");
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

    static void apply_home_hero_title_style(::ui::scene::SceneBuilder& builder,
                                            WidgetHandle h,
                                            const rgba& color) {
        StylePatch patch = make_player_text_patch(PlayerTextRole::HeroTitle, color);
        patch.has_font = true;
        patch.font = &get_player_font_px_variant(
            64, FontWeight::Regular, "wght636_wdth152_ROND50_XTRA520_YOPQ90_YTLC505");
        builder.set_style_override(h, patch);
    }

    static void apply_library_hero_title_style(::ui::scene::SceneBuilder& builder,
                                               WidgetHandle h,
                                               const rgba& color) {
        StylePatch patch = make_player_text_patch(PlayerTextRole::HeroTitle, color);
        patch.has_font = true;
        patch.font = &get_player_font_px(60, FontWeight::Bold);
        patch.has_font_color = true;
        patch.font_color = color;
        patch.has_font_color_disabled = true;
        patch.font_color_disabled = color;
        builder.set_style_override(h, patch);
    }

    static void apply_home_mix_title_style(::ui::scene::SceneBuilder& builder,
                                           WidgetHandle h,
                                           const rgba& color) {
        StylePatch patch = make_player_text_patch(PlayerTextRole::CardTitle, color);
        patch.has_font = true;
        patch.font = &get_player_font_px_variant(
            20, FontWeight::Regular, "wght630_wdth136_GRAD40_ROND100_XTRA520_YOPQ90_YTLC505");
        builder.set_style_override(h, patch);
    }

    static void apply_home_mix_subtitle_style(::ui::scene::SceneBuilder& builder,
                                              WidgetHandle h,
                                              const rgba& color) {
        StylePatch patch = make_player_text_patch(PlayerTextRole::MetaText, color);
        patch.has_font = true;
        patch.font = &get_player_font_px_variant(14, FontWeight::Regular, "ROND100");
        builder.set_style_override(h, patch);
    }

    static void apply_home_daily_mix_preview_plate_style(::ui::scene::SceneBuilder& builder,
                                                         WidgetHandle h,
                                                         int corner_radius) {
        StylePatch patch = ::ui::scene::make_clean_surface_patch({
            .apply_bg_color = true,
            .apply_border_color = true,
            .apply_border_width = true,
            .apply_corner_radius = true,
            .bg_color = rgba{kUiTitle.r, kUiTitle.g, kUiTitle.b, 18},
            .border_color = rgba{kUiTitle.r, kUiTitle.g, kUiTitle.b, 176},
            .border_width = 2,
            .corner_radius = corner_radius,
        });
        builder.set_style_override(h, patch);
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
            .corner_radius = layout.progress_section_radius + 4,
        });
        builder.set_style_override(h, patch);
    }

    static void apply_now_progress_visual_style(::ui::scene::SceneBuilder& builder, WidgetHandle h) {
        StylePatch patch = ::ui::scene::make_seek_bar_style_patch({
            .bg_color = kUiProgressBg,
            .track_color = kUiProgressBorder,
            .fill_color = kUiSwitchOn,
            .padding = 3,
            .corner_radius = 9,
        });
        builder.set_style_override(h, patch);
    }

    static void apply_now_time_side_style(::ui::scene::SceneBuilder& builder,
                                          WidgetHandle h,
                                          const NowPlayingLayout& layout,
                                          bool align_right = false) {
        StylePatch patch = make_player_text_patch(PlayerTextRole::MetaText, kUiTime);
        patch.has_font = true;
        patch.font = &get_player_font_px(18, FontWeight::Medium);
        patch.has_padding = true;
        patch.padding = layout.time_label_inner_pad + 2;
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
        StylePatch patch = ::ui::scene::make_clean_surface_patch({
            .apply_bg_color = true,
            .apply_border_color = true,
            .apply_corner_radius = true,
            .bg_color = {kUiInfoTagBg.r, kUiInfoTagBg.g, kUiInfoTagBg.b, 255},
            .border_color = {kUiSwitchOn.r, kUiSwitchOn.g, kUiSwitchOn.b, 0},
            .corner_radius = layout.info_tag_radius,
        });
        patch.has_font = true;
        patch.font = &get_player_font_px(18, FontWeight::Medium);
        patch.has_font_color = true;
        patch.font_color = kUiTitle;
        patch.has_padding = true;
        patch.padding = 8;
        builder.set_style_override(h, patch);
    }

    static void apply_clean_button_shell_style(::ui::scene::SceneBuilder& builder,
                                               WidgetHandle h,
                                               int padding,
                                               int corner_radius) {
        StylePatch patch = ::ui::scene::make_clean_surface_patch({
            .apply_corner_radius = true,
            .apply_padding = true,
            .corner_radius = corner_radius,
            .padding = padding,
        });
        builder.set_style_patch(h, patch);
    }

    static int centered_button_padding(int button_size, int content_px) noexcept {
        return std::max(0, (button_size - content_px) / 2);
    }

    static void apply_player_style_class(::ui::scene::SceneBuilder& builder,
                                         WidgetHandle h,
                                         PlayerStyleClass style_class) {
        builder.set_style_class(h, static_cast<StyleClassId>(style_class));
    }

    static void apply_icon_button_style(::ui::scene::SceneBuilder& builder,
                                        WidgetHandle h,
                                        PlayerStyleClass style_class,
                                        int padding) {
        apply_player_style_class(builder, h, style_class);
        StylePatch patch{};
        patch.has_padding = true;
        patch.padding = padding;
        builder.set_style_patch(h, patch);
    }

    static void apply_icon_button_shell_style(::ui::scene::SceneBuilder& builder,
                                              WidgetHandle h,
                                              PlayerStyleClass style_class,
                                              int padding,
                                              int corner_radius) {
        apply_player_style_class(builder, h, style_class);
        apply_clean_button_shell_style(builder, h, padding, corner_radius);
    }

    static void apply_centered_icon_button_style(::ui::scene::SceneBuilder& builder,
                                                 WidgetHandle h,
                                                 PlayerStyleClass style_class,
                                                 int button_size,
                                                 int icon_px) {
        apply_icon_button_style(builder, h, style_class,
                                centered_button_padding(button_size, icon_px));
    }

    static void apply_centered_icon_button_shell_style(::ui::scene::SceneBuilder& builder,
                                                       WidgetHandle h,
                                                       PlayerStyleClass style_class,
                                                       int button_size,
                                                       int icon_px,
                                                       int corner_radius) {
        apply_icon_button_shell_style(builder, h, style_class,
                                      centered_button_padding(button_size, icon_px),
                                      corner_radius);
    }

    static std::uint8_t clamp_u8(int value) noexcept {
        if (value <= 0) return 0;
        if (value >= 255) return 255;
        return static_cast<std::uint8_t>(value);
    }

    static rgba alpha_of(const rgba& color, std::uint8_t alpha) noexcept {
        return {color.r, color.g, color.b, alpha};
    }

    static void apply_home_action_button_style(::ui::scene::SceneBuilder& builder,
                                               WidgetHandle h,
                                               int corner_radius = 12) {
        StylePatch patch = ::ui::scene::make_clean_surface_patch({
            .apply_bg_color = true,
            .apply_border_color = true,
            .apply_border_width = true,
            .apply_corner_radius = true,
            .bg_color = alpha_of(kUiButtonBg, 118),
            .border_color = {0, 0, 0, 0},
            .border_width = 0,
            .corner_radius = corner_radius,
        });
        builder.set_style_override(h, patch);
    }

    static WidgetHandle create_home_card_action_button(::ui::scene::SceneBuilder& builder,
                                                       WidgetHandle parent,
                                                       const Rect& rect,
                                                       ::ui::gfx::ImageId icon,
                                                       int icon_px,
                                                       int corner_radius = 12,
                                                       bool interactive = false) {
        const WidgetHandle button = builder.create_button_static("");
        anchor_rect(builder, button, rect);
        builder.set_button_icon(button, icon);
        builder.set_button_icon_size(button, icon_px);
        apply_home_action_button_style(builder, button, corner_radius);
        builder.set_hit_testable(button, interactive);
        builder.link(parent, button);
        return button;
    }

    static StylePatch make_home_daily_mix_header_band_patch(int corner_radius) {
        StylePatch patch{};
        patch.has_gradient_enabled = true;
        patch.gradient_enabled = true;
        patch.has_gradient_start = true;
        patch.gradient_start = kUiHomeDailyMixTop;
        patch.has_gradient_end = true;
        patch.gradient_end = kUiHomeDailyMixBottom;
        patch.has_gradient_direction = true;
        patch.gradient_direction = 1;
        patch.has_border_color = true;
        patch.border_color = {0, 0, 0, 0};
        patch.has_border_width = true;
        patch.border_width = 0;
        patch.has_corner_radius = true;
        patch.corner_radius = corner_radius;
        return patch;
    }

    static StylePatch make_home_stats_probe_header_band_patch(int corner_radius) {
        StylePatch patch = ::ui::scene::make_clean_surface_patch({
            .apply_bg_color = true,
            .apply_border_color = true,
            .apply_border_width = true,
            .apply_corner_radius = true,
            .bg_color = alpha_of(kUiButtonBg, 156),
            .border_color = {0, 0, 0, 0},
            .border_width = 0,
            .corner_radius = corner_radius,
        });
        return patch;
    }

    static WidgetHandle create_home_card_header_band(::ui::scene::SceneBuilder& builder,
                                                     WidgetHandle parent,
                                                     const Rect& rect,
                                                     StylePatch patch) {
        const WidgetHandle band = builder.create_container();
        anchor_rect(builder, band, rect);
        builder.set_style_override(band, patch);
        builder.set_hit_testable(band, false);
        builder.link(parent, band);
        return band;
    }

    static void apply_home_cover_plate_style(::ui::scene::SceneBuilder& builder,
                                             WidgetHandle h,
                                             int corner_radius,
                                             bool emphasize = false) {
        StylePatch patch = ::ui::scene::make_clean_surface_patch({
            .apply_bg_color = true,
            .apply_border_color = true,
            .apply_border_width = true,
            .apply_corner_radius = true,
            .bg_color = emphasize ? alpha_of(kUiListBg, 34)
                                  : alpha_of(kUiListBg, 18),
            .border_color = {0, 0, 0, 0},
            .border_width = 0,
            .corner_radius = corner_radius,
        });
        builder.set_style_override(h, patch);
    }

    static WidgetHandle create_home_meta_chip(::ui::scene::SceneBuilder& builder,
                                              WidgetHandle parent,
                                              const Rect& rect,
                                              std::string_view text) {
        const WidgetHandle chip = create_label_text(builder, text);
        apply_time_label_style(builder, chip);
        builder.set_label_align(chip, ::ui::scene::TextAlignH::Center, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, chip, rect);
        apply_info_tag_style(builder, chip);
        builder.link(parent, chip);
        return chip;
    }

    static void apply_top_bar_button_style(::ui::scene::SceneBuilder& builder, WidgetHandle h) {
        apply_player_style_class(builder, h, PlayerStyleClass::TopBarButton);
    }

    static void apply_top_icon_button_shell_style(::ui::scene::SceneBuilder& builder,
                                                  WidgetHandle h,
                                                  int padding,
                                                  int corner_radius) {
        apply_top_bar_button_style(builder, h);
        apply_clean_button_shell_style(builder, h, padding, corner_radius);
    }

    static void apply_home_toolbar_button_style(::ui::scene::SceneBuilder& builder,
                                                WidgetHandle h,
                                                int corner_radius) {
        apply_top_bar_button_style(builder, h);
        apply_home_action_button_style(builder, h, corner_radius);
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
        apply_player_style_class(builder, h, PlayerStyleClass::ListCard);
    }

    static void apply_control_side_shell_style(::ui::scene::SceneBuilder& builder,
                                               WidgetHandle h,
                                               int btn_size,
                                               int icon_px,
                                               int corner_radius) {
        apply_centered_icon_button_shell_style(builder, h, PlayerStyleClass::ControlSide,
                                               btn_size, icon_px, corner_radius);
    }

    static void apply_control_play_shell_style(::ui::scene::SceneBuilder& builder,
                                               WidgetHandle h,
                                               int btn_size,
                                               int icon_px,
                                               int corner_radius) {
        apply_centered_icon_button_shell_style(builder, h, PlayerStyleClass::ControlPlay,
                                               btn_size, icon_px, corner_radius);
    }

    static void apply_bottom_bar_style(::ui::scene::SceneBuilder& builder, WidgetHandle h) {
        builder.set_style_class(h, static_cast<StyleClassId>(PlayerStyleClass::BottomBar));
    }

    static void apply_bottom_button_style(::ui::scene::SceneBuilder& builder, WidgetHandle h,
                                          int btn_size, int icon_px) {
        apply_centered_icon_button_style(builder, h, PlayerStyleClass::BottomButton, btn_size, icon_px);
    }

    static void apply_bottom_play_style(::ui::scene::SceneBuilder& builder, WidgetHandle h,
                                        int btn_size, int icon_px) {
        apply_centered_icon_button_style(builder, h, PlayerStyleClass::BottomPlay, btn_size, icon_px);
    }

    static void apply_nav_label_style(::ui::scene::SceneBuilder& builder, WidgetHandle h) {
        apply_player_text_style(builder, h, PlayerTextRole::MetaText, kUiTitle);
        builder.set_hit_testable(h, false);
    }

    static void apply_nav_caption_style(::ui::scene::SceneBuilder& builder, WidgetHandle h) {
        apply_text_label_style(builder, h, rgba{kUiTitle.r, kUiTitle.g, kUiTitle.b, 222},
                               get_player_font_px(12, FontWeight::Medium));
        builder.set_hit_testable(h, false);
    }

    static void apply_nav_bar_style(::ui::scene::SceneBuilder& builder, WidgetHandle h, int radius) {
        StylePatch patch = ::ui::scene::make_clean_surface_patch({
            .apply_bg_color = true,
            .apply_border_color = true,
            .apply_corner_radius = true,
            .bg_color = {0, 0, 0, 0},
            .border_color = {0, 0, 0, 0},
            .corner_radius = radius,
        });
        builder.set_style_override(h, patch);
    }

    static void apply_nav_hit_style(::ui::scene::SceneBuilder& builder, WidgetHandle h, int padding) {
        StylePatch patch = make_player_text_patch(PlayerTextRole::MetaText, kUiTitle);
        patch.has_bg_color = true;
        patch.bg_color = {0, 0, 0, 0};
        patch.has_border_color = true;
        patch.border_color = {0, 0, 0, 0};
        patch.has_border_width = true;
        patch.border_width = 0;
        patch.has_shadow_enabled = true;
        patch.shadow_enabled = false;
        patch.has_padding = true;
        patch.padding = padding;
        builder.set_style_override(h, patch);
    }

    static void apply_nav_icon_style(::ui::scene::SceneBuilder& builder, WidgetHandle h) {
        StylePatch patch = ::ui::scene::make_clean_surface_patch({
            .apply_bg_color = true,
            .apply_border_color = true,
            .apply_corner_radius = false,
            .bg_color = {0, 0, 0, 0},
            .border_color = {0, 0, 0, 0},
        });
        builder.set_style_override(h, patch);
        builder.set_hit_testable(h, false);
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

    static void apply_transparent_hit_button_style(::ui::scene::SceneBuilder& builder,
                                                   WidgetHandle h,
                                                   bool interactive = true) {
        StylePatch patch = ::ui::scene::make_clean_surface_patch({
            .apply_bg_color = true,
            .apply_border_color = true,
            .apply_border_width = true,
            .bg_color = {0, 0, 0, 0},
            .border_color = {0, 0, 0, 0},
            .border_width = 0,
        });
        builder.set_style_override(h, patch);
        builder.set_hit_testable(h, interactive);
    }

    static WidgetHandle create_transparent_hit_button(::ui::scene::SceneBuilder& builder,
                                                      const Rect& rect,
                                                      bool interactive = true) {
        const WidgetHandle button = builder.create_button_static("");
        anchor_rect(builder, button, rect);
        apply_transparent_hit_button_style(builder, button, interactive);
        return button;
    }

    static void apply_home_card_style(::ui::scene::SceneBuilder& builder,
                                      WidgetHandle h,
                                      int corner_radius) {
        StylePatch patch = ::ui::scene::make_clean_surface_patch({
            .apply_bg_color = true,
            .apply_border_color = true,
            .apply_border_width = true,
            .apply_corner_radius = true,
            .bg_color = alpha_of(kUiListBg, 230),
            .border_color = {0, 0, 0, 0},
            .border_width = 0,
            .corner_radius = corner_radius,
        });
        builder.set_style_override(h, patch);
    }

    static void apply_home_stats_probe_card_style(::ui::scene::SceneBuilder& builder,
                                                  WidgetHandle h,
                                                  int corner_radius) {
        StylePatch patch = ::ui::scene::make_clean_surface_patch({
            .apply_bg_color = true,
            .apply_border_color = true,
            .apply_border_width = true,
            .apply_corner_radius = true,
            .bg_color = alpha_of(kUiButtonBg, 198),
            .border_color = {0, 0, 0, 0},
            .border_width = 0,
            .corner_radius = corner_radius,
        });
        builder.set_style_override(h, patch);
    }

    static void apply_home_card_preview_plate_style(::ui::scene::SceneBuilder& builder,
                                                    WidgetHandle h,
                                                    int corner_radius,
                                                    bool emphasize = false) {
        StylePatch patch = ::ui::scene::make_clean_surface_patch({
            .apply_bg_color = true,
            .apply_border_color = true,
            .apply_border_width = true,
            .apply_corner_radius = true,
            .bg_color = emphasize ? alpha_of(kUiButtonBg, 54)
                                  : alpha_of(kUiButtonBg, 34),
            .border_color = {0, 0, 0, 0},
            .border_width = 0,
            .corner_radius = corner_radius,
        });
        builder.set_style_override(h, patch);
    }

    static WidgetHandle create_home_daily_mix_card(::ui::scene::SceneBuilder& builder,
                                                   UiHandles& h,
                                                   WidgetHandle parent,
                                                   const Rect& rect,
                                                   std::string_view body,
                                                   std::string_view meta) {
        ::ui::scene::CardBuilder card{builder, rect, 0, 0,
                                      ::ui::scene::LayoutAlign::Start,
                                      static_cast<StyleClassId>(PlayerStyleClass::ListCard),
                                      true};
        const WidgetHandle root = card.root();
        h.home_daily_mix_card = root;
        apply_home_card_style(builder, root, 24);
        h.home_daily_mix_header_band = create_home_card_header_band(
            builder, root, {0, 0, rect.w, 80}, make_home_daily_mix_header_band_patch(24));

        const WidgetHandle title_label = builder.create_label_static("DAILY MIX");
        apply_home_mix_title_style(builder, title_label, kUiTitle);
        builder.set_label_align(title_label, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, title_label, {20, 14, rect.w - 200, 28});
        builder.link(root, title_label);
        h.home_daily_mix_title = title_label;

        const WidgetHandle subtitle_label = builder.create_label_static("Based on History");
        apply_home_mix_subtitle_style(builder, subtitle_label,
                                      rgba{kUiTitle.r, kUiTitle.g, kUiTitle.b, 214});
        builder.set_label_align(subtitle_label, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, subtitle_label, {21, 44, rect.w - 200, 16});
        builder.link(root, subtitle_label);
        h.home_daily_mix_subtitle = subtitle_label;

        {
            const auto slot = ::ui::scene::create_image_slot(
                builder,
                root,
                {
                    .image_rect = {rect.w - 126, 14, 50, 50},
                    .image_corner_radius = 16,
                    .plate_pad = 2,
                },
                [&](::ui::scene::SceneBuilder& slot_builder, WidgetHandle plate) {
                    apply_home_daily_mix_preview_plate_style(slot_builder, plate, 18);
                });
            h.home_daily_mix_covers[0] = slot.image;
            h.home_daily_mix_preview_plates[0] = slot.plate;
        }
        {
            const auto slot = ::ui::scene::create_image_slot(
                builder,
                root,
                {
                    .image_rect = {rect.w - 92, 20, 50, 50},
                    .image_corner_radius = 16,
                    .plate_pad = 2,
                },
                [&](::ui::scene::SceneBuilder& slot_builder, WidgetHandle plate) {
                    apply_home_daily_mix_preview_plate_style(slot_builder, plate, 24);
                });
            h.home_daily_mix_covers[1] = slot.image;
            h.home_daily_mix_preview_plates[1] = slot.plate;
        }
        {
            const auto slot = ::ui::scene::create_image_slot(
                builder,
                root,
                {
                    .image_rect = {rect.w - 64, 16, 50, 50},
                    .image_corner_radius = 16,
                    .plate_pad = 2,
                },
                [&](::ui::scene::SceneBuilder& slot_builder, WidgetHandle plate) {
                    apply_home_daily_mix_preview_plate_style(slot_builder, plate, 18);
                });
            h.home_daily_mix_covers[2] = slot.image;
            h.home_daily_mix_preview_plates[2] = slot.plate;
        }

        const WidgetHandle body_label = create_label_text(builder, body);
        apply_player_text_style(builder, body_label, PlayerTextRole::CardBody, kUiSubtitle);
        builder.set_label_align(body_label, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, body_label, {20, 94, rect.w - 120, 18});
        builder.link(root, body_label);
        h.home_daily_mix_body = body_label;

        h.home_daily_mix_chip = create_home_meta_chip(builder, root, {rect.w - 102, 90, 82, 26}, meta);

        builder.link(parent, root);
        return root;
    }

    static WidgetHandle create_home_recently_played_card(::ui::scene::SceneBuilder& builder,
                                                         UiHandles& h,
                                                         WidgetHandle parent,
                                                         const Rect& rect,
                                                         const PlayerIconIds& icons,
                                                         std::string_view body,
                                                         std::string_view meta) {
        ::ui::scene::CardBuilder card{builder, rect, 0, 0,
                                      ::ui::scene::LayoutAlign::Start,
                                      static_cast<StyleClassId>(PlayerStyleClass::ListCard),
                                      true};
        const WidgetHandle root = card.root();
        h.home_recently_played_card = root;
        apply_home_card_style(builder, root, 24);

        const WidgetHandle title_label = builder.create_label_static("Recently Played");
        apply_player_text_style(builder, title_label, PlayerTextRole::CardTitle, kUiTitle);
        builder.set_label_align(title_label, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, title_label, {20, 14, rect.w - 112, 24});
        builder.link(root, title_label);
        h.home_recently_played_title = title_label;

        h.home_recently_played_action = create_home_card_action_button(
            builder, root, {rect.w - 58, 14, 36, 36}, icons.next, 18, 18);

        const WidgetHandle body_label = create_label_text(builder, body);
        apply_player_text_style(builder, body_label, PlayerTextRole::CardBody, kUiSubtitle);
        builder.set_label_align(body_label, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, body_label, {20, 44, rect.w - 136, 18});
        builder.link(root, body_label);
        h.home_recently_played_body = body_label;

        h.home_recently_played_chip = create_home_meta_chip(builder, root, {rect.w - 96, 44, 76, 24}, meta);

        {
            const auto slot = ::ui::scene::create_image_slot(
                builder,
                root,
                {
                    .image_rect = {20, 82, 118, 40},
                    .image_corner_radius = 20,
                    .plate_pad = 1,
                },
                [&](::ui::scene::SceneBuilder& slot_builder, WidgetHandle plate) {
                    apply_home_card_preview_plate_style(slot_builder, plate, 21, true);
                });
            h.home_recently_played_covers[0] = slot.image;
            h.home_recently_played_preview_plates[0] = slot.plate;
        }
        {
            const auto slot = ::ui::scene::create_image_slot(
                builder,
                root,
                {
                    .image_rect = {148, 82, 96, 40},
                    .image_corner_radius = 20,
                    .plate_pad = 1,
                },
                [&](::ui::scene::SceneBuilder& slot_builder, WidgetHandle plate) {
                    apply_home_card_preview_plate_style(slot_builder, plate, 21, false);
                });
            h.home_recently_played_covers[1] = slot.image;
            h.home_recently_played_preview_plates[1] = slot.plate;
        }
        {
            const auto slot = ::ui::scene::create_image_slot(
                builder,
                root,
                {
                    .image_rect = {rect.w - 68, 76, 48, 48},
                    .image_corner_radius = 24,
                    .plate_pad = 1,
                },
                [&](::ui::scene::SceneBuilder& slot_builder, WidgetHandle plate) {
                    apply_home_card_preview_plate_style(slot_builder, plate, 25, true);
                });
            h.home_recently_played_covers[2] = slot.image;
            h.home_recently_played_preview_plates[2] = slot.plate;
        }

        builder.link(parent, root);
        return root;
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

        const WidgetHandle title_label = create_label_text(builder, title);
        apply_player_text_style(builder, title_label, PlayerTextRole::CardTitle, kUiTitle);
        builder.set_label_align(title_label, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, title_label, {16, 12, rect.w - 120, 24});
        builder.link(root, title_label);

        const WidgetHandle body_label = create_label_text(builder, body);
        apply_player_text_style(builder, body_label, PlayerTextRole::CardBody, kUiSubtitle);
        builder.set_label_align(body_label, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, body_label, {16, 38, rect.w - 120, 18});
        builder.link(root, body_label);

        create_home_meta_chip(builder, root, {rect.w - 92, 22, 72, 24}, meta);

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
        h.home_stats_probe_card = root;
        apply_home_stats_probe_card_style(builder, root, 16);
        h.home_stats_header_band = create_home_card_header_band(
            builder, root, {0, 0, rect.w, 56}, make_home_stats_probe_header_band_patch(18));

        const WidgetHandle title_label = builder.create_label_static("Listening stats");
        apply_player_text_style(builder, title_label, PlayerTextRole::CardTitle, kUiTitle);
        builder.set_label_align(title_label, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, title_label, {18, 10, rect.w - 110, 24});
        builder.link(root, title_label);
        h.home_stats_title = title_label;

        const WidgetHandle subtitle_label = builder.create_label_static("This Week");
        apply_player_text_style(builder, subtitle_label, PlayerTextRole::MetaText, kUiSubtitle);
        builder.set_label_align(subtitle_label, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, subtitle_label, {18, 31, rect.w - 110, 16});
        builder.link(root, subtitle_label);
        h.home_stats_subtitle = subtitle_label;

        h.home_stats_action = create_home_card_action_button(
            builder, root, {rect.w - 54, 11, 34, 34}, icons.next, 18, 17);

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
        h.home_stats_plays_label = plays_label;

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
        h.home_stats_avg_label = avg_label;

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

    static void build_bottom_track_tile(::ui::scene::SceneBuilder& builder,
                                        ::ui::scene::RowBuilder& bottom_row,
                                        UiHandles& h,
                                        const Rect& bottom_bar_rect,
                                        int play_size,
                                        int next_size,
                                        int bar_gap) {
        const Rect content = bottom_row.content_rect();
        const int tile_w = std::max<int>(
            0,
            static_cast<int>(content.w - play_size - next_size - bar_gap * 2));
        const int tile_h = content.h;
        const Rect tile_rect = bottom_row.next_rect(tile_w, tile_h);
        const int bottom_text_x = 56;
        const int bottom_text_w = std::max<int>(
            0,
            static_cast<int>(tile_rect.w - bottom_text_x - 4));

        ::ui::scene::TileBuilder bottom_tile{builder, tile_rect, 48, 8, false, kStyleClassInvalid, true, ""};
        h.bottom_cover = bottom_tile.image();
        ::ui::scene::configure_image_surface(builder, h.bottom_cover, 16);
        h.bottom_title = bottom_tile.label();
        anchor_rect(builder, h.bottom_title, {bottom_text_x, 4, bottom_text_w, 18});
        builder.set_label_align(h.bottom_title, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        apply_text_label_style(builder, h.bottom_title, kUiTitle, get_player_font_px(14, FontWeight::Medium));
        bottom_row.add_at(bottom_tile.root(), tile_rect);

        h.bottom_subtitle = builder.create_label_static("");
        anchor_rect(builder, h.bottom_subtitle, {bottom_text_x, 24, bottom_text_w, 16});
        builder.set_label_align(h.bottom_subtitle, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        apply_text_label_style(builder, h.bottom_subtitle, kUiSubtitle, get_player_font_px(12, FontWeight::Regular));
        builder.link(bottom_tile.root(), h.bottom_subtitle);

        h.bottom_hit = create_transparent_hit_button(builder, {0, 0, bottom_bar_rect.w, bottom_bar_rect.h}, true);
        builder.link(h.bottom_bar, h.bottom_hit);
    }

    static WidgetHandle add_bottom_transport_button(::ui::scene::SceneBuilder& builder,
                                                    ::ui::scene::RowBuilder& bottom_row,
                                                    ::ui::gfx::ImageId icon,
                                                    int button_size,
                                                    int icon_px,
                                                    bool primary) {
        const WidgetHandle button = builder.create_button_static("");
        bottom_row.add(button, button_size, button_size);
        builder.set_button_icon(button, icon);
        builder.set_button_icon_size(button, icon_px);
        if (primary) {
            apply_bottom_play_style(builder, button, button_size, icon_px);
        } else {
            apply_bottom_button_style(builder, button, button_size, icon_px);
        }
        return button;
    }

    static void build_nav_button(::ui::scene::SceneBuilder& builder,
                                 WidgetHandle& out_hit,
                                 WidgetHandle& out_icon,
                                 WidgetHandle& out_label,
                                 const Rect& rect,
                                 const char* text,
                                 ::ui::gfx::ImageId icon,
                                 int icon_px) {
        constexpr int kNavIconSlot = 28;
        constexpr int kNavIconY = 8;
        constexpr int kNavLabelY = 40;
        constexpr int kNavLabelH = 14;

        out_hit = builder.create_button_static("");
        anchor_rect(builder, out_hit, rect);
        apply_nav_hit_style(builder, out_hit, 0);

        out_icon = builder.create_button_static("");
        anchor_rect(builder, out_icon,
                    {(rect.w - kNavIconSlot) / 2, kNavIconY, kNavIconSlot, kNavIconSlot});
        builder.set_button_icon(out_icon, icon);
        builder.set_button_icon_size(out_icon, icon_px);
        apply_nav_icon_style(builder, out_icon);
        builder.link(out_hit, out_icon);

        out_label = builder.create_label_static(text);
        anchor_rect(builder, out_label, {4, kNavLabelY, rect.w - 8, kNavLabelH});
        apply_nav_caption_style(builder, out_label);
        builder.set_label_align(out_label, ::ui::scene::TextAlignH::Center, ::ui::scene::TextAlignV::Center);
        builder.link(out_hit, out_label);
    }

    static void apply_transition_shell_style(::ui::scene::SceneBuilder& builder,
                                             WidgetHandle h,
                                             int corner_radius) {
        StylePatch patch = ::ui::scene::make_clean_surface_patch({
            .apply_bg_color = true,
            .apply_border_color = true,
            .apply_border_width = true,
            .apply_corner_radius = true,
            .bg_color = {0, 0, 0, 0},
            .border_color = {0, 0, 0, 0},
            .border_width = 0,
            .corner_radius = corner_radius,
        });
        patch.has_shadow_enabled = true;
        patch.shadow_enabled = false;
        builder.set_style_override(h, patch);
        builder.set_hit_testable(h, false);
    }

    static void apply_transition_play_button_shell_style(::ui::scene::SceneBuilder& builder,
                                                         WidgetHandle h) {
        StylePatch patch{};
        patch.has_padding = true;
        patch.padding = 20;
        patch.has_corner_radius = true;
        patch.corner_radius = 26;
        patch.has_shadow_enabled = true;
        patch.shadow_enabled = false;
        patch.has_border_width = true;
        patch.border_width = 0;
        patch.has_inner_stroke_enabled = true;
        patch.inner_stroke_enabled = false;
        patch.has_outline_enabled = true;
        patch.outline_enabled = false;
        builder.set_style_patch(h, patch);
        builder.set_hit_testable(h, false);
    }

    static WidgetHandle create_transition_play_button(::ui::scene::SceneBuilder& builder,
                                                      ::ui::gfx::ImageId icon) {
        const WidgetHandle button = builder.create_button_static("");
        anchor_rect(builder, button, {0, 0, 0, 0});
        builder.set_button_icon(button, icon);
        builder.set_button_icon_size(button, 32);
        apply_bottom_play_style(builder, button, 72, 32);
        apply_transition_play_button_shell_style(builder, button);
        return button;
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
        mark_draw_scope(builder, h.bottom_bar, PlayerDrawScopeId::BottomBar);
        apply_bottom_bar_style(builder, h.bottom_bar);
        build_bottom_track_tile(builder, bottom_row, h, bottom_bar_rect, play_size, next_size, bar_gap);
        h.bottom_play = add_bottom_transport_button(builder, bottom_row, icons.play, play_size, 24, true);
        h.bottom_next = add_bottom_transport_button(builder, bottom_row, icons.next, next_size, 24, false);

        const Rect nav_rect{kUiPadding, layout.nav_y,
                            screen_width - kUiPadding * 2, layout.nav_h};
        h.nav_bar = builder.create_container();
        anchor_rect(builder, h.nav_bar, nav_rect);
        apply_nav_bar_style(builder, h.nav_bar, nav_rect.h / 2);
        const int nav_btn_w = (nav_rect.w - layout.nav_gap * 2) / 3;
        const int nav_home_icon = 24;
        const int nav_search_icon = 26;
        const int nav_library_icon = 24;

        build_nav_button(builder, h.nav_home, h.nav_home_icon, h.nav_home_label,
                         {0, 0, nav_btn_w, layout.nav_h}, "Home", icons.home, nav_home_icon);
        build_nav_button(builder, h.nav_search, h.nav_search_icon, h.nav_search_label,
                         {nav_btn_w + layout.nav_gap, 0, nav_btn_w, layout.nav_h},
                         "Search", icons.search, nav_search_icon);
        build_nav_button(builder, h.nav_library, h.nav_library_icon, h.nav_library_label,
                         {(nav_btn_w + layout.nav_gap) * 2, 0, nav_btn_w, layout.nav_h},
                         "Library", icons.folder, nav_library_icon);

        builder.link(h.nav_bar, h.nav_home);
        builder.link(h.nav_bar, h.nav_search);
        builder.link(h.nav_bar, h.nav_library);

        builder.link(h.root, h.bottom_bar);
        builder.set_hit_testable(h.bottom_bar, true);
        builder.link(h.root, h.nav_bar);
    }

    static void build_now_playing_transition_overlay(::ui::scene::SceneBuilder& builder,
                                                     UiHandles& h,
                                                     const PlayerIconIds& icons) {
        h.transition_root = builder.create_container();
        mark_draw_scope(builder, h.transition_root, PlayerDrawScopeId::NowTransition);
        anchor_rect(builder, h.transition_root, {0, 0, screen_width, screen_height});

        h.transition_shell = builder.create_container();
        anchor_rect(builder, h.transition_shell, {0, 0, 0, 0});
        apply_transition_shell_style(builder, h.transition_shell, 28);

        h.transition_cover = builder.create_image();
        anchor_rect(builder, h.transition_cover, {0, 0, 0, 0});
        ::ui::scene::configure_image_surface(builder, h.transition_cover, 18);

        h.transition_title = builder.create_label_static("");
        {
            StylePatch patch = make_player_text_patch(PlayerTextRole::CardTitle, kUiTitle);
            patch.has_font = true;
            patch.font = &get_player_font_px(20, FontWeight::Medium);
            builder.set_style_override(h.transition_title, patch);
        }
        builder.set_label_align(h.transition_title, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        builder.set_hit_testable(h.transition_title, false);
        anchor_rect(builder, h.transition_title, {0, 0, 0, 0});

        h.transition_subtitle = builder.create_label_static("");
        {
            StylePatch patch = make_player_text_patch(PlayerTextRole::PageSubtitle, kUiSubtitle);
            patch.has_font = true;
            patch.font = &get_player_font_px(14, FontWeight::Regular);
            builder.set_style_override(h.transition_subtitle, patch);
        }
        builder.set_label_align(h.transition_subtitle, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        builder.set_hit_testable(h.transition_subtitle, false);
        anchor_rect(builder, h.transition_subtitle, {0, 0, 0, 0});

        h.transition_play = create_transition_play_button(builder, icons.play);

        h.transition_hit = create_transparent_hit_button(builder, {0, 0, screen_width, screen_height}, true);

        builder.link(h.transition_root, h.transition_shell);
        builder.link(h.transition_root, h.transition_cover);
        builder.link(h.transition_root, h.transition_title);
        builder.link(h.transition_root, h.transition_subtitle);
        builder.link(h.transition_root, h.transition_play);
        builder.link(h.transition_root, h.transition_hit);
    }
