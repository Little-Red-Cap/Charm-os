        void refresh_exact_font_styles() noexcept {
            if (!access.valid()) return;
            const Font& title_font = get_player_font_px(72, FontWeight::Bold);
            const Font& subtitle_font = get_player_font_px(18, FontWeight::Regular);
            auto apply_font = [&](WidgetHandle h, const Font& font) {
                if (!h) return;
                StylePatch patch{};
                patch.has_font = true;
                patch.font = &font;
                access.set_style_override(h, patch);
            };
            apply_font(handles.home_title_top, title_font);
            apply_font(handles.home_title_bottom, title_font);
            apply_font(handles.home_subtitle, subtitle_font);
            apply_font(handles.probe_hero_top, title_font);
            apply_font(handles.probe_hero_bottom, title_font);
            apply_font(handles.probe_hero_subtitle, subtitle_font);
            access.set_text(handles.home_title_top, "Your");
            access.set_text(handles.home_title_bottom, "Mix");
            access.set_text(handles.home_subtitle, "Today's Mix for you");
            access.set_text(handles.probe_hero_top, "Your");
            access.set_text(handles.probe_hero_bottom, "Mix");
            access.set_text(handles.probe_hero_subtitle, "Today's Mix for you");
        }

        void rebind_player_fonts() noexcept {
            if (font_ttf_path.empty()) return;
            reset_player_font_package_cache();
            bind_player_freetype_font(font_ttf_path,
                                      font_small_px,
                                      font_normal_px,
                                      font_large_px);
            apply_player_theme();
            refresh_exact_font_styles();
        }

        void set_font_config(std::string_view path,
                             int small_px,
                             int normal_px,
                             int large_px) {
            font_ttf_path.assign(path.begin(), path.end());
            font_small_px = small_px;
            font_normal_px = normal_px;
            font_large_px = large_px;
            font_retry_done = false;
            if (!font_ttf_path.empty()) {
                rebind_player_fonts();
                if (fs_ready) {
                    font_retry_done = true;
                }
            }
        }
