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

export namespace player {
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

    UiHandles build_ui(::ui::scene::SceneBuilder& builder, PlayerController& ctx, const PlayerIconIds& icons) {
        (void)ctx;

        auto anchor_rect = [&](WidgetHandle h, const Rect& r) { builder.set_rect(h, r); };

        UiHandles h{};
        h.root = builder.create_container();
        anchor_rect(h.root, {0, 0, screen_width, screen_height});
        builder.set_input_root(h.root);

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

        const int cover_top = top_bar_y + top_bar_h + 18;
        const int cover_x = (screen_width - kCoverSize) / 2;
        const int cover_y = cover_top;
        const int header_top = cover_top + kCoverSize;
        const int title_y = header_top + 18;
        const int subtitle_y = title_y + 30;
        const int progress_y = subtitle_y + 30;
        const int time_y = progress_y + 30;

        const int controls_y = time_y + 40;
        const int controls_w = 56 * 2 + 96 + 16 * 2;
        const int controls_x = (screen_width - controls_w) / 2;

        const int tabs_y = top_bar_y + top_bar_h + 6;
        const int tabs_h = 32;
        const int tabs_gap = 8;
        const int tabs_w = screen_width - kUiPadding * 2;
        const int tab_w = (tabs_w - tabs_gap * 2) / 3;

        const int shuffle_y = tabs_y + tabs_h + 6;
        const int shuffle_h = 32;
        const int list_card_y = shuffle_y + shuffle_h + 6;
        const int list_card_h = std::max(220, bottom_bar_y - list_card_y - 20);

        const UiLayout layout{
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
            .title_y = title_y,
            .subtitle_y = subtitle_y,
            .progress_y = progress_y,
            .time_y = time_y,
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

        auto build_now_playing = [&] {
            h.cover = builder.create_image();
            anchor_rect(h.cover, {layout.cover_x, layout.cover_y, kCoverSize, kCoverSize});

            const int collage_small = 88;
            const int collage_tiny = 68;
            h.cover_left = builder.create_image();
            anchor_rect(h.cover_left, {layout.cover_x - 12,
                                       layout.cover_y + kCoverSize - collage_small - 10,
                                       collage_small, collage_small});
            h.cover_right = builder.create_image();
            anchor_rect(h.cover_right, {layout.cover_x + kCoverSize - collage_tiny + 8,
                                        layout.cover_y + kCoverSize / 2 + 12,
                                        collage_tiny, collage_tiny});

            h.title = builder.create_label_static("");
            anchor_rect(h.title, {kUiPadding, layout.title_y,
                                  screen_width - kUiPadding * 2, 32});

            h.subtitle = builder.create_label_static("");
            anchor_rect(h.subtitle, {kUiPadding, layout.subtitle_y,
                                     screen_width - kUiPadding * 2, 22});

            h.progress = builder.create_progress();
            anchor_rect(h.progress, {kUiPadding, layout.progress_y,
                                     screen_width - kUiPadding * 2, 20});
            builder.set_range(h.progress, 0, 100);
            builder.set_value(h.progress, 0);
            builder.set_hit_testable(h.progress, true);

            h.time = builder.create_label_static("");
            anchor_rect(h.time, {kUiPadding, layout.time_y,
                                 screen_width - kUiPadding * 2, 18});

            h.status = builder.create_label_static("");
            anchor_rect(h.status, {0, 0, 0, 0});

            h.mode_hint = builder.create_label_static("");
            anchor_rect(h.mode_hint, {0, 0, 0, 0});

            h.spectrum = builder.create_container();
            anchor_rect(h.spectrum, {0, 0, 0, 0});
            builder.set_hit_testable(h.spectrum, false);

            h.eq_panel = builder.create_container();
            anchor_rect(h.eq_panel, {0, 0, 0, 0});
            builder.set_hit_testable(h.eq_panel, false);

            h.eq_title = builder.create_label_static("EQ");
            anchor_rect(h.eq_title, {0, 0, 0, 0});

            constexpr std::array<const char*, kEqBands> kEqLabels{
                "60", "250", "1K", "4K", "16K"
            };
            for (std::size_t i = 0; i < kEqBands; ++i) {
                const int row_y = 0;
                h.eq_labels[i] = builder.create_label_static(kEqLabels[i]);
                anchor_rect(h.eq_labels[i], {0, row_y, 0, 0});

                const int slider_x = 0;
                const int slider_w = 0;
                h.eq_sliders[i] = builder.create_slider();
                anchor_rect(h.eq_sliders[i], {slider_x, row_y, slider_w, 0});
                builder.set_range(h.eq_sliders[i], -12, 12);
                builder.set_value(h.eq_sliders[i], 0);

                h.eq_values[i] = builder.create_label_static("");
                anchor_rect(h.eq_values[i], {0, row_y, 0, 0});
            }

            const int vol_y = 0;
            h.volume_label = builder.create_label_static("Vol");
            anchor_rect(h.volume_label, {0, vol_y, 0, 0});
            const int vol_slider_x = 0;
            const int vol_slider_w = 0;
            h.volume_slider = builder.create_slider();
            anchor_rect(h.volume_slider, {vol_slider_x, vol_y, vol_slider_w, 0});
            builder.set_range(h.volume_slider, 0, 100);
            builder.set_value(h.volume_slider, 80);
            h.volume_value = builder.create_label_static("");
            anchor_rect(h.volume_value, {0, vol_y, 0, 0});

            const int dc_y = 0;
            h.dc_label = builder.create_label_static("DC");
            anchor_rect(h.dc_label, {0, dc_y, 0, 0});
            h.dc_switch = builder.create_switch();
            anchor_rect(h.dc_switch, {0, dc_y, 0, 0});
            builder.set_checked(h.dc_switch, true);

            const int clip_y = 0;
            h.clip_label = builder.create_label_static("Clip");
            anchor_rect(h.clip_label, {0, clip_y, 0, 0});
            h.clip_switch = builder.create_switch();
            anchor_rect(h.clip_switch, {0, clip_y, 0, 0});
            builder.set_checked(h.clip_switch, true);
            const int clip_slider_x = 0;
            const int clip_slider_w = 0;
            h.clip_slider = builder.create_slider();
            anchor_rect(h.clip_slider, {clip_slider_x, clip_y, clip_slider_w, 0});
            builder.set_range(h.clip_slider, 60, 100);
            builder.set_value(h.clip_slider, 85);
            h.clip_value = builder.create_label_static("");
            anchor_rect(h.clip_value, {0, clip_y, 0, 0});

            h.controls = builder.create_container();
            anchor_rect(h.controls, {layout.controls_x, layout.controls_y, layout.controls_w, 72});

            const int small_btn = 56;
            const int play_btn = 96;
            const int btn_gap = 16;
            const int prev_x = 0;
            const int play_x = prev_x + small_btn + btn_gap;
            const int next_x = play_x + play_btn + btn_gap;

            h.btn_prev = builder.create_button_static("");
            anchor_rect(h.btn_prev, {prev_x, 8, small_btn, small_btn});
            builder.set_button_icon(h.btn_prev, icons.prev);
            builder.set_button_icon_size(h.btn_prev, 18);

            h.btn_pause = builder.create_button_static("");
            anchor_rect(h.btn_pause, {play_x, -4, play_btn, play_btn});
            builder.set_button_icon(h.btn_pause, icons.play);
            builder.set_button_icon_size(h.btn_pause, 22);
            {
                StylePatch patch{};
                patch.has_bg_color = true;
                patch.bg_color = kUiPlayBg;
                patch.has_border_color = true;
                patch.border_color = kUiButtonBorder;
                patch.has_corner_radius = true;
                patch.corner_radius = 22;
                builder.set_style_patch(h.btn_pause, patch);
            }

            h.btn_next = builder.create_button_static("");
            anchor_rect(h.btn_next, {next_x, 8, small_btn, small_btn});
            builder.set_button_icon(h.btn_next, icons.next);
            builder.set_button_icon_size(h.btn_next, 18);

            h.btn_mode = builder.create_button_static("");
            anchor_rect(h.btn_mode, {0, 0, 0, 0});

            const WidgetHandle now_playing = builder.create_container();
            anchor_rect(now_playing, {0, 0, screen_width, screen_height});
            h.page_now_playing = now_playing;

            const WidgetHandle top_bar = builder.create_container();
            anchor_rect(top_bar, {layout.top_bar_x, layout.top_bar_y, layout.top_bar_w, layout.top_bar_h});
            h.now_back = builder.create_button_static("");
            anchor_rect(h.now_back, {layout.top_bar_x, layout.top_bar_y, 40, 40});
            builder.set_button_icon(h.now_back, icons.prev);
            builder.set_button_icon_size(h.now_back, 16);
            const WidgetHandle top_title = builder.create_label_static("Now Playing");
            anchor_rect(top_title, {layout.top_bar_x + 48, layout.top_bar_y + 8,
                                    layout.top_bar_w - 96, 24});
            const WidgetHandle top_more = builder.create_button_static("");
            anchor_rect(top_more, {layout.top_bar_x + layout.top_bar_w - 40, layout.top_bar_y, 40, 40});
            builder.set_button_icon(top_more, icons.shuffle);
            builder.set_button_icon_size(top_more, 14);

            builder.link(h.root, now_playing);
            builder.link(now_playing, top_bar);
            builder.link(top_bar, h.now_back);
            builder.link(top_bar, top_title);
            builder.link(top_bar, top_more);

            builder.link(now_playing, h.cover);
            builder.link(now_playing, h.cover_left);
            builder.link(now_playing, h.cover_right);
            builder.link(now_playing, h.title);
            builder.link(now_playing, h.subtitle);
            builder.link(now_playing, h.progress);
            builder.link(now_playing, h.time);
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
        };

        auto build_library = [&] {
            h.list_title = builder.create_label_static("");
            anchor_rect(h.list_title, {0, 0, 0, 0});

            h.list = builder.create_list_view();
            anchor_rect(h.list, {0, 0, 0, 0});
            builder.set_list_row_height(h.list, 34);
            builder.set_scroll_step(h.list, 34);
            {
                StylePatch patch{};
                patch.has_bg_color = true;
                patch.bg_color = kUiListBg;
                patch.has_border_color = true;
                patch.border_color = kUiListBorder;
                patch.has_corner_radius = true;
                patch.corner_radius = 14;
                builder.set_style_patch(h.list, patch);
            }

            h.list_scroll = builder.create_scrollbar_for(h.list);
            anchor_rect(h.list_scroll, {0, 0, 0, 0});
            builder.set_scrollbar_orientation(h.list_scroll, ScrollBarOrientation::Vertical);

            h.list_hint = builder.create_label_static("");
            anchor_rect(h.list_hint, {0, 0, 0, 0});

            h.bottom_bar = builder.create_container();
            anchor_rect(h.bottom_bar, {0, 0, 0, 0});
            h.bottom_cover = builder.create_image();
            anchor_rect(h.bottom_cover, {0, 0, 0, 0});
            h.bottom_title = builder.create_label_static("");
            anchor_rect(h.bottom_title, {0, 0, 0, 0});
            h.bottom_subtitle = builder.create_label_static("");
            anchor_rect(h.bottom_subtitle, {0, 0, 0, 0});
            h.bottom_play = builder.create_button_static("");
            anchor_rect(h.bottom_play, {0, 0, 0, 0});

            h.nav_bar = builder.create_container();
            anchor_rect(h.nav_bar, {0, 0, 0, 0});
            h.nav_home = builder.create_button_static("Home");
            anchor_rect(h.nav_home, {0, 0, 0, 0});
            h.nav_search = builder.create_button_static("Search");
            anchor_rect(h.nav_search, {0, 0, 0, 0});
            h.nav_library = builder.create_button_static("Library");
            anchor_rect(h.nav_library, {0, 0, 0, 0});

            const WidgetHandle library = builder.create_container();
            anchor_rect(library, {0, 0, screen_width, screen_height});
            h.page_library = library;

            const WidgetHandle lib_top = builder.create_container();
            anchor_rect(lib_top, {layout.top_bar_x, layout.top_bar_y, layout.top_bar_w, layout.top_bar_h});
            const WidgetHandle lib_title = builder.create_label_static("Library");
            anchor_rect(lib_title, {layout.top_bar_x + 8, layout.top_bar_y + 8,
                                    layout.top_bar_w - 56, 24});
            const WidgetHandle lib_settings = builder.create_button_static("");
            anchor_rect(lib_settings, {layout.top_bar_x + layout.top_bar_w - 44, layout.top_bar_y, 40, 40});
            builder.set_button_icon(lib_settings, icons.loop);
            builder.set_button_icon_size(lib_settings, 14);

            const WidgetHandle tab_songs = builder.create_button_static("Songs");
            anchor_rect(tab_songs, {kUiPadding, layout.tabs_y, layout.tab_w, layout.tabs_h});
            const WidgetHandle tab_albums = builder.create_button_static("Albums");
            anchor_rect(tab_albums, {kUiPadding + layout.tab_w + layout.tabs_gap, layout.tabs_y,
                                     layout.tab_w, layout.tabs_h});
            const WidgetHandle tab_artist = builder.create_button_static("Artist");
            anchor_rect(tab_artist, {kUiPadding + (layout.tab_w + layout.tabs_gap) * 2,
                                     layout.tabs_y, layout.tab_w, layout.tabs_h});

            const WidgetHandle shuffle_btn = builder.create_button_static("Shuffle");
            anchor_rect(shuffle_btn, {kUiPadding, layout.shuffle_y, 120, layout.shuffle_h});
            {
                StylePatch patch{};
                patch.has_bg_color = true;
                patch.bg_color = kUiButtonBg;
                patch.has_border_color = true;
                patch.border_color = kUiButtonBorder;
                patch.has_corner_radius = true;
                patch.corner_radius = 12;
                builder.set_style_patch(tab_songs, patch);
                builder.set_style_patch(tab_albums, patch);
                builder.set_style_patch(tab_artist, patch);
                builder.set_style_patch(shuffle_btn, patch);
            }

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
            anchor_rect(h.list_title, {list_card_rect.x + 12, list_card_rect.y + 8,
                                       list_card_rect.w - 24, 18});
            anchor_rect(h.list, {list_card_rect.x + 8,
                                 list_card_rect.y + 30,
                                 list_card_rect.w - 16,
                                 list_card_rect.h - 42});
            anchor_rect(h.list_scroll, {list_card_rect.x + list_card_rect.w - 10,
                                        list_card_rect.y + 30, 8,
                                        list_card_rect.h - 42});
            anchor_rect(h.list_hint, {list_card_rect.x + 12,
                                      list_card_rect.y + list_card_rect.h - 22,
                                      list_card_rect.w - 24, 18});

            builder.link(library, h.list);
            builder.link(library, h.list_scroll);
            builder.link(library, h.list_title);
            builder.link(library, h.list_hint);

            anchor_rect(h.bottom_bar, {kUiPadding, layout.bottom_bar_y,
                                       screen_width - kUiPadding * 2, layout.bottom_bar_h});
            anchor_rect(h.bottom_cover, {kUiPadding + 12, layout.bottom_bar_y + 12, 48, 48});
            anchor_rect(h.bottom_title, {kUiPadding + 72, layout.bottom_bar_y + 10,
                                         screen_width - kUiPadding * 2 - 140, 22});
            anchor_rect(h.bottom_subtitle, {kUiPadding + 72, layout.bottom_bar_y + 34,
                                            screen_width - kUiPadding * 2 - 140, 18});
            anchor_rect(h.bottom_play, {screen_width - kUiPadding - 64, layout.bottom_bar_y + 14, 48, 48});
            builder.set_button_icon(h.bottom_play, icons.play);
            builder.set_button_icon_size(h.bottom_play, 18);

            anchor_rect(h.nav_bar, {kUiPadding, layout.nav_y,
                                    screen_width - kUiPadding * 2, layout.nav_h});
            const int nav_w = screen_width - kUiPadding * 2;
            const int nav_btn_w = (nav_w - layout.nav_gap * 2) / 3;
            anchor_rect(h.nav_home, {kUiPadding, layout.nav_y, nav_btn_w, layout.nav_h});
            anchor_rect(h.nav_search, {kUiPadding + nav_btn_w + layout.nav_gap,
                                       layout.nav_y, nav_btn_w, layout.nav_h});
            anchor_rect(h.nav_library, {kUiPadding + (nav_btn_w + layout.nav_gap) * 2,
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
        };

        build_now_playing();
        build_library();
#if CHARM_PLAYER_DEBUG_UI
        const int debug_h = 18;
        const int debug_y = controls_y - debug_h - 6;
        h.debug_text = builder.create_label_static("");
        anchor_rect(h.debug_text, {kUiPadding, debug_y,
                                   screen_width - kUiPadding * 2, debug_h});
        builder.link(h.root, h.debug_text);
#endif

        builder.set_root(h.root);
        return h;
    }
}
