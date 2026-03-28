module;
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

    UiHandles build_ui(::ui::scene::SceneBuilder& builder, PlayerController& ctx, const PlayerIconIds& icons) {
        (void)ctx;

        auto anchor_rect = [&](WidgetHandle h, const Rect& r) { builder.set_rect(h, r); };

        UiHandles h{};
        h.root = builder.create_container();
        anchor_rect(h.root, {0, 0, screen_width, screen_height});
        builder.set_input_root(h.root);

        const int mode_w = 56;
        const int mode_h = 44;
        const int controls_w = kButtonWidth * 3 + kButtonGap * 2 + mode_w + kButtonGap;
        const int controls_x = (screen_width - controls_w) / 2;
        const int controls_y = screen_height - kControlsBottomMargin - kButtonHeight;

        const WidgetHandle scroll_area = builder.create_scroll_container();
        anchor_rect(scroll_area, {0, 0, screen_width, controls_y - 8});
        builder.set_clip_children(scroll_area, true);
        builder.set_scroll_step(scroll_area, 28);

        const int cover_top = kUiPadding * 2;
        const int header_top = cover_top + kCoverSize;
        const int title_y = header_top + 20;
        const int subtitle_y = title_y + 34;
        const int progress_y = subtitle_y + 26;
        const int time_y = progress_y + 18;
        const int content_h = time_y + 32;

        const WidgetHandle content = builder.create_container();
        anchor_rect(content, {0, 0, screen_width, content_h});
        h.cover = builder.create_image();
        anchor_rect(h.cover, {(screen_width - kCoverSize) / 2, cover_top, kCoverSize, kCoverSize});

        h.title = builder.create_label_static("");
        anchor_rect(h.title, {kUiPadding, title_y,
                              screen_width - kUiPadding * 2, 32});

        h.subtitle = builder.create_label_static("");
        anchor_rect(h.subtitle, {kUiPadding, subtitle_y,
                                 screen_width - kUiPadding * 2, 22});

        h.progress = builder.create_progress();
        anchor_rect(h.progress, {kUiPadding, progress_y,
                                 screen_width - kUiPadding * 2, 12});
        builder.set_range(h.progress, 0, 100);
        builder.set_value(h.progress, 0);
        builder.set_hit_testable(h.progress, true);

        h.time = builder.create_label_static("");
        anchor_rect(h.time, {kUiPadding, time_y,
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

        h.list_title = builder.create_label_static("");
        anchor_rect(h.list_title, {0, 0, 0, 0});

        h.list = builder.create_list_view();
        anchor_rect(h.list, {0, 0, 0, 0});
        builder.set_list_row_height(h.list, 34);
        builder.set_scroll_step(h.list, 34);

        h.list_scroll = builder.create_scrollbar_for(h.list);
        anchor_rect(h.list_scroll, {0, 0, 0, 0});
        builder.set_scrollbar_orientation(h.list_scroll, ScrollBarOrientation::Vertical);

        h.list_hint = builder.create_label_static("");
        anchor_rect(h.list_hint, {0, 0, 0, 0});

        h.controls = builder.create_container();
        anchor_rect(h.controls, {controls_x, controls_y, controls_w, kButtonHeight});

#if CHARM_PLAYER_DEBUG_UI
        const int debug_h = 18;
        const int debug_y = controls_y - debug_h - 6;
        h.debug_text = builder.create_label_static("");
        anchor_rect(h.debug_text, {kUiPadding, debug_y,
                                   screen_width - kUiPadding * 2, debug_h});
#endif

        h.btn_prev = builder.create_button_static("");
        anchor_rect(h.btn_prev, {0, 0, kButtonWidth, kButtonHeight});
        builder.set_button_icon(h.btn_prev, icons.prev);
        builder.set_button_icon_size(h.btn_prev, 16);

        h.btn_pause = builder.create_button_static("");
        anchor_rect(h.btn_pause, {kButtonWidth + kButtonGap, 0,
                                  kButtonWidth, kButtonHeight});
        builder.set_button_icon(h.btn_pause, icons.play);
        builder.set_button_icon_size(h.btn_pause, 16);

        h.btn_next = builder.create_button_static("");
        anchor_rect(h.btn_next, {(kButtonWidth + kButtonGap) * 2, 0,
                                 kButtonWidth, kButtonHeight});
        builder.set_button_icon(h.btn_next, icons.next);
        builder.set_button_icon_size(h.btn_next, 16);

        h.btn_mode = builder.create_button_static("");
        anchor_rect(h.btn_mode, {kButtonWidth * 3 + kButtonGap * 3, 6,
                                 mode_w, mode_h});
        builder.set_button_icon(h.btn_mode, icons.loop);
        builder.set_button_icon_size(h.btn_mode, 16);

        builder.link(h.root, scroll_area);
        builder.link(scroll_area, content);
        builder.link(content, h.cover);
        builder.link(content, h.title);
        builder.link(content, h.subtitle);
        builder.link(content, h.progress);
        builder.link(content, h.time);
        builder.link(content, h.status);
        builder.link(content, h.mode_hint);
        builder.link(content, h.spectrum);
        builder.link(content, h.eq_panel);
        builder.link(content, h.eq_title);
        for (std::size_t i = 0; i < kEqBands; ++i) {
            builder.link(content, h.eq_labels[i]);
            builder.link(content, h.eq_sliders[i]);
            builder.link(content, h.eq_values[i]);
        }
        builder.link(content, h.volume_label);
        builder.link(content, h.volume_slider);
        builder.link(content, h.volume_value);
        builder.link(content, h.dc_label);
        builder.link(content, h.dc_switch);
        builder.link(content, h.clip_label);
        builder.link(content, h.clip_switch);
        builder.link(content, h.clip_slider);
        builder.link(content, h.clip_value);
        builder.link(content, h.list_title);
        builder.link(content, h.list);
        builder.link(content, h.list_scroll);
        builder.link(content, h.list_hint);
        builder.link(h.root, h.controls);
        builder.link(h.controls, h.btn_prev);
        builder.link(h.controls, h.btn_pause);
        builder.link(h.controls, h.btn_next);
        builder.link(h.controls, h.btn_mode);
#if CHARM_PLAYER_DEBUG_UI
        builder.link(h.root, h.debug_text);
#endif

        builder.set_root(h.root);
        return h;
    }
}
