        void refresh_exact_font_styles() noexcept {
            if (!access.valid()) return;
            const Font& title_font = get_player_font_px_variant(
                64, FontWeight::Regular, "wght636_wdth152_ROND50_XTRA520_YOPQ90_YTLC505");
            const Font& library_title_font = get_player_font_px(60, FontWeight::Bold);
            const Font& subtitle_font = get_player_font_px_variant(
                14, FontWeight::Regular, "ROND100");
            auto apply_font = [&](WidgetHandle h, const Font& font) {
                if (!h) return;
                StylePatch patch{};
                patch.has_font = true;
                patch.font = &font;
                access.set_style_adjust(h, patch);
            };
            auto apply_library_title_style = [&](WidgetHandle h) {
                if (!h) return;
                StylePatch patch{};
                patch.has_font = true;
                patch.font = &library_title_font;
                patch.has_font_color = true;
                patch.font_color = kUiOk;
                patch.has_font_color_disabled = true;
                patch.font_color_disabled = kUiOk;
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
                access.set_style_override(h, patch);
            };
            if (handles.library_hero_title) {
                // TODO(player-ui): move this rect back to the UI builder once the library top-bar layout source stays authoritative.
                access.set_rect(handles.library_hero_title,
                                {2, 4, screen_width - kUiPadding * 2 - 66, 92});
            }
            apply_font(handles.home_title_top, title_font);
            apply_font(handles.home_title_bottom, title_font);
            apply_font(handles.home_subtitle, subtitle_font);
            apply_library_title_style(handles.library_hero_title);
            apply_font(handles.probe_hero_top, title_font);
            apply_font(handles.probe_hero_bottom, title_font);
            apply_font(handles.probe_hero_subtitle, subtitle_font);
            access.set_text(handles.home_title_top, "Your");
            access.set_text(handles.home_title_bottom, "Mix");
            access.set_text(handles.home_subtitle, "Today's Mix for you");
            access.set_text(handles.library_hero_title, "Library");
            access.set_text(handles.probe_hero_top, "Your");
            access.set_text(handles.probe_hero_bottom, "Mix");
            access.set_text(handles.probe_hero_subtitle, "Today's Mix for you");
        }

        void refresh_font_visual_content() noexcept {
            apply_player_theme();
            refresh_exact_font_styles();
            refresh_now_playing_visual_content();
            refresh_home_visual_if_active();
            invalidate_library_visual_state();
            refresh_library_visual_if_active();
        }

        void rebind_player_fonts() noexcept {
            if (font_ttf_path.empty()) return;
            reset_player_font_package_cache();
            bind_player_freetype_font(std::string_view{font_ttf_path},
                                     std::string_view{font_fallback_ttf_path},
                                     font_small_px,
                                     font_normal_px,
                                     font_large_px);
            refresh_font_visual_content();
        }

        void apply_pending_font_resource_binding() noexcept {
            const bool ui_built = handles.root && handles.home_title_top;
            if (!ui_built || font_ttf_path.empty() || font_retry_done || !fs_ready) {
                return;
            }
            reset_player_font_package_cache();
            bind_player_freetype_font(std::string_view{font_ttf_path},
                                     std::string_view{font_fallback_ttf_path},
                                     font_small_px,
                                     font_normal_px,
                                     font_large_px);
            font_retry_done = true;
        }

        void set_font_config(std::string_view path,
                             std::string_view fallback_path,
                             int small_px,
                             int normal_px,
                             int large_px) {
            font_ttf_path.assign(path);
            font_fallback_ttf_path.assign(fallback_path);
            font_small_px = small_px;
            font_normal_px = normal_px;
            font_large_px = large_px;
            font_retry_done = false;
            const bool ui_built = handles.root && handles.home_title_top;
            if (!font_ttf_path.empty() && ui_built) {
                rebind_player_fonts();
                if (fs_ready) {
                    font_retry_done = true;
                }
            }
        }

        void apply_builtin_font_resource_config(int small_px,
                                                int normal_px,
                                                int large_px) noexcept {
            font_ttf_path.clear();
            font_fallback_ttf_path.clear();
            font_small_px = small_px;
            font_normal_px = normal_px;
            font_large_px = large_px;
            font_retry_done = true;
            clear_player_font_binding();
            const bool ui_built = handles.root && handles.home_title_top;
            if (ui_built) {
                refresh_font_visual_content();
            }
        }

        void apply_font_resource_config(const FontResourceConfig& font) {
            switch (font.kind) {
            case FontResourceKind::FilePath:
                set_font_config(font.primary_path.view(),
                                font.fallback_path.view(),
                                font.small_px,
                                font.normal_px,
                                font.large_px);
                break;
            case FontResourceKind::Package:
                // Memory-backed package consume path is not wired into MD3 yet.
                apply_builtin_font_resource_config(font.small_px,
                                                   font.normal_px,
                                                   font.large_px);
                break;
            case FontResourceKind::Builtin:
            default:
                apply_builtin_font_resource_config(font.small_px,
                                                   font.normal_px,
                                                   font.large_px);
                break;
            }
        }
