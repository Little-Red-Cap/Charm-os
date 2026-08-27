        void apply_now_theme() noexcept {
            if (!access.valid() || !handles.now_backdrop) return;
            const auto& roles = now_playing_roles;

            access.set_style_override(handles.now_backdrop,
                                      make_now_backdrop_patch(roles.page_backdrop, roles.title));
            sync_now_cover_plate_surface();

            access.set_text_color(handles.title, roles.title);
            access.set_text_color(handles.subtitle, roles.subtitle);
#if CHARM_PLAYER_LYRICS
            access.set_text_color(handles.now_lyrics_prev, roles.subtitle);
            access.set_text_color(handles.now_lyrics_current, roles.title);
            access.set_text_color(handles.now_lyrics_next, roles.subtitle);
            access.set_text_color(handles.now_lyrics_status, roles.time);
#endif
            access.set_text_color(handles.time_left, roles.time);
            access.set_text_color(handles.time_right, roles.time);
            access.set_text_color(handles.mode_hint, roles.mode_hint);
            access.set_text_color(handles.bottom_title, roles.bottom_title);
            access.set_text_color(handles.bottom_subtitle, roles.bottom_subtitle);

            access.recolor_surface(handles.now_back, roles.top_button.bg, roles.top_button.border, roles.top_button.fg);
            access.recolor_surface(handles.now_more, roles.top_button.bg, roles.top_button.border, roles.top_button.fg);
#if CHARM_PLAYER_LYRICS
            access.recolor_surface(handles.now_lyrics, roles.top_button.bg, roles.top_button.border, roles.top_button.fg);
#endif
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
            apply_now_progress_interaction_style();

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
#if CHARM_PLAYER_LAYERED_TRANSITIONS
            if (!access.valid() || !handles.transition_root) return;
            set_label(handles.transition_title, title_text.c_str());
            set_label(handles.transition_subtitle, subtitle_text.c_str());
            const bool has_cover = has_cover_image();
            if (handles.transition_cover && access.kind(handles.transition_cover) == WidgetKind::Image) {
                if (has_cover) {
                    show_now_playing_transition_cover(current_cover.image_id);
                } else {
                    hide_now_playing_transition_cover();
                }
            }
            access.set_button_icon(handles.transition_play, playback.playing() ? icons.pause : icons.play);
#else
            (void)this;
#endif
        }

