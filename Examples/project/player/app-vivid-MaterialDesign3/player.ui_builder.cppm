module;
#include <algorithm>
#include <array>
#include <cstdint>

export module player.ui_builder;

import charm.core.config;
import charm.core.geometry;
import charm.ui.scene;
import player.controller;
import player.ui;

namespace player::ui_builder_detail {
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
        const int title_h = 32;
        const int subtitle_h = 16;
        const int progress_h = 3;
        const int text_col_y = header_top + 12;
        const int text_col_h = title_h + subtitle_h + progress_h + text_gap * 2;
        const int controls_y = text_col_y + text_col_h + 58;
        const int control_side = 104;
        const int control_play = 120;
        const int control_gap = 16;
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
        const int title_h = 32;
        const int subtitle_h = 16;
        const int progress_h = 3;
        const int time_row_h = 16;
        const int text_gap = 6;
        const int header_top = (layout.top_bar_y + layout.top_bar_h + 12) + kCoverSize;
        const int text_col_y = header_top + 12;
        const int text_col_h = 118;
        const int time_row_y = text_col_y + title_h + text_gap + subtitle_h + text_gap + progress_h + text_gap;

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
        patch.font = &get_font(FontId::Small);
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

    static void build_now_playing(::ui::scene::SceneBuilder& builder, UiHandles& h,
                                  const UiLayout& layout, const NowTextLayout& text_layout,
                                  const PlayerIconIds& icons) {
        constexpr int kNowTextPadding = 0;
        constexpr int kNowControlsPadding = 0;
        constexpr int kNowTimePadding = 0;
        const int title_h = text_layout.title_h;
        const int subtitle_h = text_layout.subtitle_h;
        const int progress_h = text_layout.progress_h;
        const int time_row_h = text_layout.time_row_h;
        const int time_row_y = text_layout.time_row_y;
        const int text_col_y = text_layout.text_col_y;
        const int text_col_h = text_layout.text_col_h;
        const int text_gap = text_layout.text_gap;
        WidgetHandle text_col_root{};

        h.cover = builder.create_image();
        anchor_rect(builder, h.cover, {layout.cover_x, layout.cover_y, kCoverSize, kCoverSize});
        {
            StylePatch patch{};
            patch.has_corner_radius = true;
            patch.corner_radius = 20;
            builder.set_style_override(h.cover, patch);
        }

        constexpr bool kShowCoverCollage = false;
        if (kShowCoverCollage) {
            const int collage_small = 64;
            const int collage_tiny = 52;
            h.cover_left = builder.create_image();
            anchor_rect(builder, h.cover_left, {layout.cover_x - 18,
                                                layout.cover_y + kCoverSize - collage_small - 8,
                                                collage_small, collage_small});
            {
                StylePatch patch{};
                patch.has_corner_radius = true;
                patch.corner_radius = 12;
                patch.has_shadow_enabled = true;
                patch.shadow_enabled = true;
                patch.has_shadow_color = true;
                patch.shadow_color = {0, 0, 0, 80};
                patch.has_shadow_offset_x = true;
                patch.shadow_offset_x = 0;
                patch.has_shadow_offset_y = true;
                patch.shadow_offset_y = 2;
                patch.has_shadow_spread = true;
                patch.shadow_spread = 2;
                patch.has_shadow_radius = true;
                patch.shadow_radius = 10;
                builder.set_style_override(h.cover_left, patch);
            }
            h.cover_right = builder.create_image();
            anchor_rect(builder, h.cover_right, {layout.cover_x + kCoverSize - collage_tiny + 18,
                                                 layout.cover_y + kCoverSize / 2 + 28,
                                                 collage_tiny, collage_tiny});
            {
                StylePatch patch{};
                patch.has_corner_radius = true;
                patch.corner_radius = 12;
                patch.has_shadow_enabled = true;
                patch.shadow_enabled = true;
                patch.has_shadow_color = true;
                patch.shadow_color = {0, 0, 0, 80};
                patch.has_shadow_offset_x = true;
                patch.shadow_offset_x = 0;
                patch.has_shadow_offset_y = true;
                patch.shadow_offset_y = 2;
                patch.has_shadow_spread = true;
                patch.shadow_spread = 2;
                patch.has_shadow_radius = true;
                patch.shadow_radius = 10;
                builder.set_style_override(h.cover_right, patch);
            }
        }

        h.title = builder.create_label_static("");
        apply_text_label_style(builder, h.title, kUiTitle, get_font(FontId::Large));
        builder.set_label_align(h.title, ::ui::scene::TextAlignH::Center, ::ui::scene::TextAlignV::Center);
        builder.set_hit_testable(h.title, false);

        h.subtitle = builder.create_label_static("");
        apply_text_label_style(builder, h.subtitle, kUiSubtitle, get_font(FontId::Small));
        builder.set_label_align(h.subtitle, ::ui::scene::TextAlignH::Center, ::ui::scene::TextAlignV::Center);
        builder.set_hit_testable(h.subtitle, false);

        h.progress = builder.create_progress();
        builder.set_range(h.progress, 0, 100);
        builder.set_value(h.progress, 0);
        builder.set_hit_testable(h.progress, true);

        h.time_left = builder.create_label_static("");
        h.time_right = builder.create_label_static("");
        h.info_tag = builder.create_button_static("");
        apply_time_label_style(builder, h.time_left);
        apply_time_label_style(builder, h.time_right);
        builder.set_label_align(h.time_right, ::ui::scene::TextAlignH::Right, ::ui::scene::TextAlignV::Center);
        apply_info_tag_style(builder, h.info_tag);
        builder.set_hit_testable(h.info_tag, false);

        h.status = builder.create_label_static("");
        anchor_rect(builder, h.status, {0, 0, 0, 0});

        h.mode_hint = builder.create_label_static("");
        anchor_rect(builder, h.mode_hint, {0, 0, 0, 0});

        h.spectrum = builder.create_container();
        anchor_rect(builder, h.spectrum, {0, 0, 0, 0});
        builder.set_hit_testable(h.spectrum, false);

        h.eq_panel = builder.create_container();
        anchor_rect(builder, h.eq_panel, {0, 0, 0, 0});
        builder.set_hit_testable(h.eq_panel, false);

        h.eq_title = builder.create_label_static("EQ");
        anchor_rect(builder, h.eq_title, {0, 0, 0, 0});

        constexpr std::array<const char*, kEqBands> kEqLabels{
            "60", "250", "1K", "4K", "16K"
        };
        for (std::size_t i = 0; i < kEqBands; ++i) {
            const int row_y = 0;
            h.eq_labels[i] = builder.create_label_static(kEqLabels[i]);
            anchor_rect(builder, h.eq_labels[i], {0, row_y, 0, 0});

            const int slider_x = 0;
            const int slider_w = 0;
            h.eq_sliders[i] = builder.create_slider();
            anchor_rect(builder, h.eq_sliders[i], {slider_x, row_y, slider_w, 0});
            builder.set_range(h.eq_sliders[i], -12, 12);
            builder.set_value(h.eq_sliders[i], 0);

            h.eq_values[i] = builder.create_label_static("");
            anchor_rect(builder, h.eq_values[i], {0, row_y, 0, 0});
        }

        const int vol_y = 0;
        h.volume_label = builder.create_label_static("Vol");
        anchor_rect(builder, h.volume_label, {0, vol_y, 0, 0});
        const int vol_slider_x = 0;
        const int vol_slider_w = 0;
        h.volume_slider = builder.create_slider();
        anchor_rect(builder, h.volume_slider, {vol_slider_x, vol_y, vol_slider_w, 0});
        builder.set_range(h.volume_slider, 0, 100);
        builder.set_value(h.volume_slider, 80);
        h.volume_value = builder.create_label_static("");
        anchor_rect(builder, h.volume_value, {0, vol_y, 0, 0});

        const int dc_y = 0;
        h.dc_label = builder.create_label_static("DC");
        anchor_rect(builder, h.dc_label, {0, dc_y, 0, 0});
        h.dc_switch = builder.create_switch();
        anchor_rect(builder, h.dc_switch, {0, dc_y, 0, 0});
        builder.set_checked(h.dc_switch, true);

        const int clip_y = 0;
        h.clip_label = builder.create_label_static("Clip");
        anchor_rect(builder, h.clip_label, {0, clip_y, 0, 0});
        h.clip_switch = builder.create_switch();
        anchor_rect(builder, h.clip_switch, {0, clip_y, 0, 0});
        builder.set_checked(h.clip_switch, true);
        const int clip_slider_x = 0;
        const int clip_slider_w = 0;
        h.clip_slider = builder.create_slider();
        anchor_rect(builder, h.clip_slider, {clip_slider_x, clip_y, clip_slider_w, 0});
        builder.set_range(h.clip_slider, 60, 100);
        builder.set_value(h.clip_slider, 85);
        h.clip_value = builder.create_label_static("");
        anchor_rect(builder, h.clip_value, {0, clip_y, 0, 0});

        constexpr int control_side = 104;
        constexpr int control_play = 120;
        const int small_btn = control_side;
        const int play_btn = control_play;
        const int btn_gap = 16;
        const Rect controls_rect{layout.controls_x, layout.controls_y, layout.controls_w, control_play};
        ::ui::scene::RowBuilder controls_row{builder, controls_rect, btn_gap, kNowControlsPadding,
                                             ::ui::scene::LayoutAlign::Center};
        h.controls = controls_row.root();

        h.btn_prev = builder.create_button_static("");
        controls_row.add(h.btn_prev, small_btn, small_btn);
        builder.set_button_icon(h.btn_prev, icons.prev);
        builder.set_button_icon_size(h.btn_prev, 26);
        apply_control_side_style(builder, h.btn_prev, small_btn, 26);

        h.btn_pause = builder.create_button_static("");
        controls_row.add(h.btn_pause, play_btn, play_btn);
        builder.set_button_icon(h.btn_pause, icons.play);
        builder.set_button_icon_size(h.btn_pause, 32);
        apply_control_play_style(builder, h.btn_pause, play_btn, 32);

        h.btn_next = builder.create_button_static("");
        controls_row.add(h.btn_next, small_btn, small_btn);
        builder.set_button_icon(h.btn_next, icons.next);
        builder.set_button_icon_size(h.btn_next, 26);
        apply_control_side_style(builder, h.btn_next, small_btn, 26);

        constexpr bool kShowModeButton = false;
        if constexpr (kShowModeButton) {
            h.btn_mode = builder.create_button_static("");
            anchor_rect(builder, h.btn_mode, {0, 0, 0, 0});
            builder.link(h.controls, h.btn_mode);
        }

        {
            const Rect text_rect{kUiPadding, text_col_y,
                                 screen_width - kUiPadding * 2, text_col_h};
            ::ui::scene::ColumnBuilder text_col{builder, text_rect, text_gap, kNowTextPadding,
                                                ::ui::scene::LayoutAlign::Start};
            text_col_root = text_col.root();
            text_col.add(h.title, text_rect.w, title_h);
            text_col.add(h.subtitle, text_rect.w, subtitle_h);
            text_col.add(h.progress, text_rect.w, progress_h);
        }
        WidgetHandle time_row_root{};
        {
            const int time_row_w = screen_width - kUiPadding * 2;
            const int time_row_x = kUiPadding;
            const int time_side_w = 52;
            const int info_w = std::max(170, time_row_w - time_side_w * 2 - 16);
            const int extra = time_row_w - (time_side_w * 2 + info_w);
            const int row_pad = (extra > 0) ? (extra / 2) : 0;
            const Rect time_row_rect{time_row_x, time_row_y, time_row_w, time_row_h};
            ::ui::scene::RowBuilder time_row{builder, time_row_rect, 0, row_pad + kNowTimePadding,
                                             ::ui::scene::LayoutAlign::Center};
            time_row_root = time_row.root();
            time_row.add(h.time_left, time_side_w, time_row_h);
            time_row.add(h.info_tag, info_w, time_row_h);
            time_row.add(h.time_right, time_side_w, time_row_h);
        }

#if defined(CHARM_PLAYER_COVER_DEBUG)
        constexpr bool kShowCoverDebugLabel = false;
        if constexpr (kShowCoverDebugLabel) {
            h.cover_debug = builder.create_label_static("");
            apply_time_label_style(builder, h.cover_debug);
            builder.set_label_align(h.cover_debug, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
            anchor_rect(builder, h.cover_debug, {kUiPadding, time_row_y + time_row_h + 4,
                                                 screen_width - kUiPadding * 2, 12});
            builder.set_hit_testable(h.cover_debug, false);
        }
#endif

        const WidgetHandle now_playing = builder.create_container();
        anchor_rect(builder, now_playing, {0, 0, screen_width, screen_height});
        h.page_now_playing = now_playing;

        h.now_backdrop = builder.create_container();
        anchor_rect(builder, h.now_backdrop, {0, 0, screen_width, screen_height});
        {
            StylePatch patch{};
            patch.has_bg_color = true;
            patch.bg_color = kUiBackdropBase;
            patch.has_border_color = true;
            patch.border_color = {0, 0, 0, 0};
            patch.has_border_width = true;
            patch.border_width = 0;
            patch.has_corner_radius = true;
            patch.corner_radius = 0;
            builder.set_style_override(h.now_backdrop, patch);
        }

        const WidgetHandle top_bar = builder.create_container();
        anchor_rect(builder, top_bar, {layout.top_bar_x, layout.top_bar_y, layout.top_bar_w, layout.top_bar_h});
        h.now_back = builder.create_button_static("");
        anchor_rect(builder, h.now_back, {layout.top_bar_x, layout.top_bar_y, 40, 40});
        builder.set_button_icon(h.now_back, icons.prev);
        builder.set_button_icon_size(h.now_back, 16);
        apply_top_bar_button_style(builder, h.now_back);
        const WidgetHandle top_title = builder.create_label_static("Now Playing");
        anchor_rect(builder, top_title, {layout.top_bar_x + 48, layout.top_bar_y + 8,
                                         layout.top_bar_w - 96, 24});
        const WidgetHandle top_more = builder.create_button_static("");
        anchor_rect(builder, top_more, {layout.top_bar_x + layout.top_bar_w - 40, layout.top_bar_y, 40, 40});
        builder.set_button_icon(top_more, icons.more);
        builder.set_button_icon_size(top_more, 14);
        apply_top_bar_button_style(builder, top_more);
        h.now_more = top_more;

        const WidgetHandle top_lyrics = builder.create_button_static("Lyrics");
        anchor_rect(builder, top_lyrics, {layout.top_bar_x + layout.top_bar_w - 88, layout.top_bar_y, 40, 40});
        builder.set_label_align(top_lyrics, ::ui::scene::TextAlignH::Center, ::ui::scene::TextAlignV::Center);
        apply_top_bar_button_style(builder, top_lyrics);
        h.now_lyrics = top_lyrics;

        builder.link(h.root, now_playing);
        builder.link(now_playing, h.now_backdrop);
        builder.link(now_playing, top_bar);
        builder.link(top_bar, h.now_back);
        builder.link(top_bar, top_title);
        builder.link(top_bar, top_more);
        builder.link(top_bar, top_lyrics);

        builder.link(now_playing, h.cover);
        if (kShowCoverCollage) {
            builder.link(now_playing, h.cover_left);
            builder.link(now_playing, h.cover_right);
        }
        if (text_col_root) {
            builder.link(now_playing, text_col_root);
        }
        if (time_row_root) {
            builder.link(now_playing, time_row_root);
        }
#if defined(CHARM_PLAYER_COVER_DEBUG)
        builder.link(now_playing, h.cover_debug);
#endif
        builder.link(now_playing, h.status);
        builder.link(now_playing, h.mode_hint);
        builder.link(now_playing, h.spectrum);
        builder.link(now_playing, h.eq_panel);
        builder.link(now_playing, h.eq_title);
        for (std::size_t i = 0; i < kEqBands; ++i) {
            builder.link(now_playing, h.eq_labels[i]);
            builder.link(now_playing, h.eq_sliders[i]);
            builder.link(now_playing, h.eq_values[i]);
        }
        builder.link(now_playing, h.volume_label);
        builder.link(now_playing, h.volume_slider);
        builder.link(now_playing, h.volume_value);
        builder.link(now_playing, h.dc_label);
        builder.link(now_playing, h.dc_switch);
        builder.link(now_playing, h.clip_label);
        builder.link(now_playing, h.clip_switch);
        builder.link(now_playing, h.clip_slider);
        builder.link(now_playing, h.clip_value);
        builder.link(now_playing, h.controls);
    }

    static void build_home(::ui::scene::SceneBuilder& builder, UiHandles& h,
                           const UiLayout& layout, const PlayerIconIds& icons) {
        const WidgetHandle home = builder.create_container();
        anchor_rect(builder, home, {0, 0, screen_width, screen_height});
        h.page_home = home;

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
        const int title_line_h = 52;
        h.home_title_top = builder.create_label_static("Your");
        apply_text_label_style(builder, h.home_title_top, kUiTitle, get_font(FontId::Large));
        builder.set_label_align(h.home_title_top, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, h.home_title_top, {title_x, title_y, title_w, title_line_h});

        h.home_title_bottom = builder.create_label_static("Mix");
        apply_text_label_style(builder, h.home_title_bottom, kUiTitle, get_font(FontId::Large));
        builder.set_label_align(h.home_title_bottom, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, h.home_title_bottom, {title_x, title_y + title_line_h, title_w, title_line_h});

        h.home_subtitle = builder.create_label_static("Today's Mix for you");
        apply_text_label_style(builder, h.home_subtitle, kUiSubtitle, get_font(FontId::Small));
        builder.set_label_align(h.home_subtitle, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, h.home_subtitle, {title_x, title_y + title_line_h * 2 + 6, title_w, 20});

        h.home_play = builder.create_button_static("");
        const int play_size = 80;
        const int play_x = screen_width - kUiPadding - play_size;
        const int play_y = title_y + 6;
        anchor_rect(builder, h.home_play, {play_x, play_y, play_size, play_size});
        builder.set_button_icon(h.home_play, icons.play);
        builder.set_button_icon_size(h.home_play, 20);
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

        const int collage_big = 260;
        const int collage_small = 92;
        const int collage_small2 = 86;
        const int collage_small3 = 74;
        const int collage_y = title_y + 136;
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
                                                 collage_y + collage_big - collage_small + 10,
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
        anchor_rect(builder, h.home_cover_right, {collage_x + collage_big - collage_small2 + 26,
                                                  collage_y + collage_big / 2 + 14,
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
                                                  collage_y + collage_big - collage_small3 + 20,
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

        const WidgetHandle home_toolbar = builder.create_container();
        anchor_rect(builder, home_toolbar, {layout.top_bar_x, layout.top_bar_y,
                                            layout.top_bar_w, layout.top_bar_h});
        const WidgetHandle home_settings = builder.create_button_static("");
        anchor_rect(builder, home_settings, {layout.top_bar_x + layout.top_bar_w - 44, layout.top_bar_y, 40, 40});
        builder.set_button_icon(home_settings, icons.settings);
        builder.set_button_icon_size(home_settings, 14);
        apply_top_bar_button_style(builder, home_settings);
        const WidgetHandle home_more = builder.create_button_static("");
        anchor_rect(builder, home_more, {layout.top_bar_x + layout.top_bar_w - 92, layout.top_bar_y, 40, 40});
        builder.set_button_icon(home_more, icons.more);
        builder.set_button_icon_size(home_more, 14);
        apply_top_bar_button_style(builder, home_more);

        builder.link(home, h.home_backdrop);
        builder.link(home, h.home_title_top);
        builder.link(home, h.home_title_bottom);
        builder.link(home, h.home_subtitle);
        builder.link(home, h.home_play);
        builder.link(home, h.home_cover_big);
        builder.link(home, h.home_cover_left);
        builder.link(home, h.home_cover_right);
        builder.link(home, h.home_cover_small);
        builder.link(home, home_toolbar);
        builder.link(home_toolbar, home_settings);
        builder.link(home_toolbar, home_more);
    }

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

        h.nav_bar = builder.create_container();
        anchor_rect(builder, h.nav_bar, {0, 0, 0, 0});
        h.nav_home = builder.create_button_static("");
        anchor_rect(builder, h.nav_home, {0, 0, 0, 0});
        h.nav_search = builder.create_button_static("");
        anchor_rect(builder, h.nav_search, {0, 0, 0, 0});
        h.nav_library = builder.create_button_static("");
        anchor_rect(builder, h.nav_library, {0, 0, 0, 0});

        const WidgetHandle library = builder.create_container();
        anchor_rect(builder, library, {0, 0, screen_width, screen_height});
        h.page_library = library;

        const WidgetHandle lib_top = builder.create_container();
        anchor_rect(builder, lib_top, {layout.top_bar_x, layout.top_bar_y, layout.top_bar_w, layout.top_bar_h});
        const WidgetHandle lib_title = builder.create_label_static("Library");
        anchor_rect(builder, lib_title, {layout.top_bar_x + 8, layout.top_bar_y + 8,
                                         layout.top_bar_w - 56, 24});
        const WidgetHandle lib_settings = builder.create_button_static("");
        anchor_rect(builder, lib_settings, {layout.top_bar_x + layout.top_bar_w - 44, layout.top_bar_y, 40, 40});
        builder.set_button_icon(lib_settings, icons.settings);
        builder.set_button_icon_size(lib_settings, 14);
        apply_top_bar_button_style(builder, lib_settings);

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

        {
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
        }

        {
            const Rect nav_rect{kUiPadding, layout.nav_y,
                                screen_width - kUiPadding * 2, layout.nav_h};
            ::ui::scene::RowBuilder nav_row{builder, nav_rect, layout.nav_gap, kNavPadding,
                                            ::ui::scene::LayoutAlign::Center};
            h.nav_bar = nav_row.root();
            const int nav_w = nav_rect.w;
            const int nav_btn_w = (nav_w - layout.nav_gap * 2) / 3;
            nav_row.add(h.nav_home, nav_btn_w, layout.nav_h);
            nav_row.add(h.nav_search, nav_btn_w, layout.nav_h);
            nav_row.add(h.nav_library, nav_btn_w, layout.nav_h);
            builder.set_button_icon(h.nav_home, icons.home);
            builder.set_button_icon_size(h.nav_home, 18);
            builder.set_button_icon(h.nav_search, icons.search);
            builder.set_button_icon_size(h.nav_search, 18);
            builder.set_button_icon(h.nav_library, icons.folder);
            builder.set_button_icon_size(h.nav_library, 18);
        }

        builder.link(h.root, h.bottom_bar);
        builder.set_hit_testable(h.bottom_bar, true);
        builder.link(h.root, h.nav_bar);
    }

} // namespace player::ui_builder_detail

export namespace player {
    using namespace player::ui;

    UiHandles build_ui(::ui::scene::SceneBuilder& builder, PlayerController& ctx, const PlayerIconIds& icons) {
        (void)ctx;

        UiHandles h{};
        h.root = builder.create_container();
        ui_builder_detail::anchor_rect(builder, h.root, {0, 0, screen_width, screen_height});
        builder.set_input_root(h.root);

        const ui_builder_detail::UiLayout layout = ui_builder_detail::make_layout();
        const ui_builder_detail::NowTextLayout now_text = ui_builder_detail::make_now_text_layout(layout);
        ui_builder_detail::build_home(builder, h, layout, icons);
        if (h.page_home) {
            builder.link(h.root, h.page_home);
        }
        ui_builder_detail::build_now_playing(builder, h, layout, now_text, icons);
        ui_builder_detail::build_library(builder, h, layout, icons);
#if CHARM_PLAYER_DEBUG_UI
        const int debug_h = 18;
        const int debug_y = layout.controls_y - debug_h - 6;
        h.debug_text = builder.create_label_static("");
        ui_builder_detail::anchor_rect(builder, h.debug_text, {kUiPadding, debug_y,
                                                               screen_width - kUiPadding * 2, debug_h});
        builder.link(h.root, h.debug_text);
#endif

        builder.set_root(h.root);
        return h;
    }
}
