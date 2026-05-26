module;

export module player.font_resource_apply;

import player.app_config;

export namespace player {
    template <typename Controller>
    void apply_player_font_resource(Controller& controller, const FontResourceConfig& font) {
        if constexpr (requires { controller.apply_font_resource_config(font); }) {
            controller.apply_font_resource_config(font);
        } else if (font.has_file_resource()) {
            if constexpr (requires {
                              controller.set_font_config(font.primary_path.view(),
                                                         font.fallback_path.view(),
                                                         font.small_px,
                                                         font.normal_px,
                                                         font.large_px);
                          }) {
                controller.set_font_config(font.primary_path.view(),
                                           font.fallback_path.view(),
                                           font.small_px,
                                           font.normal_px,
                                           font.large_px);
            } else if constexpr (requires {
                                     controller.set_font_config(font.primary_path.view(),
                                                                font.small_px,
                                                                font.normal_px,
                                                                font.large_px);
                                 }) {
                controller.set_font_config(font.primary_path.view(),
                                           font.small_px,
                                           font.normal_px,
                                           font.large_px);
            }
        }
    }
}
