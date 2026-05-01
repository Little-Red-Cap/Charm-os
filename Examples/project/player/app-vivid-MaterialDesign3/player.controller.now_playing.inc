        void apply_now_theme() noexcept {
            if (!access.valid() || !handles.now_backdrop) return;
            const auto& roles = now_playing_roles;

            StylePatch patch{};
            patch.has_bg_color = true;
            patch.bg_color = roles.page_backdrop;
            patch.has_border_color = true;
            patch.border_color = {0, 0, 0, 0};
            patch.has_border_width = true;
            patch.border_width = 0;
            patch.has_corner_radius = true;
            patch.corner_radius = 0;
            access.set_style_override(handles.now_backdrop, patch);
            sync_now_cover_plate_surface();

            access.set_text_color(handles.title, roles.title);
            access.set_text_color(handles.subtitle, roles.subtitle);
            access.set_text_color(handles.time_left, roles.time);
            access.set_text_color(handles.time_right, roles.time);
            access.set_text_color(handles.mode_hint, roles.mode_hint);
            access.set_text_color(handles.bottom_title, roles.bottom_title);
            access.set_text_color(handles.bottom_subtitle, roles.bottom_subtitle);

            access.recolor_surface(handles.now_back, roles.top_button.bg, roles.top_button.border, roles.top_button.fg);
            access.recolor_surface(handles.now_more, roles.top_button.bg, roles.top_button.border, roles.top_button.fg);
            access.recolor_surface(handles.now_lyrics, roles.top_button.bg, roles.top_button.border, roles.top_button.fg);
            access.recolor_surface(handles.btn_prev,
                                   roles.secondary_control.bg,
                                   roles.secondary_control.border,
                                   roles.secondary_control.fg);
            access.recolor_surface(handles.btn_next,
                                   roles.secondary_control.bg,
                                   roles.secondary_control.border,
                                   roles.secondary_control.fg);
            access.recolor_surface(handles.btn_mode, roles.mode_button.bg, roles.mode_button.border, roles.mode_button.fg);
            access.recolor_surface(handles.btn_pause,
                                   roles.primary_control.bg,
                                   roles.primary_control.border,
                                   roles.primary_control.fg);
            access.recolor_surface(handles.bottom_next,
                                   roles.bottom_secondary_button.bg,
                                   roles.bottom_secondary_button.border,
                                   roles.bottom_secondary_button.fg);
            access.recolor_surface(handles.bottom_play,
                                   roles.bottom_primary_button.bg,
                                   roles.bottom_primary_button.border,
                                   roles.bottom_primary_button.fg);
            access.recolor_surface(handles.bottom_bar, roles.bottom_bar.bg, roles.bottom_bar.border, roles.bottom_bar.fg);
            access.recolor_surface(handles.info_tag, roles.info_tag.bg, roles.info_tag.border, roles.info_tag.fg);

            if (handles.progress_visual) {
                StylePatch prog{};
                prog.has_border_color = true;
                prog.border_color = roles.progress.track;
                prog.has_accent_color = true;
                prog.accent_color = roles.progress.fill;
                access.set_style_override(handles.progress_visual, prog);
            }

#if defined(CHARM_PLAYER_COVER_DEBUG)
            update_cover_debug_label();
#endif
        }

        ::ui::scene::ImageSlotHandles now_playing_transition_cover_slot() const noexcept {
            return make_image_slot_handles(handles.transition_cover);
        }

        void hide_now_playing_transition_cover() noexcept {
            set_handle_visible(handles.transition_cover, false);
            clear_image_slot_if_present(now_playing_transition_cover_slot(), false);
        }

        void show_now_playing_transition_cover(::ui::scene::ImageId image_id) noexcept {
            set_image_slot_if_present(now_playing_transition_cover_slot(), image_id, false);
            set_handle_visible(handles.transition_cover, true);
        }

        void sync_now_playing_transition_overlay() {
            if (!access.valid() || !handles.transition_root) return;
            set_label(handles.transition_title, title_text.c_str());
            set_label(handles.transition_subtitle, subtitle_text.c_str());
            const bool has_cover = has_cover_image();
            if (handles.transition_cover && access.kind(handles.transition_cover) == WidgetKind::Image) {
                if (has_cover) {
                    show_now_playing_transition_cover(cover_image.image_id);
                } else {
                    hide_now_playing_transition_cover();
                }
            }
            access.set_button_icon(handles.transition_play, playback.playing() ? icons.pause : icons.play);
        }

        struct NowPlayingOverlayGeometry {
            Rect shell{};
            Rect cover{};
            Rect title{};
            Rect subtitle{};
            Rect play{};
        };

        struct NowPlayingGroupBaseLayout {
            Rect top_bar{};
            Rect progress{};
            Rect controls{};
            Rect aux{};
        };

        struct NowPlayingTransitionSnapshot {
            NowPlayingOverlayGeometry mini{};
            NowPlayingOverlayGeometry now{};
            NowPlayingGroupBaseLayout now_groups{};
        };

        struct NowPlayingGroupVisibilityState {
            bool top_bar{};
            bool cover_plate{};
            bool cover{};
            bool text{};
            bool progress{};
            bool controls{};
            bool aux{};
        };

        struct NowPlayingGroupVisibilityThresholds {
            float top_bar{};
            float cover_plate{};
            float cover{};
            float text{};
            float progress{};
            float controls{};
            float aux{};
        };

        struct NowPlayingGroupSlideSpec {
            WidgetHandle handle{};
            Rect base{};
            float expand_start{};
            float expand_duration{};
            float collapse_start{};
            float collapse_duration{};
            int expand_offset_y{};
            int collapse_offset_y{};
        };

        struct NowPlayingTransitionFramePhase {
            float progress{};
            float eased{};
            float toward_destination{};
            bool expanding{};
            bool source_now{};
            bool destination_now{};
        };

        NowPlayingOverlayGeometry capture_mini_bar_overlay_geometry() noexcept {
            return {
                .shell = access.world_rect(handles.bottom_bar),
                .cover = access.world_rect(handles.bottom_cover),
                .title = access.world_rect(handles.bottom_title),
                .subtitle = access.world_rect(handles.bottom_subtitle),
                .play = access.world_rect(handles.bottom_play),
            };
        }

        static Rect make_now_playing_cover_rect_for_transition() noexcept {
            constexpr int kTopButton = 40;
            constexpr int kCoverSize = 392;
            const int top_bar_y = kUiPadding + 8;
            return {
                (screen_width - kCoverSize) / 2,
                top_bar_y + kTopButton + 34,
                kCoverSize,
                kCoverSize,
            };
        }

        static Rect make_now_playing_title_rect_for_transition() noexcept {
            constexpr int kTitleHeight = 46;
            constexpr int kTextGap = 8;
            const Rect cover_rect = make_now_playing_cover_rect_for_transition();
            const int text_y = cover_rect.y + cover_rect.h + 26;
            const int text_side = kUiPadding + 10;
            return {
                text_side,
                text_y,
                screen_width - text_side * 2,
                kTitleHeight,
            };
        }

        static Rect make_now_playing_subtitle_rect_for_transition() noexcept {
            constexpr int kTextGap = 8;
            constexpr int kSubtitleHeight = 30;
            const Rect title_rect = make_now_playing_title_rect_for_transition();
            return {
                title_rect.x,
                title_rect.y + title_rect.h + kTextGap,
                title_rect.w,
                kSubtitleHeight,
            };
        }

        static Rect make_now_playing_progress_group_rect_for_transition() noexcept {
            constexpr int kProgressHeight = 18;
            constexpr int kProgressPadX = 18;
            constexpr int kProgressPadY = 14;
            constexpr int kProgressGap = 12;
            constexpr int kTimeRowHeight = 32;
            const Rect subtitle_rect = make_now_playing_subtitle_rect_for_transition();
            const int progress_section_y = subtitle_rect.y + subtitle_rect.h + 26;
            return {
                kUiPadding,
                progress_section_y,
                screen_width - kUiPadding * 2,
                kProgressPadY * 2 + kProgressHeight + kProgressGap + kTimeRowHeight,
            };
        }

        static Rect make_now_playing_controls_rect_for_transition() noexcept {
            constexpr int kSmallButton = 96;
            constexpr int kPlayButton = 116;
            constexpr int kControlGap = 14;
            const Rect progress_rect = make_now_playing_progress_group_rect_for_transition();
            const int controls_w = kSmallButton * 2 + kPlayButton + kControlGap * 2;
            return {
                (screen_width - controls_w) / 2,
                progress_rect.y + progress_rect.h + 34,
                controls_w,
                kPlayButton,
            };
        }

        static Rect make_now_playing_play_rect_for_transition() noexcept {
            constexpr int kSmallButton = 96;
            constexpr int kControlGap = 14;
            constexpr int kPlayButton = 116;
            const Rect controls_rect = make_now_playing_controls_rect_for_transition();
            return {
                controls_rect.x + kSmallButton + kControlGap,
                controls_rect.y,
                kPlayButton,
                kPlayButton,
            };
        }

        static Rect make_now_playing_top_bar_rect_for_transition() noexcept {
            constexpr int kTopButton = 40;
            const int top_bar_y = kUiPadding + 8;
            return {0, top_bar_y - 4, screen_width, kTopButton + 8};
        }

        static Rect make_now_playing_aux_group_rect_for_transition() noexcept {
            constexpr int kAuxWidth = 224;
            constexpr int kAuxHeight = 68;
            constexpr int kPlayButton = 116;
            const Rect controls_rect = make_now_playing_controls_rect_for_transition();
            return {
                (screen_width - kAuxWidth) / 2,
                controls_rect.y + kPlayButton + 30,
                kAuxWidth,
                kAuxHeight,
            };
        }

        NowPlayingOverlayGeometry capture_now_playing_overlay_geometry() noexcept {
            return {
                .shell = {0, 0, screen_width, screen_height},
                .cover = make_now_playing_cover_rect_for_transition(),
                .title = make_now_playing_title_rect_for_transition(),
                .subtitle = make_now_playing_subtitle_rect_for_transition(),
                .play = make_now_playing_play_rect_for_transition(),
            };
        }

        NowPlayingGroupBaseLayout capture_now_playing_group_base_layout() noexcept {
            return {
                .top_bar = make_now_playing_top_bar_rect_for_transition(),
                .progress = make_now_playing_progress_group_rect_for_transition(),
                .controls = make_now_playing_controls_rect_for_transition(),
                .aux = make_now_playing_aux_group_rect_for_transition(),
            };
        }

        bool has_now_playing_transition_handles() const noexcept {
            return handles.transition_root
                && handles.transition_shell
                && handles.transition_cover
                && handles.transition_title
                && handles.transition_subtitle
                && handles.transition_play;
        }

        bool capture_now_playing_transition_snapshot(NowPlayingTransitionSnapshot& snapshot) noexcept {
            snapshot.mini = capture_mini_bar_overlay_geometry();
            snapshot.now = capture_now_playing_overlay_geometry();
            if (!has_valid_now_playing_transition_geometry(snapshot.mini, snapshot.now)) {
                return false;
            }
            snapshot.now_groups = capture_now_playing_group_base_layout();
            return true;
        }

        static NowPlayingTransitionEndpoints make_now_playing_transition_route(
            PlayerPage source_page,
            PlayerPage destination_page,
            NowPlayingTransitionDirection direction) noexcept {
            return {
                .source_page = source_page,
                .destination_page = destination_page,
                .expanding = direction == NowPlayingTransitionDirection::Expand,
            };
        }

        NowPlayingExpandTransition make_now_playing_transition_state(
            PlayerPage destination_page,
            NowPlayingTransitionDirection direction,
            const NowPlayingTransitionSnapshot& snapshot) const noexcept {
            const auto route = make_now_playing_transition_route(current_page, destination_page, direction);
            return NowPlayingExpandTransition{
                .active = true,
                .reveal_started = false,
                .route = route,
                .start_ms = charm::system::ClockCaps::TimeSource::now(),
                .shell_from = route.expanding ? snapshot.mini.shell : snapshot.now.shell,
                .shell_to = route.expanding ? snapshot.now.shell : snapshot.mini.shell,
                .cover_from = route.expanding ? snapshot.mini.cover : snapshot.now.cover,
                .cover_to = route.expanding ? snapshot.now.cover : snapshot.mini.cover,
                .title_from = route.expanding ? snapshot.mini.title : snapshot.now.title,
                .title_to = route.expanding ? snapshot.now.title : snapshot.mini.title,
                .subtitle_from = route.expanding ? snapshot.mini.subtitle : snapshot.now.subtitle,
                .subtitle_to = route.expanding ? snapshot.now.subtitle : snapshot.mini.subtitle,
                .play_from = route.expanding ? snapshot.mini.play : snapshot.now.play,
                .play_to = route.expanding ? snapshot.now.play : snapshot.mini.play,
                .now_top_bar_rect = snapshot.now_groups.top_bar,
                .now_progress_rect = snapshot.now_groups.progress,
                .now_controls_rect = snapshot.now_groups.controls,
                .now_aux_rect = snapshot.now_groups.aux,
            };
        }

        static bool is_expand_now_playing_transition(const NowPlayingExpandTransition& transition) noexcept {
            return transition.route.expanding;
        }

        static bool has_valid_now_playing_transition_geometry(const NowPlayingOverlayGeometry& mini,
                                                              const NowPlayingOverlayGeometry& now) noexcept {
            return mini.shell.w > 0 && mini.shell.h > 0 && now.cover.w > 0 && now.cover.h > 0;
        }

        void set_rect_if_valid(WidgetHandle handle, const Rect& rect) noexcept {
            if (!access.valid() || !handle || rect.w <= 0 || rect.h <= 0) return;
            access.set_rect(handle, rect);
        }

        void apply_now_playing_group_visibility(const NowPlayingGroupVisibilityState& state) noexcept {
            set_now_playing_group_visibility(state.top_bar,
                                            state.cover_plate,
                                            state.cover,
                                            state.text,
                                            state.progress,
                                            state.controls,
                                            state.aux);
        }

        std::array<NowPlayingGroupSlideSpec, 4> make_now_playing_group_slide_specs() const noexcept {
            return {{
                {
                    .handle = handles.now_top_bar,
                    .base = now_playing_transition.now_top_bar_rect,
                    .expand_start = 0.66f,
                    .expand_duration = 0.24f,
                    .collapse_start = 0.00f,
                    .collapse_duration = 0.12f,
                    .expand_offset_y = -16,
                    .collapse_offset_y = -14,
                },
                {
                    .handle = handles.now_progress_group,
                    .base = now_playing_transition.now_progress_rect,
                    .expand_start = 0.76f,
                    .expand_duration = 0.18f,
                    .collapse_start = 0.00f,
                    .collapse_duration = 0.18f,
                    .expand_offset_y = 18,
                    .collapse_offset_y = 16,
                },
                {
                    .handle = handles.controls,
                    .base = now_playing_transition.now_controls_rect,
                    .expand_start = 0.82f,
                    .expand_duration = 0.16f,
                    .collapse_start = 0.00f,
                    .collapse_duration = 0.20f,
                    .expand_offset_y = 22,
                    .collapse_offset_y = 18,
                },
                {
                    .handle = handles.now_aux_group,
                    .base = now_playing_transition.now_aux_rect,
                    .expand_start = 0.86f,
                    .expand_duration = 0.14f,
                    .collapse_start = 0.00f,
                    .collapse_duration = 0.14f,
                    .expand_offset_y = 26,
                    .collapse_offset_y = 22,
                },
            }};
        }

        static NowPlayingGroupVisibilityState sample_now_playing_group_visibility_state(
            const NowPlayingExpandTransition& transition,
            float progress) noexcept {
            if (!is_expand_now_playing_transition(transition)) {
                return {};
            }
            constexpr NowPlayingGroupVisibilityThresholds kExpandThresholds{
                .top_bar = 0.74f,
                .cover_plate = 0.92f,
                .cover = 0.94f,
                .text = 0.94f,
                .progress = 0.80f,
                .controls = 0.86f,
                .aux = 0.90f,
            };
            return {
                .top_bar = progress >= kExpandThresholds.top_bar,
                .cover_plate = progress >= kExpandThresholds.cover_plate,
                .cover = progress >= kExpandThresholds.cover,
                .text = progress >= kExpandThresholds.text,
                .progress = progress >= kExpandThresholds.progress,
                .controls = progress >= kExpandThresholds.controls,
                .aux = progress >= kExpandThresholds.aux,
            };
        }

        static NowPlayingTransitionFramePhase sample_now_playing_transition_frame_phase(
            const NowPlayingExpandTransition& transition,
            float progress) noexcept {
            const auto& endpoints = transition.route;
            return {
                .progress = progress,
                .eased = ease_out_cubic(progress),
                .toward_destination = endpoints.expanding
                    ? smoothstep01((progress - 0.42f) / 0.40f)
                    : smoothstep01((progress - 0.14f) / 0.56f),
                .expanding = endpoints.expanding,
                .source_now = endpoints.source_page == PlayerPage::NowPlaying,
                .destination_now = endpoints.destination_page == PlayerPage::NowPlaying,
            };
        }

        static NowPlayingOverlayGeometry sample_now_playing_transition_frame_geometry(
            const NowPlayingExpandTransition& transition,
            float eased) noexcept {
            return {
                .shell = lerp_rect(transition.shell_from, transition.shell_to, eased),
                .cover = lerp_rect(transition.cover_from, transition.cover_to, eased),
                .title = lerp_rect(transition.title_from, transition.title_to, eased),
                .subtitle = lerp_rect(transition.subtitle_from, transition.subtitle_to, eased),
                .play = lerp_rect(transition.play_from, transition.play_to, eased),
            };
        }

        static std::uint8_t sample_now_playing_transition_mini_alpha(bool source_now,
                                                                     bool destination_now,
                                                                     std::uint8_t mini_alpha,
                                                                     float toward_destination) noexcept {
            return lerp_u8(source_now ? 0 : mini_alpha,
                           destination_now ? 0 : mini_alpha,
                           toward_destination);
        }

        static std::uint8_t sample_now_playing_transition_mini_alpha(const NowPlayingTransitionFramePhase& phase,
                                                                     std::uint8_t mini_alpha) noexcept {
            return sample_now_playing_transition_mini_alpha(
                phase.source_now, phase.destination_now, mini_alpha, phase.toward_destination);
        }

        static int sample_now_playing_transition_now_or_mini_value(bool source_now,
                                                                   bool destination_now,
                                                                   int now_value,
                                                                   int mini_from,
                                                                   int mini_to,
                                                                   float eased) noexcept {
            return lerp_int(source_now ? now_value : mini_from,
                            destination_now ? now_value : mini_to,
                            eased);
        }

        static int sample_now_playing_transition_now_or_mini_value(const NowPlayingTransitionFramePhase& phase,
                                                                   int now_value,
                                                                   int mini_from,
                                                                   int mini_to) noexcept {
            return sample_now_playing_transition_now_or_mini_value(
                phase.source_now, phase.destination_now, now_value, mini_from, mini_to, phase.eased);
        }

        static int mini_transition_shell_corner_radius(const Rect& rect) noexcept {
            return std::min(rect.h / 2, 30);
        }

        static int mini_transition_cover_corner_radius(const Rect& rect) noexcept {
            return std::min(rect.w / 3, 18);
        }

        static int mini_transition_play_corner_radius(const Rect& rect) noexcept {
            return std::max(rect.h / 2, 18);
        }

        static bool can_begin_now_playing_transition(PlayerPage current_page,
                                                     PlayerPage destination_page,
                                                     NowPlayingTransitionDirection direction) noexcept {
            if (direction == NowPlayingTransitionDirection::Expand) {
                return is_mini_bar_page(current_page) && destination_page == PlayerPage::NowPlaying;
            }
            return current_page == PlayerPage::NowPlaying && is_mini_bar_page(destination_page);
        }

        bool prepare_now_playing_transition_state(PlayerPage destination_page,
                                                  NowPlayingTransitionDirection direction,
                                                  NowPlayingExpandTransition& transition_state) noexcept {
            if (!can_begin_now_playing_transition(current_page, destination_page, direction)) return false;
            if (!has_now_playing_transition_handles()) return false;

            NowPlayingTransitionSnapshot snapshot{};
            if (!capture_now_playing_transition_snapshot(snapshot)) return false;

            transition_state = make_now_playing_transition_state(destination_page, direction, snapshot);
            return true;
        }

        static rgba sample_now_playing_transition_now_or_mini_color(bool source_now,
                                                                    bool destination_now,
                                                                    const rgba& now_color,
                                                                    const rgba& mini_color,
                                                                    float toward_destination) noexcept {
            return lerp_color(source_now ? now_color : mini_color,
                              destination_now ? now_color : mini_color,
                              toward_destination);
        }

        static rgba sample_now_playing_transition_now_or_mini_color(const NowPlayingTransitionFramePhase& phase,
                                                                    const rgba& now_color,
                                                                    const rgba& mini_color) noexcept {
            return sample_now_playing_transition_now_or_mini_color(
                phase.source_now, phase.destination_now, now_color, mini_color, phase.toward_destination);
        }

        void apply_now_playing_transition_frame_geometry(const NowPlayingOverlayGeometry& geometry) noexcept {
            access.set_rect(handles.transition_shell, geometry.shell);
            access.set_rect(handles.transition_cover, geometry.cover);
            access.set_rect(handles.transition_title, geometry.title);
            access.set_rect(handles.transition_subtitle, geometry.subtitle);
            access.set_rect(handles.transition_play, geometry.play);
        }

        StylePatch make_now_playing_transition_shell_patch(const NowPlayingTransitionFramePhase& phase,
                                                           const player::cover_theme::NowPlayingColorRoles& roles) const noexcept {
            StylePatch patch{};
            patch.has_bg_color = true;
            patch.bg_color = sample_now_playing_transition_now_or_mini_color(
                phase, roles.page_backdrop, roles.bottom_bar.bg);
            patch.has_border_color = true;
            patch.border_color = sample_now_playing_transition_now_or_mini_color(
                phase, rgba{0, 0, 0, 0}, roles.bottom_bar.border);
            patch.has_border_width = true;
            patch.border_width = 0;
            patch.has_corner_radius = true;
            patch.corner_radius = sample_now_playing_transition_now_or_mini_value(
                phase,
                0,
                mini_transition_shell_corner_radius(now_playing_transition.shell_from),
                mini_transition_shell_corner_radius(now_playing_transition.shell_to));
            patch.has_shadow_enabled = true;
            patch.shadow_enabled = false;
            patch.has_inner_stroke_enabled = true;
            patch.inner_stroke_enabled = false;
            patch.has_outline_enabled = true;
            patch.outline_enabled = false;
            return patch;
        }

        StylePatch make_now_playing_transition_cover_patch(const NowPlayingTransitionFramePhase& phase) const noexcept {
            StylePatch patch{};
            patch.has_corner_radius = true;
            patch.corner_radius = sample_now_playing_transition_now_or_mini_value(
                phase,
                32,
                mini_transition_cover_corner_radius(now_playing_transition.cover_from),
                mini_transition_cover_corner_radius(now_playing_transition.cover_to));
            patch.has_shadow_enabled = true;
            patch.shadow_enabled = false;
            return patch;
        }

        StylePatch make_now_playing_transition_text_patch(const NowPlayingTransitionFramePhase& phase,
                                                          const rgba& now_color,
                                                          const rgba& mini_color,
                                                          std::uint8_t mini_alpha) const noexcept {
            StylePatch patch{};
            patch.has_font_color = true;
            patch.font_color = with_alpha(
                sample_now_playing_transition_now_or_mini_color(phase, now_color, mini_color),
                sample_now_playing_transition_mini_alpha(phase, mini_alpha));
            return patch;
        }

        StylePatch make_now_playing_transition_play_patch(const NowPlayingTransitionFramePhase& phase,
                                                          const NowPlayingOverlayGeometry& geometry,
                                                          const player::cover_theme::NowPlayingColorRoles& roles) const noexcept {
            const std::uint8_t play_fill_alpha = sample_now_playing_transition_mini_alpha(phase, 255);
            const rgba play_bg = sample_now_playing_transition_now_or_mini_color(
                phase, roles.primary_control.bg, roles.bottom_primary_button.bg);
            const rgba play_border = sample_now_playing_transition_now_or_mini_color(
                phase, roles.primary_control.border, roles.bottom_primary_button.border);
            const rgba play_fg = sample_now_playing_transition_now_or_mini_color(
                phase, roles.primary_control.fg, roles.bottom_primary_button.fg);
            StylePatch patch{};
            patch.has_bg_color = true;
            patch.bg_color = with_alpha(play_bg, play_fill_alpha);
            patch.has_border_color = true;
            patch.border_color = with_alpha(play_border, play_fill_alpha);
            patch.has_border_width = true;
            patch.border_width = 0;
            patch.has_corner_radius = true;
            patch.corner_radius = sample_now_playing_transition_now_or_mini_value(
                phase,
                34,
                mini_transition_play_corner_radius(now_playing_transition.play_from),
                mini_transition_play_corner_radius(now_playing_transition.play_to));
            patch.has_padding = true;
            patch.padding = std::max(0, (geometry.play.w - 32) / 2);
            patch.has_shadow_enabled = true;
            patch.shadow_enabled = false;
            patch.has_inner_stroke_enabled = true;
            patch.inner_stroke_enabled = false;
            patch.has_outline_enabled = true;
            patch.outline_enabled = false;
            patch.has_font_color = true;
            patch.font_color = with_alpha(play_fg, play_fill_alpha);
            return patch;
        }

        struct NowPlayingTransitionTickPhase {
            float progress{};
            bool should_start_reveal{};
            bool should_refresh_destination{};
            bool should_finish{};
        };

        static float now_playing_transition_reveal_threshold(const NowPlayingExpandTransition& transition) noexcept {
            return transition.route.expanding ? 0.60f : 0.12f;
        }

        static NowPlayingTransitionTickPhase sample_now_playing_transition_tick_phase(
            const NowPlayingExpandTransition& transition,
            std::uint64_t now_ms) noexcept {
            const auto elapsed = now_ms - transition.start_ms;
            const float progress = (kNowPlayingExpandDurationMs == 0)
                ? 1.0f
                : static_cast<float>(elapsed) / static_cast<float>(kNowPlayingExpandDurationMs);
            const float clamped = clamp01(progress);
            const bool should_start_reveal = !transition.reveal_started
                && clamped >= now_playing_transition_reveal_threshold(transition);
            return {
                .progress = clamped,
                .should_start_reveal = should_start_reveal,
                .should_refresh_destination = transition.reveal_started || should_start_reveal,
                .should_finish = clamped >= 1.0f,
            };
        }

        void start_now_playing_transition_reveal() noexcept {
            const auto& endpoints = now_playing_transition.route;
            now_playing_transition.reveal_started = true;
            set_page_root_visible(endpoints.destination_page, true);
            set_page_root_visible(endpoints.source_page, false);
        }

        ::ui::scene::SnapshotHandle capture_transition_source_snapshot() noexcept {
            if (!scene) return {};
            const ::ui::scene::SnapshotSpec spec{
                .bounds = Rect{0, 0, screen_width, screen_height},
                .preferred_kind = ::ui::scene::SnapshotKind::CommandBuffer,
            };
            return scene->capture_command_snapshot(spec);
        }

        void release_transition_source_snapshot() noexcept {
            if (!scene || !now_playing_transition.source_snapshot) return;
            if (scene->release_snapshot(now_playing_transition.source_snapshot)) {
                ++layer_transition_release_count;
            }
            now_playing_transition.source_snapshot = {};
        }

        void sample_transition_source_snapshot_compose(float progress) noexcept {
            if (!scene || !now_playing_transition.source_snapshot) return;
            const auto phase = sample_now_playing_transition_frame_phase(
                now_playing_transition, progress);
            const int slide = now_playing_transition.route.expanding
                ? static_cast<int>((1.0f - phase.toward_destination) * -18.0f)
                : static_cast<int>(phase.toward_destination * 18.0f);
            const auto result = scene->compose_snapshot_dry_run({
                .source = now_playing_transition.source_snapshot,
                .transform = ::ui::scene::LayerTransform{
                    .x = static_cast<std::int16_t>(slide),
                    .y = 0,
                    .opacity = static_cast<std::uint8_t>(
                        lerp_u8(255, 180, phase.toward_destination)),
                },
                .clip = Rect{0, 0, screen_width, screen_height},
                .has_clip = true,
            });
            if (result.ok) {
                ++layer_transition_compose_count;
                layer_transition_composite_pixels += result.composite_pixels;
            } else {
                ++layer_transition_compose_fail_count;
            }
        }

        void set_now_playing_transition_root_visible(bool visible) noexcept {
            set_handle_visible(handles.transition_root, visible);
        }

        void begin_now_playing_transition_lifecycle() noexcept {
            const auto& endpoints = now_playing_transition.route;
            now_playing_transition.source_snapshot = capture_transition_source_snapshot();
            if (now_playing_transition.source_snapshot) {
                ++layer_transition_capture_count;
            } else {
                ++layer_transition_capture_fail_count;
            }
            sample_transition_source_snapshot_compose(0.0f);
            sync_now_playing_transition_overlay();
            if (endpoints.expanding) {
                set_page_root_visible(endpoints.destination_page, false);
            }
            set_now_playing_transition_root_visible(true);
            update_now_playing_transition_group_layout(0.0f);
            update_now_playing_transition_group_visibility(0.0f);
            apply_now_playing_transition_frame(0.0f);
            if (endpoints.expanding) {
                set_mini_bar_chrome_visible(false);
            }
        }

        void restore_now_playing_transition_scene_state() noexcept {
            release_transition_source_snapshot();
            restore_now_playing_group_layout();
            restore_now_playing_group_visibility();
            restore_bottom_bar_content_visibility();
            set_now_playing_transition_root_visible(false);
        }

        void set_now_playing_group_visibility(bool top_bar,
                                              bool cover_plate,
                                              bool cover,
                                              bool text,
                                              bool progress,
                                              bool controls_visible,
                                              bool aux) noexcept {
            if (!access.valid()) return;
            const bool cover_plate_visible = cover_plate && !has_cover_image();
            const bool cover_widgets_visible = cover && has_cover_image();
            set_handle_visible(handles.now_top_bar, top_bar);
            set_handle_visible(handles.now_cover_plate, cover_plate_visible);
            set_handle_visible(handles.cover, cover_widgets_visible);
            set_handle_visible(handles.now_text_group, text);
            set_handle_visible(handles.now_progress_group, progress);
            set_handle_visible(handles.controls, controls_visible);
            set_handle_visible(handles.now_aux_group, aux);
        }

        void restore_now_playing_group_visibility() noexcept {
            set_now_playing_group_visibility(true, true, true, true, true, true, true);
        }

        void set_bottom_bar_content_visibility(bool cover,
                                               bool text,
                                               bool play,
                                               bool next) noexcept {
            if (!access.valid()) return;
            const bool cover_widget_visible = cover && has_cover_image();
            set_handle_visible(handles.bottom_cover, cover_widget_visible);
            set_handle_visible(handles.bottom_title, text);
            set_handle_visible(handles.bottom_subtitle, text);
            set_handle_visible(handles.bottom_play, play);
            set_handle_visible(handles.bottom_next, next);
        }

        void restore_bottom_bar_content_visibility() noexcept {
            set_bottom_bar_content_visibility(true, true, true, true);
        }

        void restore_now_playing_group_layout() noexcept {
            set_rect_if_valid(handles.now_top_bar, now_playing_transition.now_top_bar_rect);
            set_rect_if_valid(handles.now_progress_group, now_playing_transition.now_progress_rect);
            set_rect_if_valid(handles.controls, now_playing_transition.now_controls_rect);
            set_rect_if_valid(handles.now_aux_group, now_playing_transition.now_aux_rect);
        }

        void update_now_playing_transition_group_layout(float progress) noexcept {
            if (!access.valid() || !now_playing_transition.active) return;
            const bool expanding = now_playing_transition.route.expanding;
            for (const auto& spec : make_now_playing_group_slide_specs()) {
                if (!spec.handle || spec.base.w <= 0 || spec.base.h <= 0) continue;
                const float start = expanding ? spec.expand_start : spec.collapse_start;
                const float duration = expanding ? spec.expand_duration : spec.collapse_duration;
                const float local = smoothstep01((progress - start) / duration);
                const int offset_y = expanding
                    ? lerp_int(spec.expand_offset_y, 0, local)
                    : lerp_int(0, spec.collapse_offset_y, local);
                access.set_rect(spec.handle, offset_rect(spec.base, 0, offset_y));
            }
        }

        void update_now_playing_transition_group_visibility(float progress) noexcept {
            if (!now_playing_transition.active) {
                restore_now_playing_group_visibility();
                restore_bottom_bar_content_visibility();
                return;
            }

            const bool expanding = now_playing_transition.route.expanding;
            if (expanding) {
                restore_bottom_bar_content_visibility();
                apply_now_playing_group_visibility(
                    sample_now_playing_group_visibility_state(now_playing_transition, progress));
                return;
            }

            set_bottom_bar_content_visibility(false, false, false, false);
            apply_now_playing_group_visibility({});
        }

        void apply_now_playing_transition_frame(float progress) noexcept {
            if (!access.valid() || !handles.transition_root || !handles.transition_shell) return;
            const auto phase = sample_now_playing_transition_frame_phase(now_playing_transition, progress);
            const auto geometry = sample_now_playing_transition_frame_geometry(now_playing_transition, phase.eased);
            const auto& roles = now_playing_roles;

            apply_now_playing_transition_frame_geometry(geometry);
            access.set_style_override(handles.transition_shell,
                                      make_now_playing_transition_shell_patch(phase, roles));
            access.set_style_override(handles.transition_cover, make_now_playing_transition_cover_patch(phase));
            access.set_style_override(handles.transition_title,
                                      make_now_playing_transition_text_patch(
                                          phase, roles.title, roles.bottom_title, 255));
            access.set_style_override(handles.transition_subtitle,
                                      make_now_playing_transition_text_patch(
                                          phase,
                                          roles.subtitle,
                                          roles.bottom_subtitle,
                                          224));
            access.set_style_override(handles.transition_play,
                                      make_now_playing_transition_play_patch(phase, geometry, roles));
        }

        void finish_now_playing_transition() noexcept {
            if (!now_playing_transition.active) return;
            set_page(now_playing_transition.route.destination_page);
            restore_now_playing_transition_scene_state();
            now_playing_transition = {};
        }

        void tick_now_playing_transition(std::uint64_t now_ms) noexcept {
            if (!now_playing_transition.active) return;
            const auto tick = sample_now_playing_transition_tick_phase(now_playing_transition, now_ms);
            const auto destination_page = now_playing_transition.route.destination_page;
            if (tick.should_start_reveal) {
                start_now_playing_transition_reveal();
            }
            if (tick.should_refresh_destination) {
                const bool reveal_hook_already_refreshed = tick.should_start_reveal
                    && page_has_show_refresh_hook(destination_page);
                if (!reveal_hook_already_refreshed) {
                    refresh_page(destination_page);
                }
            }
            update_now_playing_transition_group_layout(tick.progress);
            update_now_playing_transition_group_visibility(tick.progress);
            apply_now_playing_transition_frame(tick.progress);
            if (tick.should_finish) {
                finish_now_playing_transition();
            }
        }

        bool begin_now_playing_transition(PlayerPage destination_page,
                                          NowPlayingTransitionDirection direction) noexcept {
            if (!access.valid()) return false;
            if (now_playing_transition.active) return true;
            NowPlayingExpandTransition transition_state{};
            if (!prepare_now_playing_transition_state(destination_page, direction, transition_state)) return false;

            now_playing_transition = transition_state;
            begin_now_playing_transition_lifecycle();
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

        void open_now_playing_page() noexcept {
            if (!begin_now_playing_expand_transition()) {
                set_page(PlayerPage::NowPlaying);
            }
        }

        void close_now_playing_page() noexcept {
            if (!begin_now_playing_collapse_transition()) {
                set_page(return_page);
            }
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

        void refresh_now_playing_runtime_content() {
            update_duration_from_player();
            if (!update_progress()) {
                set_time_label(playback.current_sec());
            }
            update_play_mode_label();
        }

        void refresh_now_playing_visual_content() noexcept {
            apply_now_theme();
        }

        void refresh_now_playing_body_content() {
            refresh_now_playing_runtime_content();
            refresh_now_playing_visual_content();
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
            sync_page_chrome(PlayerPage::NowPlaying);
            refresh_now_playing_body_content();
            update_debug_overlay();
        }
