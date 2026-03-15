module;
#include <array>
#include <cstdint>

export module player.ui_builder;

import charm.core.config;
import charm.core.geometry;
import charm.core.soa_factory;
import charm.core.soa_kernel;
import player.controller;
import player.ui;

export namespace player {
    using namespace player::ui;

    UiHandles build_ui(SoaFactory& factory, PlayerController& ctx, const PlayerIconIds& icons) {
        (void)ctx;
        auto& kernel = factory.kernel();

        auto anchor_rect = [&](WidgetHandle h, const Rect& r) {
            kernel.set_rect(h, r);
        };

        UiHandles h{};
        h.root = factory.create_container();
        anchor_rect(h.root, {0, 0, screen_width, screen_height});
        kernel.set_input_root(h.root);

        const int controls_w = kButtonWidth * 3 + kButtonGap * 2 + kModeButtonWidth;
        const int controls_x = (screen_width - controls_w) / 2;
        const int controls_y = screen_height - kControlsBottomMargin - kButtonHeight;

        const WidgetHandle scroll_area = factory.create_scroll_container();
        anchor_rect(scroll_area, {0, 0, screen_width, controls_y});
        kernel.set_clip_children(scroll_area, true);
        kernel.set_scroll_step(scroll_area, 28);

        const int cover_top = kUiPadding * 2;
        const int header_top = cover_top + kCoverSize;
        const int mode_y = header_top + kHeaderModeOffset;
        const int mode_hint_y = mode_y + kModeHeight + kModeHintGap;
        const int spectrum_y = mode_hint_y + kModeHintHeight + kSpectrumGap;
        const int eq_y = spectrum_y + kSpectrumHeight + kSpectrumGap;
        const int list_y = eq_y + kEqPanelHeight + kSpectrumGap + kListTitleGap;
        const int list_h = 320;
        const int content_h = list_y + list_h + kControlsBottomMargin;

        const WidgetHandle content = factory.create_container();
        anchor_rect(content, {0, 0, screen_width, content_h});
        h.cover = factory.create_container();
        anchor_rect(h.cover, {(screen_width - kCoverSize) / 2, cover_top, kCoverSize, kCoverSize});

        h.title = factory.create_label_static("");
        anchor_rect(h.title, {kUiPadding, header_top + kHeaderTitleOffset,
                              screen_width - kUiPadding * 2, 24});

        h.subtitle = factory.create_label_static("");
        anchor_rect(h.subtitle, {kUiPadding, header_top + kHeaderSubtitleOffset,
                                 screen_width - kUiPadding * 2, 20});

        h.progress = factory.create_progress();
        anchor_rect(h.progress, {kUiPadding, header_top + kHeaderProgressOffset,
                                 screen_width - kUiPadding * 2, 16});
        kernel.set_range(h.progress, 0, 100);
        kernel.set_value(h.progress, 0);
        kernel.set_hit_testable(h.progress, true);

        h.time = factory.create_label_static("");
        anchor_rect(h.time, {kUiPadding, header_top + kHeaderTimeOffset,
                             screen_width - kUiPadding * 2, 18});

        h.status = factory.create_label_static("");
        anchor_rect(h.status, {kUiPadding, header_top + kHeaderStatusOffset,
                               screen_width - kUiPadding * 2, 18});

        h.mode_hint = factory.create_label_static("");
        anchor_rect(h.mode_hint, {kUiPadding, mode_hint_y,
                                  screen_width - kUiPadding * 2, kModeHintHeight});

        h.spectrum = factory.create_container();
        anchor_rect(h.spectrum, {kUiPadding, spectrum_y,
                                 screen_width - kUiPadding * 2, kSpectrumHeight});
        kernel.set_hit_testable(h.spectrum, false);

        h.eq_panel = factory.create_container();
        anchor_rect(h.eq_panel, {kUiPadding, eq_y,
                                 screen_width - kUiPadding * 2, kEqPanelHeight});
        kernel.set_hit_testable(h.eq_panel, false);

        h.eq_title = factory.create_label_static("EQ");
        anchor_rect(h.eq_title, {kUiPadding, eq_y,
                                 screen_width - kUiPadding * 2, kEqTitleHeight});

        constexpr std::array<const char*, kEqBands> kEqLabels{
            "60", "250", "1K", "4K", "16K"
        };
        for (std::size_t i = 0; i < kEqBands; ++i) {
            const int row_y = eq_y + kEqTitleHeight + kEqRowGap
                + static_cast<int>(i) * (kEqRowHeight + kEqRowGap);
            h.eq_labels[i] = factory.create_label_static(kEqLabels[i]);
            anchor_rect(h.eq_labels[i], {kUiPadding, row_y, kEqLabelWidth, kEqRowHeight});

            const int slider_x = kUiPadding + kEqLabelWidth + kEqRowGapX;
            const int slider_w = screen_width - kUiPadding * 2
                - kEqLabelWidth - kEqValueWidth - kEqRowGapX * 2;
            h.eq_sliders[i] = factory.create_slider();
            anchor_rect(h.eq_sliders[i], {slider_x, row_y, slider_w, kEqRowHeight});
            kernel.set_range(h.eq_sliders[i], -12, 12);
            kernel.set_value(h.eq_sliders[i], 0);

            h.eq_values[i] = factory.create_label_static("");
            anchor_rect(h.eq_values[i], {slider_x + slider_w + kEqRowGapX, row_y,
                                          kEqValueWidth, kEqRowHeight});
        }

        const int vol_y = eq_y + kEqTitleHeight + kEqRowGap
            + static_cast<int>(kEqBands) * (kEqRowHeight + kEqRowGap);
        h.volume_label = factory.create_label_static("Vol");
        anchor_rect(h.volume_label, {kUiPadding, vol_y, kEqLabelWidth, kEqRowHeight});
        const int vol_slider_x = kUiPadding + kEqLabelWidth + kEqRowGapX;
        const int vol_slider_w = screen_width - kUiPadding * 2
            - kEqLabelWidth - kEqValueWidth - kEqRowGapX * 2;
        h.volume_slider = factory.create_slider();
        anchor_rect(h.volume_slider, {vol_slider_x, vol_y, vol_slider_w, kEqRowHeight});
        kernel.set_range(h.volume_slider, 0, 100);
        kernel.set_value(h.volume_slider, 80);
        h.volume_value = factory.create_label_static("");
        anchor_rect(h.volume_value, {vol_slider_x + vol_slider_w + kEqRowGapX, vol_y,
                                      kEqValueWidth, kEqRowHeight});

        h.list_title = factory.create_label_static("");
        anchor_rect(h.list_title, {kUiPadding, list_y - kListTitleGap,
                                   screen_width - kUiPadding * 2, 18});

        h.list = factory.create_list_view();
        anchor_rect(h.list, {kUiPadding, list_y, screen_width - kUiPadding * 2, list_h});
        kernel.set_list_row_height(h.list, 34);
        kernel.set_scroll_step(h.list, 34);

        h.list_scroll = factory.create_scrollbar_for(h.list);
        anchor_rect(h.list_scroll, {screen_width - kUiPadding - kListScrollWidth, list_y,
                                    kListScrollWidth, list_h});
        kernel.set_scrollbar_orientation(h.list_scroll, ScrollBarOrientation::Vertical);

        h.list_hint = factory.create_label_static("");
        anchor_rect(h.list_hint, {kUiPadding, list_y, screen_width - kUiPadding * 2, list_h});

        h.controls = factory.create_container();
        anchor_rect(h.controls, {controls_x, controls_y, controls_w, kButtonHeight});

        h.btn_prev = factory.create_button_static("Prev");
        anchor_rect(h.btn_prev, {0, 0, kButtonWidth, kButtonHeight});
        factory.set_button_icon(h.btn_prev, icons.prev);
        factory.set_button_icon_size(h.btn_prev, 16);

        h.btn_pause = factory.create_button_static("");
        anchor_rect(h.btn_pause, {kButtonWidth + kButtonGap, 0,
                                  kButtonWidth, kButtonHeight});
        factory.set_button_icon(h.btn_pause, icons.play);
        factory.set_button_icon_size(h.btn_pause, 16);

        h.btn_next = factory.create_button_static("Next");
        anchor_rect(h.btn_next, {(kButtonWidth + kButtonGap) * 2, 0,
                                 kButtonWidth, kButtonHeight});
        factory.set_button_icon(h.btn_next, icons.next);
        factory.set_button_icon_size(h.btn_next, 16);

        h.btn_mode = factory.create_button_static("Mode");
        anchor_rect(h.btn_mode, {(kButtonWidth + kButtonGap) * 3, 0,
                                 kModeButtonWidth, kButtonHeight});
        factory.set_button_icon(h.btn_mode, icons.loop);
        factory.set_button_icon_size(h.btn_mode, 16);

        factory.link(h.root, scroll_area);
        factory.link(scroll_area, content);
        factory.link(content, h.cover);
        factory.link(content, h.title);
        factory.link(content, h.subtitle);
        factory.link(content, h.progress);
        factory.link(content, h.time);
        factory.link(content, h.status);
        factory.link(content, h.mode_hint);
        factory.link(content, h.spectrum);
        factory.link(content, h.eq_panel);
        factory.link(content, h.eq_title);
        for (std::size_t i = 0; i < kEqBands; ++i) {
            factory.link(content, h.eq_labels[i]);
            factory.link(content, h.eq_sliders[i]);
            factory.link(content, h.eq_values[i]);
        }
        factory.link(content, h.volume_label);
        factory.link(content, h.volume_slider);
        factory.link(content, h.volume_value);
        factory.link(content, h.list_title);
        factory.link(content, h.list);
        factory.link(content, h.list_scroll);
        factory.link(content, h.list_hint);
        factory.link(h.root, h.controls);
        factory.link(h.controls, h.btn_prev);
        factory.link(h.controls, h.btn_pause);
        factory.link(h.controls, h.btn_next);
        factory.link(h.controls, h.btn_mode);

        return h;
    }
}
