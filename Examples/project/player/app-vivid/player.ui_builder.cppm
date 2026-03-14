module;
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

    UiHandles build_ui(SoaFactory& factory, PlayerController& ctx) {
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
        const int list_y = mode_hint_y + kModeHintHeight + kSpectrumGap;
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

        h.time = factory.create_label("0:00 / 3:00");
        anchor_rect(h.time, {kUiPadding, header_top + kHeaderTimeOffset,
                             screen_width - kUiPadding * 2, 18});

        h.status = factory.create_label("Stopped");
        anchor_rect(h.status, {kUiPadding, header_top + kHeaderStatusOffset,
                               screen_width - kUiPadding * 2, 18});

        h.mode_hint = factory.create_label("Mode: Order");
        anchor_rect(h.mode_hint, {kUiPadding, mode_hint_y,
                                  screen_width - kUiPadding * 2, kModeHintHeight});

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

        h.btn_prev = factory.create_button("Prev");
        anchor_rect(h.btn_prev, {controls_x, controls_y, kButtonWidth, kButtonHeight});

        h.btn_pause = factory.create_button("Play");
        anchor_rect(h.btn_pause, {controls_x + kButtonWidth + kButtonGap, controls_y,
                                  kButtonWidth, kButtonHeight});

        h.btn_next = factory.create_button("Next");
        anchor_rect(h.btn_next, {controls_x + (kButtonWidth + kButtonGap) * 2, controls_y,
                                 kButtonWidth, kButtonHeight});

        h.btn_mode = factory.create_button("Mode");
        anchor_rect(h.btn_mode, {controls_x + (kButtonWidth + kButtonGap) * 3, controls_y,
                                 kModeButtonWidth, kButtonHeight});

        factory.link(h.root, h.cover);
        factory.link(h.root, h.title);
        factory.link(h.root, h.subtitle);
        factory.link(h.root, h.progress);
        factory.link(h.root, h.time);
        factory.link(h.root, h.status);
        factory.link(h.root, h.mode_hint);
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