#if CHARM_PLAYER_LAYERED_TRANSITIONS
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
        .start_ms = monotonic_now_ms(),
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
            return std::min<int>(static_cast<int>(rect.h / 2), 30);
        }

        static int mini_transition_cover_corner_radius(const Rect& rect) noexcept {
            return std::min<int>(static_cast<int>(rect.w / 3), 18);
        }

        static int mini_transition_play_corner_radius(const Rect& rect) noexcept {
            return std::max<int>(static_cast<int>(rect.h / 2), 18);
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
            patch.padding = std::max<int>(0, static_cast<int>((geometry.play.w - 32) / 2));
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

        void set_page_transition_state(PageTransitionState state) noexcept {
            page_transition_state = state;
        }

        static std::uint32_t transition_timing_delta_us(std::uint64_t start,
                                                        std::uint64_t end) noexcept {
            constexpr std::uint64_t kMaxU32 = 0xFFFFFFFFULL;
            const auto delta = end >= start ? end - start : 0ULL;
            return end >= start
                ? static_cast<std::uint32_t>(delta > kMaxU32 ? kMaxU32 : delta)
                : 0U;
        }

        std::uint64_t transition_now_us() const noexcept {
            return monotonic_now_us();
        }

        void reset_transition_capture_timing() noexcept {
            layer_transition_capture_source_us = 0;
            layer_transition_prepare_destination_us = 0;
            layer_transition_capture_destination_us = 0;
            layer_transition_first_compose_us = 0;
            layer_transition_destination_capture_defer_frames = 0;
            layer_transition_destination_capture_start_us = 0;
            layer_transition_first_compose_recorded = false;
        }

        void start_now_playing_transition_reveal() noexcept {
            const auto& endpoints = now_playing_transition.route;
            now_playing_transition.reveal_started = true;
            set_page_root_visible(endpoints.source_page, false);
            set_page_root_visible(endpoints.destination_page,
                                  !now_playing_transition.destination_snapshot);
            if (!now_playing_transition.destination_refreshed) {
                refresh_page(endpoints.destination_page);
                now_playing_transition.destination_refreshed = true;
            }
            if (!endpoints.expanding) {
                set_mini_bar_chrome_visible(false);
            }
        }

        static ::ui::scene::SnapshotSpec make_transition_snapshot_spec() noexcept {
            return ::ui::scene::SnapshotSpec{
                .bounds = Rect{0, 0, screen_width, screen_height},
                .preferred_kind = ::ui::scene::SnapshotKind::PixelSurface,
            };
        }

        PlayerLayerCaptureResult freeze_transition_page_snapshot(
            PlayerPage page,
            bool hide_live_root) noexcept {
            last_layer_transition_capture_status = ::ui::scene::LayerCaptureStatus::NoSnapshotSlot;
            if (!scene_runtime.valid()) return {};
            auto binding = page_binding_for(page);
            if (!binding.layer) return {};
            set_page_transition_state(page == now_playing_transition.route.source_page
                ? PageTransitionState::CapturingSource
                : PageTransitionState::CapturingDestination);
            const auto capture = binding.layer->freeze(scene_runtime, make_transition_snapshot_spec());
            last_layer_transition_capture_status = capture.status;
            if (capture.ok()) {
                binding.layer->mark_transitioning();
                if (hide_live_root) {
                    binding.layer->hide(access);
                    binding.layer->mark_transitioning();
                }
            }
            return capture;
        }

        ::ui::scene::SnapshotHandle capture_transition_source_snapshot() noexcept {
            const auto start = transition_now_us();
            const auto capture = freeze_transition_page_snapshot(
                now_playing_transition.route.source_page,
                true);
            const auto end = transition_now_us();
            layer_transition_capture_source_us = transition_timing_delta_us(start, end);
            return capture.handle;
        }

        void note_transition_capture_failure() noexcept {
            ++layer_transition_capture_fail_count;
            switch (last_layer_transition_capture_status) {
            case ::ui::scene::LayerCaptureStatus::NoSnapshotSlot:
                ++layer_transition_capture_no_slot_count;
                break;
            case ::ui::scene::LayerCaptureStatus::RecordFailed:
                ++layer_transition_capture_record_fail_count;
                break;
            case ::ui::scene::LayerCaptureStatus::StoreFailed:
                ++layer_transition_capture_store_fail_count;
                break;
            case ::ui::scene::LayerCaptureStatus::Ok:
                break;
            }
        }

        void release_transition_source_snapshot() noexcept {
            if (!scene_runtime.valid() || !now_playing_transition.source_snapshot) return;
            auto binding = page_binding_for(now_playing_transition.route.source_page);
            const bool released = binding.layer
                && binding.layer->snapshot() == now_playing_transition.source_snapshot
                    ? binding.layer->release_snapshot(scene_runtime)
                    : scene_runtime.release_snapshot(now_playing_transition.source_snapshot);
            if (released) {
                ++layer_transition_release_count;
            }
            now_playing_transition.source_snapshot = {};
        }

        void release_transition_destination_snapshot() noexcept {
            if (!scene_runtime.valid() || !now_playing_transition.destination_snapshot) return;
            auto binding = page_binding_for(now_playing_transition.route.destination_page);
            const bool released = binding.layer
                && binding.layer->snapshot() == now_playing_transition.destination_snapshot
                    ? binding.layer->release_snapshot(scene_runtime)
                    : scene_runtime.release_snapshot(now_playing_transition.destination_snapshot);
            if (released) {
                ++layer_transition_release_count;
            }
            now_playing_transition.destination_snapshot = {};
        }

        ::ui::scene::SnapshotHandle capture_transition_pixel_snapshot() noexcept {
            const auto capture = freeze_transition_page_snapshot(
                now_playing_transition.route.destination_page,
                true);
            return capture.handle;
        }

        static ::ui::scene::LayerBudget make_transition_layer_budget(
            ::ui::scene::LayerProfile profile) noexcept {
            switch (profile) {
            case ::ui::scene::LayerProfile::Rich:
                return {
                    .max_layer_bytes = 4u * 1024u * 1024u,
                    .max_composite_pixels = static_cast<std::uint32_t>(screen_width * screen_height * 2),
                    .max_command_count = 0,
                };
            case ::ui::scene::LayerProfile::Cheap:
                return {
                    .max_layer_bytes = 2u * 1024u * 1024u,
                    .max_composite_pixels = static_cast<std::uint32_t>(screen_width * screen_height),
                    .max_command_count = 0,
                };
            case ::ui::scene::LayerProfile::Static:
            case ::ui::scene::LayerProfile::Eink:
            case ::ui::scene::LayerProfile::None:
                return {
                    .max_layer_bytes = 1u * 1024u * 1024u,
                    .max_composite_pixels = static_cast<std::uint32_t>(screen_width * screen_height),
                    .max_command_count = 0,
                };
            }
            return {};
        }

        ::ui::scene::LayerBudget make_effective_transition_layer_budget(
            ::ui::scene::LayerProfile profile) const noexcept {
            auto budget = make_transition_layer_budget(profile);
            if (layer_profile_budget_drill_enabled) {
                budget.max_layer_bytes = 1u * 1024u * 1024u;
                budget.max_composite_pixels =
                    static_cast<std::uint32_t>(screen_width * screen_height);
            }
            return budget;
        }

        ::ui::scene::LayerAdmission decide_transition_layer_admission() const noexcept {
            const auto budget = make_effective_transition_layer_budget(requested_layer_profile);
            const auto pixel_bytes = static_cast<std::uint32_t>(
                ::ui::scene::snapshot_pixel_bytes(
                    screen_pixel_format, screen_width, screen_height));
            return ::ui::scene::decide_layer_admission({
                .profile = requested_layer_profile,
                .budget = budget,
                .pixel_snapshot_bytes = pixel_bytes,
                .cache_slots = layer_cache_slots,
                .need_double_snapshot = true,
            });
        }

        std::uint32_t estimate_transition_composite_pixels() const noexcept {
            const auto full_screen_pixels =
                static_cast<std::uint32_t>(screen_width * screen_height);
            std::uint32_t layers{0};
            if (now_playing_transition.source_snapshot) {
                ++layers;
            }
            if (now_playing_transition.destination_snapshot) {
                ++layers;
            }
            return full_screen_pixels * layers;
        }

        ::ui::scene::LayerBudgetResult check_transition_layer_budget(
            const ::ui::scene::LayerBudget& budget,
            std::uint32_t composite_pixels) const noexcept {
            ::ui::scene::LayerBudgetResult result{};
            if (!scene_runtime.valid()) return result;
            const auto stats = scene_runtime.layer_stats();
            if (budget.max_layer_bytes > 0 && stats.layer_bytes > budget.max_layer_bytes) {
                result.layer_bytes_over = true;
            }
            if (budget.max_composite_pixels > 0 &&
                composite_pixels > budget.max_composite_pixels) {
                result.composite_pixels_over = true;
            }
            result.ok = !result.layer_bytes_over
                && !result.composite_pixels_over
                && !result.command_count_over;
            return result;
        }

        void decide_transition_layer_profile() noexcept {
            const auto budget_caps = make_effective_transition_layer_budget(requested_layer_profile);
            const auto composite_pixels = estimate_transition_composite_pixels();
            const auto budget = check_transition_layer_budget(budget_caps, composite_pixels);
            const auto decision = ::ui::scene::decide_layer_profile(
                requested_layer_profile, budget);
            effective_layer_profile = decision.effective;
            layer_transition_fallback_reason = decision.reason;
            if (decision.reason != ::ui::scene::LayerFallbackReason::None) {
                ++layer_transition_budget_fail_count;
            }
            if (scene_runtime.valid()) {
                layer_transition_last_layer_bytes = scene_runtime.layer_stats().layer_bytes;
            }
            layer_transition_last_layer_bytes_budget = budget_caps.max_layer_bytes;
            layer_transition_last_composite_pixels = composite_pixels;
            layer_transition_last_composite_pixels_budget = budget_caps.max_composite_pixels;
            layer_transition_last_budget_ok = budget.ok;
#if CHARM_PLAYER_DEBUG_UI
            std::printf("[layer] profile requested=%s effective=%s reason=%s bytes=%u\n",
                        ::ui::scene::layer_profile_name(requested_layer_profile),
                        ::ui::scene::layer_profile_name(effective_layer_profile),
                        ::ui::scene::layer_fallback_reason_name(layer_transition_fallback_reason),
                        static_cast<unsigned>(layer_transition_last_layer_bytes));
            std::printf("[budget] layer_bytes=%u/%u ok=%d composite_pixels=%u/%u ok=%d\n",
                        static_cast<unsigned>(layer_transition_last_layer_bytes),
                        static_cast<unsigned>(layer_transition_last_layer_bytes_budget),
                        budget.layer_bytes_over ? 0 : 1,
                        static_cast<unsigned>(composite_pixels),
                        static_cast<unsigned>(layer_transition_last_composite_pixels_budget),
                        budget.composite_pixels_over ? 0 : 1);
#endif
        }

        bool transition_uses_static_cut() const noexcept {
            return effective_layer_profile == ::ui::scene::LayerProfile::Static
                || effective_layer_profile == ::ui::scene::LayerProfile::Eink
                || effective_layer_profile == ::ui::scene::LayerProfile::None;
        }

        bool transition_admission_uses_static_cut() const noexcept {
            return last_layer_admission == ::ui::scene::LayerAdmission::CommandSnapshot
                || last_layer_admission == ::ui::scene::LayerAdmission::StaticCut
                || last_layer_admission == ::ui::scene::LayerAdmission::Reject;
        }

        bool transition_uses_destination_snapshot() const noexcept {
            return now_playing_transition.active;
        }

        ::ui::scene::LayerComposePlan make_transition_snapshot_plan(
            ::ui::scene::SnapshotHandle snapshot,
            float progress,
            bool destination) noexcept {
            if (!scene_runtime.valid() || !snapshot) return {};
            const auto phase = sample_now_playing_transition_frame_phase(
                now_playing_transition, progress);
            const auto requested_opacity = static_cast<std::uint8_t>(
                destination ? 255 : lerp_u8(255, 0, phase.toward_destination));
            return scene_runtime.make_snapshot_compose_plan({
                .source = snapshot,
                .transform = ::ui::scene::LayerTransform{
                    .x = 0,
                    .y = 0,
                    .opacity = ::ui::scene::resolve_layer_opacity(
                        effective_layer_profile, requested_opacity),
                },
                .clip = Rect{0, 0, screen_width, screen_height},
                .has_clip = true,
            });
        }

        ::ui::scene::LayerComposePlan make_transition_source_snapshot_plan(float progress) noexcept {
            return make_transition_snapshot_plan(
                now_playing_transition.source_snapshot,
                progress,
                false);
        }

        ::ui::scene::LayerComposePlan make_transition_destination_snapshot_plan(float progress) noexcept {
            return make_transition_snapshot_plan(
                now_playing_transition.destination_snapshot,
                progress,
                true);
        }

        void sample_transition_source_snapshot_compose(float progress) noexcept {
            const auto plan = make_transition_source_snapshot_plan(progress);
            if (plan.valid) {
                ++layer_transition_compose_count;
                layer_transition_composite_pixels += plan.composite_pixels;
            } else if (now_playing_transition.source_snapshot) {
                ++layer_transition_compose_fail_count;
            }
        }

        void compose_transition_pixel_layer(float progress,
                                            bool destination) noexcept {
            if (!scene_runtime.valid() || !now_playing_transition.active) return;
            const auto plan = destination
                ? make_transition_destination_snapshot_plan(progress)
                : make_transition_source_snapshot_plan(progress);
            if (!plan.valid) return;
            const auto replay = scene_runtime.compose_pixel_snapshot(plan);
            if (replay.ok()) {
                layer_transition_alpha_blend_pixels += static_cast<std::uint32_t>(
                    replay.alpha_blend_count);
                layer_transition_last_composite_pixels = plan.composite_pixels;
                if (destination) {
                    ++layer_transition_destination_compose_count;
                    layer_transition_destination_compose_pixels += plan.composite_pixels;
                } else {
                    ++layer_transition_pixel_compose_count;
                    layer_transition_pixel_compose_pixels += plan.composite_pixels;
                }
            }
        }

        void compose_now_playing_transition_pixel_layer() noexcept {
            if (!now_playing_transition.active) return;
            set_page_transition_state(PageTransitionState::Composing);
    const auto now_ms = monotonic_now_ms();
            const auto tick = sample_now_playing_transition_tick_phase(
                now_playing_transition, now_ms);
            const auto start = transition_now_us();
            compose_transition_pixel_layer(tick.progress, true);
            if (!now_playing_transition.destination_snapshot) {
                compose_transition_pixel_layer(tick.progress, false);
            }
            const auto end = transition_now_us();
            if (!layer_transition_first_compose_recorded) {
                layer_transition_first_compose_us = transition_timing_delta_us(start, end);
                layer_transition_first_compose_recorded = true;
            }
        }

        bool transition_needs_destination_snapshot() const noexcept {
            return now_playing_transition.active
                && transition_uses_destination_snapshot()
                && !now_playing_transition.destination_snapshot;
        }

        bool transition_destination_snapshot_ready_to_capture() const noexcept {
            return transition_needs_destination_snapshot()
                && now_playing_transition.destination_capture_ready;
        }

        PlayerPage transition_destination_page() const noexcept {
            return now_playing_transition.route.destination_page;
        }

        void prepare_transition_destination_snapshot_scene() noexcept {
            if (!transition_destination_snapshot_ready_to_capture()) return;
            const auto start = transition_now_us();
            set_page_transition_state(PageTransitionState::PreparingDestination);
            const auto destination = now_playing_transition.route.destination_page;
            set_page_root_visible(now_playing_transition.route.source_page, false);
            set_page_root_visible(destination, true);
            sync_page_chrome(destination);
            refresh_page(destination);
            set_now_playing_transition_root_visible(false);
            const auto end = transition_now_us();
            layer_transition_prepare_destination_us = transition_timing_delta_us(start, end);
            layer_transition_destination_capture_start_us = end;
        }

        void finish_transition_destination_snapshot_capture() noexcept {
            if (!transition_destination_snapshot_ready_to_capture()) return;
            const auto start = transition_now_us();
            set_page_transition_state(PageTransitionState::CapturingDestination);
            now_playing_transition.destination_snapshot = capture_transition_pixel_snapshot();
            const auto end = transition_now_us();
            layer_transition_capture_destination_us = transition_timing_delta_us(
                layer_transition_destination_capture_start_us != 0
                    ? layer_transition_destination_capture_start_us
                    : start,
                end);
            layer_transition_destination_capture_start_us = 0;
            if (now_playing_transition.destination_snapshot) {
                ++layer_transition_capture_count;
                ++layer_transition_destination_capture_count;
                set_now_playing_transition_root_visible(true);
                decide_transition_layer_profile();
                if (transition_uses_static_cut()) {
                    finish_now_playing_transition_static_cut();
                    return;
                }
                set_page_transition_state(PageTransitionState::Composing);
            } else {
                note_transition_capture_failure();
                set_now_playing_transition_root_visible(true);
                finish_now_playing_transition_static_cut();
            }
        }

        void schedule_transition_destination_snapshot_capture() noexcept {
            if (!transition_needs_destination_snapshot()
                || now_playing_transition.destination_capture_ready) {
                return;
            }
            ++layer_transition_destination_capture_defer_frames;
            now_playing_transition.destination_capture_ready = true;
        }

        void set_now_playing_transition_root_visible(bool visible) noexcept {
            set_handle_visible(handles.transition_root, visible);
        }

        void begin_now_playing_transition_lifecycle() noexcept {
            const auto& endpoints = now_playing_transition.route;
            reset_transition_capture_timing();
            set_page_transition_state(PageTransitionState::CapturingSource);
            now_playing_transition.source_snapshot = capture_transition_source_snapshot();
            if (now_playing_transition.source_snapshot) {
                ++layer_transition_capture_count;
                set_page_root_visible(endpoints.source_page, false);
            } else {
                note_transition_capture_failure();
            }
            sample_transition_source_snapshot_compose(0.0f);
            sync_now_playing_transition_overlay();
            if (endpoints.expanding) {
                set_page_root_visible(endpoints.destination_page, false);
            } else {
                set_page_root_visible(endpoints.destination_page, false);
                set_mini_bar_chrome_visible(false);
            }
            set_now_playing_transition_root_visible(true);
            update_now_playing_transition_group_layout(0.0f);
            update_now_playing_transition_group_visibility(0.0f);
            apply_now_playing_transition_frame(0.0f);
            if (endpoints.expanding) {
                set_mini_bar_chrome_visible(false);
            }
            if (!transition_needs_destination_snapshot()) {
                decide_transition_layer_profile();
                if (transition_uses_static_cut()) {
                    finish_now_playing_transition_static_cut();
                    return;
                }
            }
            set_page_transition_state(PageTransitionState::Composing);
        }

        void restore_now_playing_transition_scene_state() noexcept {
            release_transition_source_snapshot();
            release_transition_destination_snapshot();
            restore_now_playing_group_layout();
            restore_now_playing_group_visibility();
            restore_bottom_bar_content_visibility();
            set_now_playing_transition_root_visible(false);
        }

        void restore_now_playing_transition_live_roots(PlayerPage visible_page) noexcept {
            if (now_playing_transition.active) {
                set_page_root_visible(now_playing_transition.route.source_page, false);
                set_page_root_visible(now_playing_transition.route.destination_page, false);
            }
            set_page_root_visible(visible_page, true);
        }

        void abort_now_playing_transition(PlayerPage fallback_page) noexcept {
            if (!now_playing_transition.active) return;
            set_page_transition_state(PageTransitionState::Aborting);
            restore_now_playing_transition_scene_state();
            restore_now_playing_transition_live_roots(fallback_page);
            now_playing_transition = {};
            sync_page_chrome(fallback_page);
            ++layer_transition_abort_count;
            set_page_transition_state(PageTransitionState::Idle);
        }
#endif

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
#if CHARM_PLAYER_LYRICS
            set_handle_visible(handles.now_cover_plate, cover_plate_visible && !lyrics_visible);
            set_handle_visible(handles.cover, cover_widgets_visible && !lyrics_visible);
            set_handle_visible(handles.now_lyrics_panel, cover && lyrics_visible);
#else
            set_handle_visible(handles.now_cover_plate, cover_plate_visible);
            set_handle_visible(handles.cover, cover_widgets_visible);
#endif
            set_handle_visible(handles.now_text_group, text);
            set_handle_visible(handles.now_progress_group, progress);
            set_handle_visible(handles.controls, controls_visible);
            set_handle_visible(handles.now_aux_group, aux);
        }

        void restore_now_playing_group_visibility() noexcept {
            set_now_playing_group_visibility(true, true, true, true, true, true, true);
            sync_lyrics_visibility();
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

#if CHARM_PLAYER_LAYERED_TRANSITIONS
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
            set_page_transition_state(PageTransitionState::Finishing);
            set_page(now_playing_transition.route.destination_page);
            restore_now_playing_transition_scene_state();
            now_playing_transition = {};
            set_page_transition_state(PageTransitionState::Idle);
        }

        void finish_now_playing_transition_static_cut() noexcept {
            if (!now_playing_transition.active) return;
            ++layer_static_cut_count;
            set_page_transition_state(PageTransitionState::Finishing);
            set_page(now_playing_transition.route.destination_page);
            restore_now_playing_transition_scene_state();
            now_playing_transition = {};
            set_page_transition_state(PageTransitionState::Idle);
        }

        void finish_now_playing_transition_admission_static_cut() noexcept {
            if (!now_playing_transition.active) return;
            ++layer_admission_static_cut_count;
            if (requested_layer_profile == ::ui::scene::LayerProfile::Static ||
                requested_layer_profile == ::ui::scene::LayerProfile::Eink ||
                requested_layer_profile == ::ui::scene::LayerProfile::None) {
                effective_layer_profile = requested_layer_profile;
                layer_transition_fallback_reason = ::ui::scene::LayerFallbackReason::None;
                layer_transition_last_budget_ok = true;
            } else {
                effective_layer_profile = ::ui::scene::LayerProfile::Static;
                layer_transition_fallback_reason = ::ui::scene::LayerFallbackReason::LayerBytesOver;
                layer_transition_last_budget_ok = false;
                ++layer_transition_budget_fail_count;
            }
#if CHARM_PLAYER_DEBUG_UI
            std::printf("[layer] admission=%s action=static_cut\n",
                        ::ui::scene::layer_admission_name(last_layer_admission));
#endif
            finish_now_playing_transition_static_cut();
        }

        void tick_now_playing_transition(std::uint64_t now_ms) noexcept {
            if (!now_playing_transition.active) return;
            const auto tick = sample_now_playing_transition_tick_phase(now_playing_transition, now_ms);
            const auto destination_page = now_playing_transition.route.destination_page;
            if (tick.should_start_reveal) {
                start_now_playing_transition_reveal();
            }
            if (tick.should_refresh_destination) {
                if (!now_playing_transition.destination_refreshed) {
                    refresh_page(destination_page);
                    now_playing_transition.destination_refreshed = true;
                    if (!now_playing_transition.route.expanding) {
                        set_mini_bar_chrome_visible(false);
                    }
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
            last_layer_admission = decide_transition_layer_admission();
            if (transition_admission_uses_static_cut()) {
                finish_now_playing_transition_admission_static_cut();
                return true;
            }
            set_page_transition_state(PageTransitionState::CapturingSource);
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
#else
        void set_now_playing_transition_root_visible(bool visible) noexcept {
            set_handle_visible(handles.transition_root, visible);
        }

        void abort_now_playing_transition(PlayerPage fallback_page) noexcept {
            (void)fallback_page;
        }

        void open_now_playing_page() noexcept {
            set_page(PlayerPage::NowPlaying);
        }

        void close_now_playing_page() noexcept {
            set_page(return_page);
        }
#endif

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
#if CHARM_PLAYER_LAYERED_TRANSITIONS
            if (handles.transition_play) {
                access.set_button_icon(handles.transition_play, playing_now ? icons.pause : icons.play);
            }
#endif
        }

        int resolved_duration_sec() const noexcept {
            if (!track_preloaded) return 0;
            return std::max(playback.resolved_duration_sec(), 0);
        }

        bool progress_duration_available() const noexcept {
            return resolved_duration_sec() > 0;
        }

        void set_time_label(int elapsed_sec) {
            const int total = resolved_duration_sec();
            const int max_elapsed = (total > 0) ? total : std::max(elapsed_sec, 0);
            const int clamped = std::clamp(elapsed_sec, 0, max_elapsed);
            if (clamped == last_time_sec && total == last_time_total_sec) return;
            last_time_sec = clamped;
            last_time_total_sec = total;
            const int cur_m = clamped / 60;
            const int cur_s = clamped % 60;
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
            const int total = resolved_duration_sec();
            if (!track_preloaded || total <= 0) {
                set_info_label("");
                return;
            }
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

        static const char* progress_interaction_state_text(ProgressInteractionState state) noexcept {
            switch (state) {
            case ProgressInteractionState::DraggingPreview: return "Seeking...";
            case ProgressInteractionState::SeekCommitted: return "Seek queued";
            case ProgressInteractionState::Unavailable: return "";
            case ProgressInteractionState::Idle:
            default:
                return nullptr;
            }
        }

        void set_progress_interaction_state(ProgressInteractionState state) {
            if (progress_interaction_state == state) return;
            progress_interaction_state = state;
            apply_now_progress_interaction_style();
        }

        ProgressInteractionState resolved_progress_interaction_state() const noexcept {
            if (!progress_duration_available()) return ProgressInteractionState::Unavailable;
            return progress_interaction_state == ProgressInteractionState::Unavailable
                ? ProgressInteractionState::Idle
                : progress_interaction_state;
        }

        void apply_now_progress_interaction_style() noexcept {
            if (!access.valid()) return;
            const auto& roles = now_playing_roles;
            const ProgressInteractionState state = resolved_progress_interaction_state();
            player::cover_theme::SurfaceRole info_role = roles.info_tag;
            rgba track = roles.progress.track;
            rgba fill = roles.progress.fill;
            std::uint8_t bg_alpha = 112;

            if (state == ProgressInteractionState::DraggingPreview) {
                fill = blend_on(roles.primary_control.bg, roles.progress.fill, 210);
                track = blend_on(roles.title, roles.progress.track, 92);
                info_role.bg = blend_on(roles.primary_control.bg, roles.info_tag.bg, 224);
                info_role.border = with_alpha(roles.primary_control.fg, 82);
                info_role.fg = roles.primary_control.fg;
                bg_alpha = 150;
            } else if (state == ProgressInteractionState::SeekCommitted) {
                info_role.bg = blend_on(roles.progress.fill, roles.info_tag.bg, 172);
                info_role.border = with_alpha(roles.progress.fill, 74);
                info_role.fg = roles.info_tag.fg;
                bg_alpha = 132;
            } else if (state == ProgressInteractionState::Unavailable) {
                track = with_alpha(blend_on(roles.progress.track, roles.page_backdrop, 120), 132);
                fill = with_alpha(roles.progress.fill, 72);
                info_role.bg = with_alpha(blend_on(roles.info_tag.bg, roles.page_backdrop, 180), 126);
                info_role.border = with_alpha(roles.info_tag.border, 32);
                info_role.fg = with_alpha(roles.info_tag.fg, 156);
                bg_alpha = 72;
            }

            if (handles.info_tag) {
                access.set_style_override(handles.info_tag, make_now_surface_role_patch(info_role, 16, 10, false));
            }

            if (handles.progress_visual) {
                StylePatch prog = ::ui::scene::make_seek_bar_style_patch({
                    .bg_color = blend_on(track, roles.page_backdrop, bg_alpha),
                    .track_color = track,
                    .fill_color = fill,
                    .padding = 4,
                    .corner_radius = 11,
                });
                prog.has_border_color = true;
                prog.border_color = track;
                prog.has_accent_color = true;
                prog.accent_color = fill;
                access.set_style_override(handles.progress_visual, prog);
            }
        }

        void set_progress_info_for_state(ProgressInteractionState state) {
            if (const char* text = progress_interaction_state_text(state)) {
                set_info_label(text);
                return;
            }
            update_info_label();
        }

        void sync_progress_unavailable() {
            progress_dragging = false;
            progress_drag_value = 0;
            progress_drag_sec = 0;
            progress_committed_sec = 0;
            progress_committed_hold_frames = 0;
            set_progress_interaction_state(ProgressInteractionState::Unavailable);
            sync_progress_value(0);
            set_time_label(0);
            set_info_label("");
            refresh_lyrics_window();
        }

        int progress_value_from_sec(int sec) const noexcept {
            const int duration_sec = resolved_duration_sec();
            if (duration_sec <= 0) return 0;
            const int clamped = std::clamp(sec, 0, duration_sec);
            return static_cast<int>((static_cast<std::int64_t>(clamped) * 100) / duration_sec);
        }

        void sync_progress_idle(int elapsed_sec) {
            if (!progress_duration_available()) {
                sync_progress_unavailable();
                return;
            }
            if (progress_interaction_state == ProgressInteractionState::DraggingPreview) {
                return;
            }
            if (progress_interaction_state == ProgressInteractionState::SeekCommitted
                && progress_committed_hold_frames > 0) {
                --progress_committed_hold_frames;
                const int committed_sec = std::clamp(progress_committed_sec, 0, resolved_duration_sec());
                sync_progress_value(progress_value_from_sec(committed_sec));
                set_time_label(committed_sec);
                set_progress_info_for_state(ProgressInteractionState::SeekCommitted);
                refresh_lyrics_window();
                return;
            }
            progress_committed_sec = 0;
            progress_committed_hold_frames = 0;
            set_progress_interaction_state(ProgressInteractionState::Idle);
            sync_progress_value(progress_value_from_sec(elapsed_sec));
            set_time_label(elapsed_sec);
            update_info_label();
            refresh_lyrics_window();
        }

        void reset_duration() {
            playback.reset_duration();
            last_time_sec = -1;
            last_time_total_sec = -1;
            sync_progress_unavailable();
        }

        static const char* play_mode_text(PlaybackMode mode) noexcept {
            return playback_mode_text(mode);
        }

        void refresh_now_playing_runtime_content() {
            if (track_preloaded) {
                update_duration_from_player();
            }
            if (!update_progress()) {
                sync_progress_idle(playback.current_sec());
            }
            refresh_lyrics_window();
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
            const auto mode = current_playback_mode();
            std::snprintf(buf, sizeof(buf), "Mode: %s", play_mode_text(mode));
            if (last_mode_text.view() == buf) return;
            last_mode_text.assign(buf);
            set_label_slot(handles.mode_hint, text_slots.mode_hint, buf);
            if (!access.valid()) return;
            auto icon = icons.loop;
            if (mode == PlaybackMode::RepeatOne) icon = icons.single;
            else if (mode == PlaybackMode::Shuffle) icon = icons.shuffle;
            access.set_button_icon(handles.btn_mode, icon);
        }

        void refresh_now_playing() {
            sync_page_chrome(PlayerPage::NowPlaying);
            refresh_now_playing_body_content();
            update_debug_overlay();
        }
