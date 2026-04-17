        void apply_now_theme(const cover_theme::CoverTheme& theme) noexcept {
            if (!access.valid() || !handles.now_backdrop) return;
            StylePatch patch{};
            patch.has_bg_color = true;
            patch.bg_color = theme.backdrop;
            patch.has_border_color = true;
            patch.border_color = {0, 0, 0, 0};
            patch.has_border_width = true;
            patch.border_width = 0;
            patch.has_corner_radius = true;
            patch.corner_radius = 0;
            access.set_style_override(handles.now_backdrop, patch);

            const rgba title = theme.on_backdrop;
            const rgba subtitle = blend_on(theme.on_backdrop, theme.backdrop, 200);
            const rgba time = blend_on(theme.on_backdrop, theme.backdrop, 170);

            auto apply_label = [&](WidgetHandle h, const rgba& color) {
                if (!h) return;
                StylePatch p{};
                p.has_font_color = true;
                p.font_color = color;
                access.set_style_override(h, p);
            };
            apply_label(handles.title, title);
            apply_label(handles.subtitle, subtitle);
            apply_label(handles.time_left, time);
            apply_label(handles.time_right, time);

            if (handles.bottom_bar) {
                StylePatch bar = ::ui::scene::make_clean_surface_patch({
                    .apply_bg_color = true,
                    .apply_border_color = true,
                    .apply_corner_radius = false,
                    .bg_color = theme.primary_container,
                    .border_color = {0, 0, 0, 0},
                });
                access.set_style_override(handles.bottom_bar, bar);
            }

            if (handles.info_tag) {
                StylePatch tag = ::ui::scene::make_clean_surface_patch({
                    .apply_bg_color = true,
                    .apply_border_color = true,
                    .bg_color = theme.surface_high,
                    .border_color = with_alpha(theme.outline_variant, 0),
                });
                tag.has_font_color = true;
                tag.font_color = blend_on(theme.on_backdrop, theme.surface_high, 200);
                access.set_style_override(handles.info_tag, tag);
            }

            if (handles.progress_visual) {
                StylePatch prog{};
                prog.has_border_color = true;
                prog.border_color = with_alpha(theme.on_backdrop, 24);
                prog.has_accent_color = true;
                prog.accent_color = theme.primary;
                access.set_style_override(handles.progress_visual, prog);
            }

            auto apply_btn = [&](WidgetHandle h, const rgba& bg, const rgba& border, const rgba& font) {
                if (!h) return;
                StylePatch btn = ::ui::scene::make_clean_surface_patch({
                    .apply_bg_color = true,
                    .apply_border_color = true,
                    .bg_color = bg,
                    .border_color = border,
                });
                btn.has_font_color = true;
                btn.font_color = font;
                access.set_style_override(h, btn);
            };
            const rgba top_bg = blend_on(theme.surface_high, theme.backdrop, 140);
            const rgba top_border = blend_on(theme.outline_variant, theme.backdrop, 0);
            const rgba top_font = blend_on(theme.on_surface, theme.backdrop, 220);
            apply_btn(handles.now_back, top_bg, top_border, top_font);
            apply_btn(handles.now_more, top_bg, top_border, top_font);
            apply_btn(handles.now_lyrics, top_bg, top_border, top_font);

            const rgba control_bg = theme.secondary_container;
            const rgba control_border = with_alpha(theme.secondary_container, 0);
            const rgba control_font = theme.on_secondary_container;
            apply_btn(handles.btn_prev, control_bg, control_border, control_font);
            apply_btn(handles.btn_next, control_bg, control_border, control_font);
            const rgba mode_bg = blend_on(theme.surface_high, theme.backdrop, 168);
            const rgba mode_border = with_alpha(theme.outline_variant, 0);
            const rgba mode_font = blend_on(theme.on_surface, theme.surface_high, 228);
            apply_btn(handles.btn_mode, mode_bg, mode_border, mode_font);
            apply_btn(handles.btn_pause, theme.primary, theme.primary, theme.on_primary);

#if defined(CHARM_PLAYER_COVER_DEBUG)
            update_cover_debug_label();
#endif
        }

        void sync_now_playing_transition_overlay() {
            if (!access.valid() || !handles.transition_root) return;
            set_label(handles.transition_title, title_text.c_str());
            set_label(handles.transition_subtitle, subtitle_text.c_str());
            if (handles.transition_cover && access.kind(handles.transition_cover) == WidgetKind::Image) {
                access.set_image(handles.transition_cover,
                                 ::ui::scene::image_id_valid(cover_image.image_id)
                                     ? cover_image.image_id
                                     : ::ui::scene::invalid_image_id());
            }
            access.set_button_icon(handles.transition_play, playback.playing() ? icons.pause : icons.play);
        }

        void set_now_playing_group_visibility(bool top_bar,
                                              bool cover_plate,
                                              bool cover,
                                              bool text,
                                              bool progress,
                                              bool controls_visible,
                                              bool aux) noexcept {
            if (!access.valid()) return;
            const auto set_visible = [this](WidgetHandle handle, bool visible) {
                if (handle) access.set_visible(handle, visible);
            };
            set_visible(handles.now_top_bar, top_bar);
            set_visible(handles.now_cover_plate, cover_plate);
            set_visible(handles.cover, cover);
            set_visible(handles.cover_left, cover);
            set_visible(handles.cover_right, cover);
            set_visible(handles.now_text_group, text);
            set_visible(handles.now_progress_group, progress);
            set_visible(handles.controls, controls_visible);
            set_visible(handles.now_aux_group, aux);
        }

        void restore_now_playing_group_visibility() noexcept {
            set_now_playing_group_visibility(true, true, true, true, true, true, true);
        }

        void set_bottom_bar_content_visibility(bool cover,
                                               bool text,
                                               bool play,
                                               bool next) noexcept {
            if (!access.valid()) return;
            const auto set_visible = [this](WidgetHandle handle, bool visible) {
                if (handle) access.set_visible(handle, visible);
            };
            set_visible(handles.bottom_cover, cover);
            set_visible(handles.bottom_title, text);
            set_visible(handles.bottom_subtitle, text);
            set_visible(handles.bottom_play, play);
            set_visible(handles.bottom_next, next);
        }

        void restore_bottom_bar_content_visibility() noexcept {
            set_bottom_bar_content_visibility(true, true, true, true);
        }

        void restore_now_playing_group_layout() noexcept {
            if (!access.valid()) return;
            const auto set_rect_if = [this](WidgetHandle handle, const Rect& rect) {
                if (!handle || rect.w <= 0 || rect.h <= 0) return;
                access.set_rect(handle, rect);
            };
            set_rect_if(handles.now_top_bar, now_playing_transition.now_top_bar_rect);
            set_rect_if(handles.now_progress_group, now_playing_transition.now_progress_rect);
            set_rect_if(handles.controls, now_playing_transition.now_controls_rect);
            set_rect_if(handles.now_aux_group, now_playing_transition.now_aux_rect);
        }

        void update_now_playing_transition_group_layout(float progress) noexcept {
            if (!access.valid() || !now_playing_transition.active) return;
            const bool expanding = now_playing_transition.direction == NowPlayingTransitionDirection::Expand;

            const auto apply_slide = [&](WidgetHandle handle,
                                         const Rect& base,
                                         float start,
                                         float duration,
                                         int offset_y_expand,
                                         int offset_y_collapse) {
                if (!handle || base.w <= 0 || base.h <= 0) return;
                const float local = smoothstep01((progress - start) / duration);
                const int offset_y = expanding
                    ? lerp_int(offset_y_expand, 0, local)
                    : lerp_int(0, offset_y_collapse, local);
                access.set_rect(handle, offset_rect(base, 0, offset_y));
            };

            apply_slide(handles.now_top_bar,
                        now_playing_transition.now_top_bar_rect,
                        expanding ? 0.66f : 0.00f,
                        expanding ? 0.24f : 0.12f,
                        -16,
                        -14);
            apply_slide(handles.now_progress_group,
                        now_playing_transition.now_progress_rect,
                        expanding ? 0.76f : 0.00f,
                        expanding ? 0.18f : 0.18f,
                        18,
                        16);
            apply_slide(handles.controls,
                        now_playing_transition.now_controls_rect,
                        expanding ? 0.82f : 0.00f,
                        expanding ? 0.16f : 0.20f,
                        22,
                        18);
            apply_slide(handles.now_aux_group,
                        now_playing_transition.now_aux_rect,
                        expanding ? 0.86f : 0.00f,
                        expanding ? 0.14f : 0.14f,
                        26,
                        22);
        }

        void update_now_playing_transition_group_visibility(float progress) noexcept {
            if (!now_playing_transition.active) {
                restore_now_playing_group_visibility();
                restore_bottom_bar_content_visibility();
                return;
            }

            const bool expanding = now_playing_transition.direction == NowPlayingTransitionDirection::Expand;
            if (expanding) {
                restore_bottom_bar_content_visibility();
                set_now_playing_group_visibility(
                    progress >= 0.74f,
                    progress >= 0.92f,
                    progress >= 0.94f,
                    progress >= 0.94f,
                    progress >= 0.80f,
                    progress >= 0.86f,
                    progress >= 0.90f);
                return;
            }

            set_bottom_bar_content_visibility(false, false, false, false);
            set_now_playing_group_visibility(false, false, false, false, false, false, false);
        }

        void apply_now_playing_transition_frame(float progress) noexcept {
            if (!access.valid() || !handles.transition_root || !handles.transition_shell) return;
            const float eased = ease_out_cubic(progress);
            const bool expanding = now_playing_transition.direction == NowPlayingTransitionDirection::Expand;
            const float toward_destination = expanding
                ? smoothstep01((progress - 0.42f) / 0.40f)
                : smoothstep01((progress - 0.14f) / 0.56f);
            const bool source_now = now_playing_transition.source_page == PlayerPage::NowPlaying;
            const bool destination_now = now_playing_transition.destination_page == PlayerPage::NowPlaying;

            const Rect shell_rect = lerp_rect(now_playing_transition.shell_from,
                                              now_playing_transition.shell_to,
                                              eased);
            const Rect cover_rect = lerp_rect(now_playing_transition.cover_from,
                                              now_playing_transition.cover_to,
                                              eased);
            const Rect title_rect = lerp_rect(now_playing_transition.title_from,
                                              now_playing_transition.title_to,
                                              eased);
            const Rect subtitle_rect = lerp_rect(now_playing_transition.subtitle_from,
                                                 now_playing_transition.subtitle_to,
                                                 eased);
            const Rect play_rect = lerp_rect(now_playing_transition.play_from,
                                             now_playing_transition.play_to,
                                             eased);

            access.set_rect(handles.transition_shell, shell_rect);
            access.set_rect(handles.transition_cover, cover_rect);
            access.set_rect(handles.transition_title, title_rect);
            access.set_rect(handles.transition_subtitle, subtitle_rect);
            access.set_rect(handles.transition_play, play_rect);

            {
                const rgba source_bg = source_now ? cover_theme.backdrop : cover_theme.primary_container;
                const rgba destination_bg = destination_now ? cover_theme.backdrop : cover_theme.primary_container;
                StylePatch patch{};
                patch.has_bg_color = true;
                patch.bg_color = lerp_color(source_bg, destination_bg, toward_destination);
                patch.has_border_color = true;
                patch.border_color = with_alpha(cover_theme.outline_variant,
                                                lerp_u8(source_now ? 0 : 90,
                                                        destination_now ? 0 : 90,
                                                        toward_destination));
                patch.has_border_width = true;
                patch.border_width = 0;
                patch.has_corner_radius = true;
                patch.corner_radius = lerp_int(source_now ? 0 : std::min(now_playing_transition.shell_from.h / 2, 30),
                                               destination_now ? 0 : std::min(now_playing_transition.shell_to.h / 2, 30),
                                               eased);
                patch.has_shadow_enabled = true;
                patch.shadow_enabled = false;
                patch.has_inner_stroke_enabled = true;
                patch.inner_stroke_enabled = false;
                patch.has_outline_enabled = true;
                patch.outline_enabled = false;
                access.set_style_override(handles.transition_shell, patch);
            }

            {
                StylePatch patch{};
                patch.has_corner_radius = true;
                patch.corner_radius = lerp_int(source_now ? 32 : std::min(now_playing_transition.cover_from.w / 3, 18),
                                               destination_now ? 32 : std::min(now_playing_transition.cover_to.w / 3, 18),
                                               eased);
                patch.has_shadow_enabled = true;
                patch.shadow_enabled = false;
                access.set_style_override(handles.transition_cover, patch);
            }

            {
                const rgba source_title = source_now ? cover_theme.on_backdrop : kUiTitle;
                const rgba destination_title = destination_now ? cover_theme.on_backdrop : kUiTitle;
                StylePatch patch{};
                patch.has_font_color = true;
                patch.font_color = with_alpha(lerp_color(source_title, destination_title, toward_destination),
                                              lerp_u8(source_now ? 0 : 255,
                                                      destination_now ? 0 : 255,
                                                      toward_destination));
                access.set_style_override(handles.transition_title, patch);
            }
            {
                const rgba source_subtitle = source_now
                    ? blend_on(cover_theme.on_backdrop, cover_theme.backdrop, 188)
                    : kUiSubtitle;
                const rgba destination_subtitle = destination_now
                    ? blend_on(cover_theme.on_backdrop, cover_theme.backdrop, 188)
                    : kUiSubtitle;
                StylePatch patch{};
                patch.has_font_color = true;
                patch.font_color = with_alpha(lerp_color(source_subtitle, destination_subtitle, toward_destination),
                                              lerp_u8(source_now ? 0 : 224,
                                                      destination_now ? 0 : 224,
                                                      toward_destination));
                access.set_style_override(handles.transition_subtitle, patch);
            }
            {
                StylePatch patch{};
                patch.has_bg_color = true;
                patch.bg_color = with_alpha(cover_theme.primary,
                                            lerp_u8(source_now ? 0 : 255,
                                                    destination_now ? 0 : 255,
                                                    toward_destination));
                patch.has_border_color = true;
                patch.border_color = with_alpha(cover_theme.primary,
                                                lerp_u8(source_now ? 0 : 56,
                                                        destination_now ? 0 : 56,
                                                        toward_destination));
                patch.has_border_width = true;
                patch.border_width = 0;
                patch.has_corner_radius = true;
                patch.corner_radius = lerp_int(source_now ? 34 : std::max(now_playing_transition.play_from.h / 2, 18),
                                               destination_now ? 34 : std::max(now_playing_transition.play_to.h / 2, 18),
                                               eased);
                patch.has_padding = true;
                patch.padding = std::max(0, (play_rect.w - 32) / 2);
                patch.has_shadow_enabled = true;
                patch.shadow_enabled = false;
                patch.has_inner_stroke_enabled = true;
                patch.inner_stroke_enabled = false;
                patch.has_outline_enabled = true;
                patch.outline_enabled = false;
                patch.has_font_color = true;
                patch.font_color = with_alpha(cover_theme.on_primary,
                                              lerp_u8(source_now ? 0 : 255,
                                                      destination_now ? 0 : 255,
                                                      toward_destination));
                access.set_style_override(handles.transition_play, patch);
            }
        }

        void finish_now_playing_transition() noexcept {
            if (!now_playing_transition.active) return;
            const PlayerPage destination_page = now_playing_transition.destination_page;
            set_page(destination_page);
            restore_now_playing_group_layout();
            restore_now_playing_group_visibility();
            restore_bottom_bar_content_visibility();
            if (destination_page == PlayerPage::NowPlaying) {
                refresh_now_playing();
            }
            if (handles.transition_root) {
                access.set_visible(handles.transition_root, false);
            }
            now_playing_transition = {};
        }

        void tick_now_playing_transition(std::uint64_t now_ms) noexcept {
            if (!now_playing_transition.active) return;
            const auto elapsed = now_ms - now_playing_transition.start_ms;
            const float progress = (kNowPlayingExpandDurationMs == 0)
                ? 1.0f
                : static_cast<float>(elapsed) / static_cast<float>(kNowPlayingExpandDurationMs);
            const float clamped = clamp01(progress);
            const bool expanding = now_playing_transition.direction == NowPlayingTransitionDirection::Expand;
            const float reveal_threshold = expanding ? 0.60f : 0.12f;
            if (!now_playing_transition.reveal_started && clamped >= reveal_threshold) {
                now_playing_transition.reveal_started = true;
                if (!expanding) {
                    set_page_root_visible(now_playing_transition.destination_page, true);
                    refresh_page_for_transition(now_playing_transition.destination_page);
                }
                set_page_root_visible(now_playing_transition.source_page, false);
                if (expanding) {
                    set_page_root_visible(now_playing_transition.destination_page, true);
                }
            }
            if (now_playing_transition.reveal_started) {
                refresh_page_for_transition(now_playing_transition.destination_page);
            }
            update_now_playing_transition_group_layout(clamped);
            update_now_playing_transition_group_visibility(clamped);
            apply_now_playing_transition_frame(clamped);
            if (clamped >= 1.0f) {
                finish_now_playing_transition();
            }
        }

        bool begin_now_playing_transition(PlayerPage destination_page,
                                          NowPlayingTransitionDirection direction) noexcept {
            if (!access.valid()) return false;
            if (now_playing_transition.active) return true;
            if (direction == NowPlayingTransitionDirection::Expand) {
                if (!is_mini_bar_page(current_page) || destination_page != PlayerPage::NowPlaying) return false;
            } else {
                if (current_page != PlayerPage::NowPlaying || !is_mini_bar_page(destination_page)) return false;
            }
            if (!handles.transition_root || !handles.transition_shell || !handles.transition_cover
                || !handles.transition_title || !handles.transition_subtitle || !handles.transition_play) {
                return false;
            }

            const Rect mini_shell = access.world_rect(handles.bottom_bar);
            const Rect mini_cover = access.world_rect(handles.bottom_cover);
            const Rect mini_title = access.world_rect(handles.bottom_title);
            const Rect mini_subtitle = access.world_rect(handles.bottom_subtitle);
            const Rect mini_play = access.world_rect(handles.bottom_play);
            const Rect now_shell{0, 0, screen_width, screen_height};
            const Rect now_cover = access.world_rect(handles.cover);
            const Rect now_title{
                kUiPadding + 10,
                std::max(kUiPadding, access.world_rect(handles.title).y - 4),
                screen_width - (kUiPadding + 10) * 2,
                access.world_rect(handles.title).h
            };
            const Rect now_subtitle{
                kUiPadding + 10,
                std::max(kUiPadding, access.world_rect(handles.subtitle).y - 2),
                screen_width - (kUiPadding + 10) * 2,
                access.world_rect(handles.subtitle).h
            };
            const Rect now_play = access.world_rect(handles.btn_pause);
            if (mini_shell.w <= 0 || mini_shell.h <= 0 || now_cover.w <= 0 || now_cover.h <= 0) {
                return false;
            }

            const bool expanding = direction == NowPlayingTransitionDirection::Expand;
            now_playing_transition = NowPlayingExpandTransition{
                .active = true,
                .reveal_started = false,
                .direction = direction,
                .source_page = current_page,
                .destination_page = destination_page,
                .start_ms = charm::system::ClockCaps::TimeSource::now(),
                .shell_from = expanding ? mini_shell : now_shell,
                .shell_to = expanding ? now_shell : mini_shell,
                .cover_from = expanding ? mini_cover : now_cover,
                .cover_to = expanding ? now_cover : mini_cover,
                .title_from = expanding ? mini_title : now_title,
                .title_to = expanding ? now_title : mini_title,
                .subtitle_from = expanding ? mini_subtitle : now_subtitle,
                .subtitle_to = expanding ? now_subtitle : mini_subtitle,
                .play_from = expanding ? mini_play : now_play,
                .play_to = expanding ? now_play : mini_play,
                .now_top_bar_rect = access.world_rect(handles.now_top_bar),
                .now_progress_rect = access.world_rect(handles.now_progress_group),
                .now_controls_rect = access.world_rect(handles.controls),
                .now_aux_rect = access.world_rect(handles.now_aux_group),
            };

            sync_now_playing_transition_overlay();
            if (expanding) {
                set_page_root_visible(PlayerPage::NowPlaying, false);
            }
            access.set_visible(handles.transition_root, true);
            update_now_playing_transition_group_layout(0.0f);
            update_now_playing_transition_group_visibility(0.0f);
            apply_now_playing_transition_frame(0.0f);
            if (expanding) {
                if (handles.bottom_bar) access.set_visible(handles.bottom_bar, false);
                if (handles.nav_bar) access.set_visible(handles.nav_bar, false);
            }
            return true;
        }

        bool begin_now_playing_expand_transition() noexcept {
            return begin_now_playing_transition(PlayerPage::NowPlaying,
                                                NowPlayingTransitionDirection::Expand);
        }

        bool begin_now_playing_collapse_transition() noexcept {
            if (!is_mini_bar_page(return_page)) return false;
            return begin_now_playing_transition(return_page,
                                                NowPlayingTransitionDirection::Collapse);
        }

        void set_play_button_text(bool playing_now) {
            if (!access.valid()) return;
            const int state = playing_now ? 1 : 0;
            if (last_play_button_state == state) return;
            last_play_button_state = state;
            access.set_button_icon(handles.btn_pause, playing_now ? icons.pause : icons.play);
            set_label_slot(handles.btn_pause, text_slots.btn_pause, "");
            if (handles.bottom_play) {
                access.set_button_icon(handles.bottom_play, playing_now ? icons.pause : icons.play);
            }
            if (handles.transition_play) {
                access.set_button_icon(handles.transition_play, playing_now ? icons.pause : icons.play);
            }
        }

        void set_time_label(int elapsed_sec) {
            if (elapsed_sec == last_time_sec) return;
            last_time_sec = elapsed_sec;
            int total = 0;
            if (track_preloaded) {
                total = playback.duration_sec();
                if (preloaded_duration_sec > 0) {
                    total = std::max(total, preloaded_duration_sec);
                }
            }
            const int cur_m = elapsed_sec / 60;
            const int cur_s = elapsed_sec % 60;
            const int total_m = total / 60;
            const int total_s = total % 60;
            char left[16]{};
            char right[16]{};
            std::snprintf(left, sizeof(left), "%d:%02d", cur_m, cur_s);
            std::snprintf(right, sizeof(right), "%d:%02d", total_m, total_s);
            set_label_slot(handles.time_left, text_slots.time_left, left);
            set_label_slot(handles.time_right, text_slots.time_right, right);
#if defined(CHARM_PLAYER_COVER_DEBUG)
            if (elapsed_sec == 0) {
                std::printf("[np] time_label left=%s right=%s total=%d\n", left, right, total);
            }
#endif
        }

        void update_info_label() {
            if (!access.valid() || !handles.info_tag) return;
            int total = playback.duration_sec();
            if (preloaded_duration_sec > 0) {
                total = std::max(total, preloaded_duration_sec);
            }
            if (total <= 0) return;
            std::uint32_t rate = 0;
            audio::PlayerSnapshot snap{};
            if (playback.snapshot(snap)) {
                rate = snap.input_fmt.rate;
            }
            if (rate == 0) rate = 48000;
            std::uint64_t kbps = 0;
            if (track_size_bytes > 0) {
                kbps = (track_size_bytes * 8ull) / (static_cast<std::uint64_t>(total) * 1000ull);
            }

            char rate_buf[16]{};
            const std::uint32_t khz_int = rate / 1000;
            const std::uint32_t khz_dec = (rate % 1000) / 100;
            if (khz_dec == 0) {
                std::snprintf(rate_buf, sizeof(rate_buf), "%ukHz", static_cast<unsigned>(khz_int));
            } else {
                std::snprintf(rate_buf, sizeof(rate_buf), "%u.%ukHz",
                              static_cast<unsigned>(khz_int),
                              static_cast<unsigned>(khz_dec));
            }

            char kbps_buf[16]{};
            if (kbps > 0) {
                std::snprintf(kbps_buf, sizeof(kbps_buf), "%llukbps",
                              static_cast<unsigned long long>(kbps));
            } else {
                std::snprintf(kbps_buf, sizeof(kbps_buf), "--kbps");
            }

            const char* fmt = track_format_text.empty() ? "--" : track_format_text.c_str();
            char info[96]{};
            std::snprintf(info, sizeof(info), "%s * %s * %s", rate_buf, kbps_buf, fmt);
            set_info_label(info);
        }

        void reset_duration() {
            playback.reset_duration();
            last_time_sec = -1;
        }

        static const char* play_mode_text(int mode) noexcept {
            switch (mode) {
            case 1: return "Single";
            case 2: return "Shuffle";
            default: return "Order";
            }
        }

        void update_play_mode_label() {
            char buf[32]{};
            std::snprintf(buf, sizeof(buf), "Mode: %s", play_mode_text(play_mode));
            if (last_mode_text.view() == buf) return;
            last_mode_text.assign(buf);
            set_label_slot(handles.mode_hint, text_slots.mode_hint, buf);
            if (!access.valid()) return;
            auto icon = icons.loop;
            if (play_mode == 1) icon = icons.single;
            else if (play_mode == 2) icon = icons.shuffle;
            access.set_button_icon(handles.btn_mode, icon);
        }

        void refresh_now_playing() {
            update_duration_from_player();
            update_progress();
            update_play_mode_label();
            apply_now_theme(cover_theme);
            update_debug_overlay();
        }
