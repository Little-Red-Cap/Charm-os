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

        const int cover_top = kUiPadding * 2;
        const int header_top = cover_top + kCoverSize;
        const int mode_y = header_top + kHeaderModeOffset;
        const int mode_hint_y = mode_y + kModeHeight + kModeHintGap;
        const int spectrum_y = mode_hint_y + kModeHintHeight + kSpectrumGap;
        const int eq_y = spectrum_y + kSpectrumHeight + kSpectrumGap;
        const int list_y = eq_y + kEqPanelHeight + kSpectrumGap;
        const int list_h = screen_height - list_y - kListBottomReserve;

        h.cover = factory.create_container();
        anchor_rect(h.cover, {(screen_width - kCoverSize) / 2, cover_top, kCoverSize, kCoverSize});

        h.title = factory.create_label("Beautiful Trick");
        anchor_rect(h.title, {kUiPadding, header_top + kHeaderTitleOffset,
                              screen_width - kUiPadding * 2, 24});

        h.subtitle = factory.create_label("FELT / FLAC");
        anchor_rect(h.subtitle, {kUiPadding, header_top + kHeaderSubtitleOffset,
                                 screen_width - kUiPadding * 2, 20});

        h.progress = factory.create_progress();
        anchor_rect(h.progress, {kUiPadding, header_top + kHeaderProgressOffset,
                                 screen_width - kUiPadding * 2, 16});
        kernel.set_range(h.progress, 0, 100);
        kernel.set_value(h.progress, 0);
        kernel.set_hit_testable(h.progress, true);

        h.time = factory.create_label("0:00 / 3:00");
        anchor_rect(h.time, {kUiPadding, header_top + kHeaderTimeOffset,
                             screen_width - kUiPadding * 2, 18});

        h.status = factory.create_label("Stopped");
        anchor_rect(h.status, {kUiPadding, header_top + kHeaderStatusOffset,
                               screen_width - kUiPadding * 2, 18});

        h.mode_hint = factory.create_label("Mode: Order");
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

        h.eq_title = factory.create_label("EQ");
        anchor_rect(h.eq_title, {kUiPadding, eq_y,
                                 screen_width - kUiPadding * 2, kEqTitleHeight});

        constexpr std::array<const char*, kEqBands> kEqLabels{
            "60", "250", "1K", "4K", "16K"
        };
        for (std::size_t i = 0; i < kEqBands; ++i) {
            const int row_y = eq_y + kEqTitleHeight + kEqRowGap
                + static_cast<int>(i) * (kEqRowHeight + kEqRowGap);
            h.eq_labels[i] = factory.create_label(kEqLabels[i]);
            anchor_rect(h.eq_labels[i], {kUiPadding, row_y, kEqLabelWidth, kEqRowHeight});

            const int slider_x = kUiPadding + kEqLabelWidth + kEqRowGapX;
            const int slider_w = screen_width - kUiPadding * 2
                - kEqLabelWidth - kEqValueWidth - kEqRowGapX * 2;
            h.eq_sliders[i] = factory.create_slider();
            anchor_rect(h.eq_sliders[i], {slider_x, row_y, slider_w, kEqRowHeight});
            kernel.set_range(h.eq_sliders[i], -12, 12);
            kernel.set_value(h.eq_sliders[i], 0);

            h.eq_values[i] = factory.create_label("0");
            anchor_rect(h.eq_values[i], {slider_x + slider_w + kEqRowGapX, row_y,
                                          kEqValueWidth, kEqRowHeight});
        }

        h.list_title = factory.create_label("Tracks");
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

        h.list_hint = factory.create_label("No tracks in /music or /");
        anchor_rect(h.list_hint, {kUiPadding, list_y, screen_width - kUiPadding * 2, list_h});

        const int controls_y = screen_height - kControlsBottomMargin - kButtonHeight;
        const int controls_w = kButtonWidth * 3 + kButtonGap * 2 + kModeButtonWidth;
        const int controls_x = (screen_width - controls_w) / 2;

        h.controls = factory.create_container();
        anchor_rect(h.controls, {controls_x, controls_y, controls_w, kButtonHeight});

        h.btn_prev = factory.create_button("");
        anchor_rect(h.btn_prev, {controls_x, controls_y, kButtonWidth, kButtonHeight});
        factory.set_button_icon(h.btn_prev, icons.prev);
        factory.set_button_icon_size(h.btn_prev, 16);

        h.btn_pause = factory.create_button("");
        anchor_rect(h.btn_pause, {controls_x + kButtonWidth + kButtonGap, controls_y,
                                  kButtonWidth, kButtonHeight});
        factory.set_button_icon(h.btn_pause, icons.play);
        factory.set_button_icon_size(h.btn_pause, 16);

        h.btn_next = factory.create_button("");
        anchor_rect(h.btn_next, {controls_x + (kButtonWidth + kButtonGap) * 2, controls_y,
                                 kButtonWidth, kButtonHeight});
        factory.set_button_icon(h.btn_next, icons.next);
        factory.set_button_icon_size(h.btn_next, 16);

        h.btn_mode = factory.create_button("Mode");
        anchor_rect(h.btn_mode, {controls_x + (kButtonWidth + kButtonGap) * 3, controls_y,
                                 kModeButtonWidth, kButtonHeight});
        factory.set_button_icon(h.btn_mode, icons.loop);
        factory.set_button_icon_size(h.btn_mode, 16);

        factory.link(h.root, h.cover);
        factory.link(h.root, h.title);
        factory.link(h.root, h.subtitle);
        factory.link(h.root, h.progress);
        factory.link(h.root, h.time);
        factory.link(h.root, h.status);
        factory.link(h.root, h.mode_hint);
        factory.link(h.root, h.spectrum);
        factory.link(h.root, h.eq_panel);
        factory.link(h.root, h.eq_title);
        for (std::size_t i = 0; i < kEqBands; ++i) {
            factory.link(h.root, h.eq_labels[i]);
            factory.link(h.root, h.eq_sliders[i]);
            factory.link(h.root, h.eq_values[i]);
        }
        factory.link(h.root, h.list_title);
        factory.link(h.root, h.list);
        factory.link(h.root, h.list_scroll);
        factory.link(h.root, h.list_hint);
        factory.link(h.root, h.controls);
        factory.link(h.controls, h.btn_prev);
        factory.link(h.controls, h.btn_pause);
        factory.link(h.controls, h.btn_next);
        factory.link(h.controls, h.btn_mode);

        return h;
    }
}
