module;
#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>

export module player.ui_builder;

import charm.core.config;
import charm.core.geometry;
import charm.ui.scene;
import charm.ui.scene.page_header;
import charm.ui.scene.top_bar;
import charm.ui.scene.pill;
import charm.ui.scene.path_bar;
import charm.ui.scene.pill_surface;
import charm.ui.scene.list_card_header;
import charm.ui.scene.anchored_menu;
import charm.ui.scene.page_layers;
import charm.ui.scene.seek_bar_style;
import charm.ui.scene.text_style;
import charm.font.typography;
import player.controller;
import player.ui;

namespace player::ui_builder_detail {
#include "player.ui_builder.shared.inc"
#include "player.ui_builder.probe.inc"
#include "player.ui_builder.now_playing.inc"
#include "player.ui_builder.home.inc"
#include "player.ui_builder.library.inc"
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
        ui_builder_detail::build_probe(builder, h);
        if (h.page_probe) {
            builder.link(h.root, h.page_probe);
        }
        ui_builder_detail::build_home(builder, h, layout, icons);
        if (h.page_home) {
            builder.link(h.root, h.page_home);
        }
        ui_builder_detail::build_now_playing(builder, h, layout, now_text, icons);
        ui_builder_detail::build_now_playing_transition_overlay(builder, h, icons);
        ui_builder_detail::build_library(builder, h, layout, icons);
        if (h.transition_root) {
            builder.link(h.root, h.transition_root);
        }
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
