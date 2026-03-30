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
        const int controls_y = text_col_y + text_col_h + 30;
        const int control_side = 92;
        const int control_play = 100;
        const int control_gap = 16;
        const int controls_w = control_side * 2 + control_play + control_gap * 2;
        const int controls_x = (screen_width - controls_w) / 2;

        const int tabs_y = top_bar_y + top_bar_h + 10;
        const int tabs_h = 34;
        const int tabs_gap = 10;
        const int tabs_w = screen_width - kUiPadding * 2;
        const int tab_w = (tabs_w - tabs_gap * 2) / 3;

        const int shuffle_y = tabs_y + tabs_h + 10;
        const int shuffle_h = 34;
        const int list_card_y = shuffle_y + shuffle_h + 10;
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
        patch.shadow_offset_y = 4;
        patch.has_shadow_spread = true;
        patch.shadow_spread = 4;
        patch.has_shadow_radius = true;
        patch.shadow_radius = 18;
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

    static void build_now_playing(::ui::scene::SceneBuilder& builder, UiHandles& h,
                                  const UiLayout& layout, const NowTextLayout& text_layout,
                                  const PlayerIconIds& icons) {
        const int title_h = text_layout.title_h;
        const int subtitle_h = text_layout.subtitle_h;
        const int progress_h = text_layout.progress_h;
        const int time_row_h = text_layout.time_row_h;
        const int time_row_y = text_layout.time_row_y;
        const int text_col_y = text_layout.text_col_y;
        const int text_col_h = text_layout.text_col_h;
        const int text_gap = text_layout.text_gap;

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

        constexpr int control_side = 92;
        constexpr int control_play = 100;
        h.controls = builder.create_container();
        anchor_rect(builder, h.controls, {layout.controls_x, layout.controls_y, layout.controls_w, control_play});

        const int small_btn = control_side;
        const int play_btn = control_play;
        const int btn_gap = 16;
        const Rect controls_rect{0, 0, layout.controls_w, control_play};
        auto controls_row = ::ui::scene::make_row(builder, controls_rect, btn_gap, 0,
                                                  ::ui::scene::LayoutAlign::Center);

        h.btn_prev = builder.create_button_static("");
        controls_row.place(h.btn_prev, small_btn, small_btn);
        builder.set_button_icon(h.btn_prev, icons.prev);
        builder.set_button_icon_size(h.btn_prev, 22);
        apply_control_side_style(builder, h.btn_prev, small_btn, 22);

        h.btn_pause = builder.create_button_static("");
        controls_row.place(h.btn_pause, play_btn, play_btn);
        builder.set_button_icon(h.btn_pause, icons.play);
        builder.set_button_icon_size(h.btn_pause, 26);
        apply_control_play_style(builder, h.btn_pause, play_btn, 26);

        h.btn_next = builder.create_button_static("");
        controls_row.place(h.btn_next, small_btn, small_btn);
        builder.set_button_icon(h.btn_next, icons.next);
        builder.set_button_icon_size(h.btn_next, 22);
        apply_control_side_style(builder, h.btn_next, small_btn, 22);

        h.btn_mode = builder.create_button_static("");
        anchor_rect(builder, h.btn_mode, {0, 0, 0, 0});

        {
            const Rect text_rect{kUiPadding, text_col_y,
                                 screen_width - kUiPadding * 2, text_col_h};
            auto text_col = ::ui::scene::make_column(builder, text_rect, text_gap, 0,
                                                     ::ui::scene::LayoutAlign::Start);
            text_col.place(h.title, text_rect.w, title_h);
            text_col.place(h.subtitle, text_rect.w, subtitle_h);
            text_col.place(h.progress, text_rect.w, progress_h);
        }
        {
            const int time_row_w = screen_width - kUiPadding * 2;
            const int time_row_x = kUiPadding;
            const int time_side_w = 52;
            const int info_w = std::max(170, time_row_w - time_side_w * 2 - 16);
            const int info_x = time_row_x + (time_row_w - info_w) / 2;
            anchor_rect(builder, h.time_left, {time_row_x, time_row_y, time_side_w, time_row_h});
            anchor_rect(builder, h.time_right,
                        {time_row_x + time_row_w - time_side_w, time_row_y, time_side_w, time_row_h});
            anchor_rect(builder, h.info_tag, {info_x, time_row_y, info_w, time_row_h});
        }

#if defined(CHARM_PLAYER_COVER_DEBUG)
        h.cover_debug = builder.create_label_static("");
        apply_time_label_style(builder, h.cover_debug);
        builder.set_label_align(h.cover_debug, ::ui::scene::TextAlignH::Left, ::ui::scene::TextAlignV::Center);
        anchor_rect(builder, h.cover_debug, {kUiPadding, time_row_y + time_row_h + 4,
                                             screen_width - kUiPadding * 2, 12});
        builder.set_hit_testable(h.cover_debug, false);
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
        builder.set_button_icon(top_more, icons.shuffle);
        builder.set_button_icon_size(top_more, 14);
        apply_top_bar_button_style(builder, top_more);
        h.now_more = top_more;

        builder.link(h.root, now_playing);
        builder.link(now_playing, h.now_backdrop);
        builder.link(now_playing, top_bar);
        builder.link(top_bar, h.now_back);
        builder.link(top_bar, top_title);
        builder.link(top_bar, top_more);

        builder.link(now_playing, h.cover);
        if (kShowCoverCollage) {
            builder.link(now_playing, h.cover_left);
            builder.link(now_playing, h.cover_right);
        }
        builder.link(now_playing, h.title);
        builder.link(now_playing, h.subtitle);
        builder.link(now_playing, h.progress);
        builder.link(now_playing, h.time_left);
        builder.link(now_playing, h.time_right);
        builder.link(now_playing, h.info_tag);
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
        builder.link(h.controls, h.btn_prev);
        builder.link(h.controls, h.btn_pause);
        builder.link(h.controls, h.btn_next);
        builder.link(h.controls, h.btn_mode);
        builder.link(now_playing, h.controls);
    }

    static void build_library(::ui::scene::SceneBuilder& builder, UiHandles& h,
                              const UiLayout& layout, const PlayerIconIds& icons) {
        h.list_title = builder.create_label_static("");
        anchor_rect(builder, h.list_title, {0, 0, 0, 0});

        h.list = builder.create_list_view();
        anchor_rect(builder, h.list, {0, 0, 0, 0});
        builder.set_list_row_height(h.list, 40);
        builder.set_scroll_step(h.list, 40);
        apply_list_card_style(builder, h.list);

        h.list_scroll = builder.create_scrollbar_for(h.list);
        anchor_rect(builder, h.list_scroll, {0, 0, 0, 0});
        builder.set_scrollbar_orientation(h.list_scroll, ScrollBarOrientation::Vertical);

        h.list_hint = builder.create_label_static("");
        anchor_rect(builder, h.list_hint, {0, 0, 0, 0});

        h.bottom_bar = builder.create_container();
        anchor_rect(builder, h.bottom_bar, {0, 0, 0, 0});
        apply_bottom_bar_style(builder, h.bottom_bar);
        h.bottom_cover = builder.create_image();
        anchor_rect(builder, h.bottom_cover, {0, 0, 0, 0});
        h.bottom_title = builder.create_label_static("");
        anchor_rect(builder, h.bottom_title, {0, 0, 0, 0});
        h.bottom_subtitle = builder.create_label_static("");
        anchor_rect(builder, h.bottom_subtitle, {0, 0, 0, 0});
        h.bottom_play = builder.create_button_static("");
        anchor_rect(builder, h.bottom_play, {0, 0, 0, 0});

        h.nav_bar = builder.create_container();
        anchor_rect(builder, h.nav_bar, {0, 0, 0, 0});
        h.nav_home = builder.create_button_static("Home");
        anchor_rect(builder, h.nav_home, {0, 0, 0, 0});
        h.nav_search = builder.create_button_static("Search");
        anchor_rect(builder, h.nav_search, {0, 0, 0, 0});
        h.nav_library = builder.create_button_static("Library");
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
        builder.set_button_icon(lib_settings, icons.loop);
        builder.set_button_icon_size(lib_settings, 14);
        apply_top_bar_button_style(builder, lib_settings);

        const WidgetHandle tab_songs = builder.create_button_static("Songs");
        anchor_rect(builder, tab_songs, {kUiPadding, layout.tabs_y, layout.tab_w, layout.tabs_h});
        const WidgetHandle tab_albums = builder.create_button_static("Albums");
        anchor_rect(builder, tab_albums, {kUiPadding + layout.tab_w + layout.tabs_gap, layout.tabs_y,
                                          layout.tab_w, layout.tabs_h});
        const WidgetHandle tab_artist = builder.create_button_static("Artist");
        anchor_rect(builder, tab_artist, {kUiPadding + (layout.tab_w + layout.tabs_gap) * 2,
                                          layout.tabs_y, layout.tab_w, layout.tabs_h});

        const WidgetHandle shuffle_btn = builder.create_button_static("Shuffle");
        anchor_rect(builder, shuffle_btn, {kUiPadding, layout.shuffle_y, 120, layout.shuffle_h});
        const int tab_radius = layout.tabs_h / 2;
        apply_tab_base_style(builder, tab_songs, tab_radius);
        apply_tab_base_style(builder, tab_albums, tab_radius);
        apply_tab_base_style(builder, tab_artist, tab_radius);
        apply_tab_base_style(builder, shuffle_btn, tab_radius);
        apply_tab_active_style(builder, tab_songs);
        apply_shuffle_shadow_style(builder, shuffle_btn);

        const Rect list_card_rect{kUiPadding, layout.list_card_y,
                                  screen_width - kUiPadding * 2, layout.list_card_h};

        builder.link(h.root, library);
        builder.link(library, lib_top);
        builder.link(lib_top, lib_title);
        builder.link(lib_top, lib_settings);
        builder.link(library, tab_songs);
        builder.link(library, tab_albums);
        builder.link(library, tab_artist);
        builder.link(library, shuffle_btn);
        anchor_rect(builder, h.list_title, {list_card_rect.x + 12, list_card_rect.y + 8,
                                            list_card_rect.w - 24, 18});
        anchor_rect(builder, h.list, {list_card_rect.x + 8,
                                      list_card_rect.y + 30,
                                      list_card_rect.w - 16,
                                      list_card_rect.h - 42});
        anchor_rect(builder, h.list_scroll, {list_card_rect.x + list_card_rect.w - 10,
                                             list_card_rect.y + 30, 8,
                                             list_card_rect.h - 42});
        anchor_rect(builder, h.list_hint, {list_card_rect.x + 12,
                                           list_card_rect.y + list_card_rect.h - 22,
                                           list_card_rect.w - 24, 18});

        builder.link(library, h.list);
        builder.link(library, h.list_scroll);
        builder.link(library, h.list_title);
        builder.link(library, h.list_hint);

        anchor_rect(builder, h.bottom_bar, {kUiPadding, layout.bottom_bar_y,
                                            screen_width - kUiPadding * 2, layout.bottom_bar_h});
        anchor_rect(builder, h.bottom_cover, {kUiPadding + 12, layout.bottom_bar_y + 12, 48, 48});
        anchor_rect(builder, h.bottom_title, {kUiPadding + 72, layout.bottom_bar_y + 10,
                                              screen_width - kUiPadding * 2 - 140, 22});
        anchor_rect(builder, h.bottom_subtitle, {kUiPadding + 72, layout.bottom_bar_y + 34,
                                                 screen_width - kUiPadding * 2 - 140, 18});
        anchor_rect(builder, h.bottom_play, {screen_width - kUiPadding - 64, layout.bottom_bar_y + 14, 48, 48});
        builder.set_button_icon(h.bottom_play, icons.play);
        builder.set_button_icon_size(h.bottom_play, 18);

        anchor_rect(builder, h.nav_bar, {kUiPadding, layout.nav_y,
                                         screen_width - kUiPadding * 2, layout.nav_h});
        const int nav_w = screen_width - kUiPadding * 2;
        const int nav_btn_w = (nav_w - layout.nav_gap * 2) / 3;
        anchor_rect(builder, h.nav_home, {kUiPadding, layout.nav_y, nav_btn_w, layout.nav_h});
        anchor_rect(builder, h.nav_search, {kUiPadding + nav_btn_w + layout.nav_gap,
                                            layout.nav_y, nav_btn_w, layout.nav_h});
        anchor_rect(builder, h.nav_library, {kUiPadding + (nav_btn_w + layout.nav_gap) * 2,
                                             layout.nav_y, nav_btn_w, layout.nav_h});

        builder.link(library, h.bottom_bar);
        builder.set_hit_testable(h.bottom_bar, true);
        builder.link(h.bottom_bar, h.bottom_cover);
        builder.link(h.bottom_bar, h.bottom_title);
        builder.link(h.bottom_bar, h.bottom_subtitle);
        builder.link(h.bottom_bar, h.bottom_play);
        builder.link(library, h.nav_bar);
        builder.link(h.nav_bar, h.nav_home);
        builder.link(h.nav_bar, h.nav_search);
        builder.link(h.nav_bar, h.nav_library);
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
