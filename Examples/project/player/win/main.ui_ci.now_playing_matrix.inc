    template<typename SceneT>
    Rect ui_ci_handle_rect(const SceneT& scene, WidgetHandle h) noexcept {
        return h ? scene.world_rect(h) : Rect{};
    }

    static int ui_ci_abs_i(int value) noexcept {
        return value < 0 ? -value : value;
    }

    static int ui_ci_rect_right(const Rect& r) noexcept {
        return r.x + r.w;
    }

    static int ui_ci_rect_bottom(const Rect& r) noexcept {
        return r.y + r.h;
    }

    static int ui_ci_rect_center_x(const Rect& r) noexcept {
        return r.x + r.w / 2;
    }

    static int ui_ci_rect_center_y(const Rect& r) noexcept {
        return r.y + r.h / 2;
    }

    static bool ui_ci_rect_nonempty(const Rect& r, int min_w = 1, int min_h = 1) noexcept {
        return r.w >= min_w && r.h >= min_h;
    }

    static bool ui_ci_rect_inside_screen(const Rect& r, int margin = 0) noexcept {
        return r.w > 0 && r.h > 0
            && r.x >= margin
            && r.y >= margin
            && ui_ci_rect_right(r) <= screen_width - margin
            && ui_ci_rect_bottom(r) <= screen_height - margin;
    }

    static bool ui_ci_rect_contains(const Rect& outer, const Rect& inner, int tolerance = 0) noexcept {
        return outer.w > 0 && outer.h > 0 && inner.w > 0 && inner.h > 0
            && outer.x - tolerance <= inner.x
            && outer.y - tolerance <= inner.y
            && ui_ci_rect_right(outer) + tolerance >= ui_ci_rect_right(inner)
            && ui_ci_rect_bottom(outer) + tolerance >= ui_ci_rect_bottom(inner);
    }

    static bool ui_ci_rect_below(const Rect& upper, const Rect& lower, int min_gap) noexcept {
        return upper.w > 0 && upper.h > 0 && lower.w > 0 && lower.h > 0
            && lower.y >= ui_ci_rect_bottom(upper) + min_gap;
    }

    static bool ui_ci_rect_equal(const Rect& a, const Rect& b, int tolerance = 0) noexcept {
        return ui_ci_abs_i(a.x - b.x) <= tolerance
            && ui_ci_abs_i(a.y - b.y) <= tolerance
            && ui_ci_abs_i(a.w - b.w) <= tolerance
            && ui_ci_abs_i(a.h - b.h) <= tolerance;
    }

    template<typename SceneT>
    bool ui_ci_rect_valid(const SceneT& scene, WidgetHandle h, int min_w = 1, int min_h = 1) noexcept {
        if (!h) return false;
        return ui_ci_rect_nonempty(ui_ci_handle_rect(scene, h), min_w, min_h);
    }

    template<typename SceneT>
    bool ui_ci_kind_is(SceneT& scene, WidgetHandle h, WidgetKind kind) noexcept {
        if (!h) return false;
        return scene.access().kind(h) == kind;
    }

    template<typename SceneT, typename ContextT>
    const char* ui_ci_now_playing_structure_failure(SceneT& scene, const ContextT& ctx) noexcept {
        const auto& h = ctx.handles;
        if (ctx.current_page != player::PlayerPage::NowPlaying) return "page";
        if (!ctx.page_now_layer.visible()) return "now_layer_hidden";
        if (!ctx.page_now_layer.live()) return "now_layer_not_live";
        if (!ui_ci_kind_is(scene, h.now_backdrop, WidgetKind::Container)) return "now_backdrop_kind";
        if (!ui_ci_kind_is(scene, h.now_cover_plate, WidgetKind::Container)) return "now_cover_plate_kind";
        if (!ui_ci_kind_is(scene, h.cover, WidgetKind::Image)) return "cover_kind";
        if (!ui_ci_kind_is(scene, h.title, WidgetKind::Label)) return "title_kind";
        if (!ui_ci_kind_is(scene, h.subtitle, WidgetKind::Label)) return "subtitle_kind";
        if (!ui_ci_kind_is(scene, h.now_progress_group, WidgetKind::Container)) return "progress_group_kind";
        if (!ui_ci_kind_is(scene, h.progress_visual, WidgetKind::Slider)) return "progress_visual_kind";
        if (!ui_ci_kind_is(scene, h.time_left, WidgetKind::Label)) return "time_left_kind";
        if (!ui_ci_kind_is(scene, h.time_right, WidgetKind::Label)) return "time_right_kind";
        if (!ui_ci_kind_is(scene, h.info_tag, WidgetKind::Button)) return "info_tag_kind";
        if (!ui_ci_kind_is(scene, h.btn_prev, WidgetKind::Button)) return "btn_prev_kind";
        if (!ui_ci_kind_is(scene, h.btn_pause, WidgetKind::Button)) return "btn_pause_kind";
        if (!ui_ci_kind_is(scene, h.btn_next, WidgetKind::Button)) return "btn_next_kind";
        if (!ui_ci_rect_valid(scene, h.now_backdrop, screen_width, screen_height)) return "now_backdrop_rect";
        if (!ui_ci_rect_valid(scene, h.now_cover_plate, 300, 300)) return "now_cover_plate_rect";
        if (!ui_ci_rect_valid(scene, h.cover, 300, 300)) return "cover_rect";
        if (!ui_ci_rect_valid(scene, h.title, 200, 24)) return "title_rect";
        if (!ui_ci_rect_valid(scene, h.subtitle, 120, 20)) return "subtitle_rect";
        if (!ui_ci_rect_valid(scene, h.now_progress_group, 300, 40)) return "progress_group_rect";
        if (!ui_ci_rect_valid(scene, h.progress_visual, 240, 12)) return "progress_visual_rect";
        if (!ui_ci_rect_valid(scene, h.btn_pause, 90, 90)) return "btn_pause_rect";
        return nullptr;
    }

    template<typename SceneT, typename ContextT>
    bool ui_ci_now_playing_structure_ok(SceneT& scene, const ContextT& ctx) noexcept {
        return ui_ci_now_playing_structure_failure(scene, ctx) == nullptr;
    }

    template<typename SceneT>
    const char* ui_ci_text_value(const SceneT& scene, WidgetHandle h) noexcept {
        const char* text = scene.text(h);
        return text ? text : "";
    }

    template<typename SceneT, typename ContextT>
    bool ui_ci_now_playing_text_sync_ok(const SceneT& scene, const ContextT& ctx) noexcept {
        const std::string_view title_actual{ui_ci_text_value(scene, ctx.handles.title)};
        const std::string_view subtitle_actual{ui_ci_text_value(scene, ctx.handles.subtitle)};
        const std::string_view bottom_title_actual{ui_ci_text_value(scene, ctx.handles.bottom_title)};
        const std::string_view bottom_subtitle_actual{ui_ci_text_value(scene, ctx.handles.bottom_subtitle)};
        const std::string_view title_expected = ctx.title_text.view();
        const std::string_view subtitle_expected = ctx.subtitle_text.view();
        const bool ok = title_actual == title_expected
            && subtitle_actual == subtitle_expected
            && bottom_title_actual == title_expected
            && bottom_subtitle_actual == subtitle_expected;
        if (!ok) {
            std::printf("[ui-ci] now_playing_text title='%s'(%zu/%zu) expected='%s' "
                        "subtitle='%s'(%zu/%zu) expected='%s' "
                        "bottom_title='%s'(%zu/%zu) bottom_subtitle='%s'(%zu/%zu)\n",
                        ui_ci_text_value(scene, ctx.handles.title),
                        title_actual.size(),
                        title_expected.size(),
                        ctx.title_text.c_str(),
                        ui_ci_text_value(scene, ctx.handles.subtitle),
                        subtitle_actual.size(),
                        subtitle_expected.size(),
                        ctx.subtitle_text.c_str(),
                        ui_ci_text_value(scene, ctx.handles.bottom_title),
                        bottom_title_actual.size(),
                        title_expected.size(),
                        ui_ci_text_value(scene, ctx.handles.bottom_subtitle),
                        bottom_subtitle_actual.size(),
                        subtitle_expected.size());
        }
        return ok;
    }

    template<typename SceneT, typename ContextT>
    bool ui_ci_now_playing_cover_state_ok(const SceneT& scene, const ContextT& ctx) noexcept {
        const bool cover_rect_ok = ui_ci_rect_valid(scene, ctx.handles.cover, 300, 300)
            && ui_ci_rect_valid(scene, ctx.handles.now_cover_plate, 300, 300)
            && ui_ci_rect_valid(scene, ctx.handles.bottom_cover, 24, 24);
        if (!cover_rect_ok) return false;
        if (!ctx.current_cover.valid()) {
            return !::ui::scene::image_id_valid(ctx.current_cover.image_id)
                && ctx.current_cover.key.empty()
                && ctx.current_cover.fallback == false;
        }
        return ctx.current_cover.width > 0
            && ctx.current_cover.height > 0
            && !ctx.current_cover.key.empty();
    }

    template<typename SceneT, typename ContextT>
    bool ui_ci_now_playing_play_state_ok(const SceneT& scene, ContextT& ctx) {
        const int previous_state = ctx.last_play_button_state;
        ctx.set_play_button_text(true);
        const bool playing = ctx.last_play_button_state == 1
            && ui_ci_rect_valid(scene, ctx.handles.btn_pause, 90, 90)
            && ui_ci_rect_valid(scene, ctx.handles.bottom_play, 24, 24);
        ctx.set_play_button_text(false);
        const bool paused = ctx.last_play_button_state == 0
            && ui_ci_rect_valid(scene, ctx.handles.btn_pause, 90, 90)
            && ui_ci_rect_valid(scene, ctx.handles.bottom_play, 24, 24);
        if (previous_state == 1) {
            ctx.set_play_button_text(true);
        }
        return playing && paused;
    }

    // Main-experience closure cases validate stable page composition. They should
    // not encode PixelPlayer-specific decoration choices such as shadows or glass.
    template<typename SceneT, typename ContextT>
    const char* ui_ci_now_playing_closure_layout_stack_failure(const SceneT& scene,
                                                              const ContextT& ctx) noexcept {
        const auto& h = ctx.handles;
        const Rect page = ui_ci_handle_rect(scene, h.page_now_playing);
        const Rect cover_plate = ui_ci_handle_rect(scene, h.now_cover_plate);
        const Rect cover = ui_ci_handle_rect(scene, h.cover);
        const Rect title_group = ui_ci_handle_rect(scene, h.now_text_group);
        const Rect progress = ui_ci_handle_rect(scene, h.now_progress_group);
        const Rect controls = ui_ci_handle_rect(scene, h.controls);
        const Rect aux = ui_ci_handle_rect(scene, h.now_aux_group);
        if (!ui_ci_rect_nonempty(page, screen_width, screen_height)) return "page_rect";
        if (!ui_ci_rect_inside_screen(cover_plate, kUiPadding)) return "cover_stage_screen";
        if (!ui_ci_rect_inside_screen(title_group, kUiPadding)) return "title_group_screen";
        if (!ui_ci_rect_inside_screen(progress, kUiPadding)) return "progress_screen";
        if (!ui_ci_rect_inside_screen(controls, kUiPadding)) return "controls_screen";
        if (!ui_ci_rect_inside_screen(aux, kUiPadding)) return "aux_screen";
        if (!ui_ci_rect_contains(cover_plate, cover, 0)) return "cover_not_in_stage";
        if (!ui_ci_rect_below(cover_plate, title_group, 14)) return "title_overlap_cover";
        if (!ui_ci_rect_below(title_group, progress, 14)) return "progress_overlap_title";
        if (!ui_ci_rect_below(progress, controls, 24)) return "controls_overlap_progress";
        if (!ui_ci_rect_below(controls, aux, 18)) return "aux_overlap_controls";
        if (ui_ci_rect_bottom(aux) > screen_height - 20) return "aux_bottom";
        const int cx = screen_width / 2;
        if (ui_ci_abs_i(ui_ci_rect_center_x(cover) - cx) > 2) return "cover_not_centered";
        if (ui_ci_abs_i(ui_ci_rect_center_x(title_group) - cx) > 2) return "title_not_centered";
        if (ui_ci_abs_i(ui_ci_rect_center_x(progress) - cx) > 2) return "progress_not_centered";
        if (ui_ci_abs_i(ui_ci_rect_center_x(controls) - cx) > 2) return "controls_not_centered";
        if (ui_ci_abs_i(ui_ci_rect_center_x(aux) - cx) > 2) return "aux_not_centered";
        return nullptr;
    }

    template<typename SceneT, typename ContextT>
    const char* ui_ci_now_playing_closure_cover_stage_failure(const SceneT& scene,
                                                             const ContextT& ctx) noexcept {
        const auto& h = ctx.handles;
        const Rect stage = ui_ci_handle_rect(scene, h.now_cover_plate);
        const Rect cover = ui_ci_handle_rect(scene, h.cover);
        const Rect progress = ui_ci_handle_rect(scene, h.now_progress_group);
        if (!ui_ci_rect_nonempty(stage, 380, 380)) return "stage_too_small";
        if (!ui_ci_rect_nonempty(cover, 380, 380)) return "cover_too_small";
        if (!ui_ci_rect_contains(stage, cover, 0)) return "stage_not_containing_cover";
        constexpr int kCoverStagePadTotal = 12;
        if (stage.w - cover.w < kCoverStagePadTotal || stage.h - cover.h < kCoverStagePadTotal) {
            return "stage_pad";
        }
        if (ui_ci_abs_i(stage.w - stage.h) > 2) return "stage_not_square";
        if (ui_ci_abs_i(cover.w - cover.h) > 2) return "cover_not_square";
        if (ui_ci_abs_i(ui_ci_rect_center_x(stage) - ui_ci_rect_center_x(cover)) > 2) return "cover_x";
        if (ui_ci_abs_i(ui_ci_rect_center_y(stage) - ui_ci_rect_center_y(cover)) > 2) return "cover_y";
        if (!ui_ci_rect_below(stage, progress, 90)) return "stage_progress_gap";
        return nullptr;
    }

    template<typename SceneT, typename ContextT>
    const char* ui_ci_now_playing_closure_title_block_failure(const SceneT& scene,
                                                             const ContextT& ctx) noexcept {
        const auto& h = ctx.handles;
        const Rect group = ui_ci_handle_rect(scene, h.now_text_group);
        const Rect title = ui_ci_handle_rect(scene, h.title);
        const Rect subtitle = ui_ci_handle_rect(scene, h.subtitle);
        const Rect progress = ui_ci_handle_rect(scene, h.now_progress_group);
        if (!ui_ci_rect_nonempty(group, screen_width - kUiPadding * 2, 80)) return "group_rect";
        if (!ui_ci_rect_contains(group, title, 0)) return "title_not_in_group";
        if (!ui_ci_rect_contains(group, subtitle, 0)) return "subtitle_not_in_group";
        if (!ui_ci_rect_below(title, subtitle, 4)) return "subtitle_overlap_title";
        if (!ui_ci_rect_below(group, progress, 14)) return "progress_overlap_text";
        if (title.w != group.w || subtitle.w != group.w) return "text_width";
        if (ui_ci_abs_i(ui_ci_rect_center_x(title) - ui_ci_rect_center_x(group)) > 1) return "title_center";
        if (ui_ci_abs_i(ui_ci_rect_center_x(subtitle) - ui_ci_rect_center_x(group)) > 1) return "subtitle_center";
        return nullptr;
    }

    template<typename SceneT, typename ContextT>
    const char* ui_ci_now_playing_closure_control_hierarchy_failure(const SceneT& scene,
                                                                   const ContextT& ctx) noexcept {
        const auto& h = ctx.handles;
        const Rect controls = ui_ci_handle_rect(scene, h.controls);
        const Rect prev = ui_ci_handle_rect(scene, h.btn_prev);
        const Rect play = ui_ci_handle_rect(scene, h.btn_pause);
        const Rect next = ui_ci_handle_rect(scene, h.btn_next);
        if (!ui_ci_rect_contains(controls, prev, 0)) return "prev_not_in_row";
        if (!ui_ci_rect_contains(controls, play, 0)) return "play_not_in_row";
        if (!ui_ci_rect_contains(controls, next, 0)) return "next_not_in_row";
        if (prev.w < 90 || prev.h < 90 || next.w < 90 || next.h < 90) return "side_hitbox";
        if (play.w < 110 || play.h < 110) return "play_hitbox";
        if (play.w <= prev.w || play.w <= next.w || play.h <= prev.h || play.h <= next.h) return "play_not_primary";
        if (!(prev.x < play.x && play.x < next.x)) return "button_order";
        if (ui_ci_rect_right(prev) > play.x || ui_ci_rect_right(play) > next.x) return "button_overlap";
        if (ui_ci_abs_i(ui_ci_rect_center_y(prev) - ui_ci_rect_center_y(play)) > 2) return "prev_y";
        if (ui_ci_abs_i(ui_ci_rect_center_y(next) - ui_ci_rect_center_y(play)) > 2) return "next_y";
        const int left_gap = play.x - ui_ci_rect_right(prev);
        const int right_gap = next.x - ui_ci_rect_right(play);
        if (left_gap < 10 || right_gap < 10 || ui_ci_abs_i(left_gap - right_gap) > 2) return "button_gap";
        if (ui_ci_abs_i(ui_ci_rect_center_x(controls) - screen_width / 2) > 2) return "controls_center";
        return nullptr;
    }

    template<typename SceneT, typename ContextT, typename PumpFrameFn>
    bool ui_ci_now_playing_fallback_cover_ok(SceneT& scene, ContextT& ctx, PumpFrameFn& pump_frame) {
        const auto saved_cover = ctx.current_cover;
        const auto saved_ready = ctx.cover_ready;
        const auto saved_cover_path = ctx.cover_path;
        const auto saved_embedded = ctx.cover_embedded_path;
        const auto saved_folder = ctx.cover_folder_path;
        const auto saved_failed_embedded = ctx.cover_failed_embedded_path;
        const auto saved_failed_folder = ctx.cover_failed_folder_path;
        const auto saved_tint_path = ctx.cover_tint_path;
        const auto saved_cover_theme = ctx.cover_theme;
        const auto saved_roles = ctx.now_playing_roles;

        ctx.show_default_current_cover();
        ctx.apply_cover_theme_from_current_image();
        ctx.restore_now_playing_group_visibility();
        ctx.restore_bottom_bar_content_visibility();
        pump_frame();
        const bool fallback_ok = !ctx.current_cover.valid()
            && ctx.current_cover.key.empty()
            && ui_ci_rect_valid(scene, ctx.handles.cover, 300, 300)
            && ui_ci_rect_valid(scene, ctx.handles.now_cover_plate, 300, 300)
            && ui_ci_rect_valid(scene, ctx.handles.bottom_cover, 24, 24);

        ctx.current_cover = saved_cover;
        ctx.cover_ready = saved_ready;
        ctx.cover_path = saved_cover_path;
        ctx.cover_embedded_path = saved_embedded;
        ctx.cover_folder_path = saved_folder;
        ctx.cover_failed_embedded_path = saved_failed_embedded;
        ctx.cover_failed_folder_path = saved_failed_folder;
        ctx.cover_tint_path = saved_tint_path;
        ctx.cover_theme = saved_cover_theme;
        ctx.now_playing_roles = saved_roles;
        if (ctx.current_cover.valid()) {
            ctx.access.set_image(ctx.handles.cover, ctx.current_cover.image_id);
            ctx.access.set_image(ctx.handles.bottom_cover, ctx.current_cover.image_id);
        } else {
            ctx.show_default_current_cover();
        }
        ctx.refresh_cover_theme_dependent_visuals();
        ctx.restore_now_playing_group_visibility();
        ctx.restore_bottom_bar_content_visibility();
        pump_frame();
        return fallback_ok;
    }

    template<typename SceneT, typename ContextT, typename PumpFrameFn>
    bool ui_ci_now_playing_long_text_ok(const SceneT& scene, ContextT& ctx, PumpFrameFn& pump_frame) {
        const auto saved_title = ctx.title_text;
        const auto saved_subtitle = ctx.subtitle_text;

        ctx.title_text.assign(
            "A Very Long Now Playing Title That Must Stay Inside The Restored Cover Layout");
        ctx.subtitle_text.assign(
            "A Very Long Artist Name / Album Name Combination For Matrix Coverage");
        ctx.set_label_slot(ctx.handles.title, ctx.text_slots.title, ctx.title_text.c_str());
        ctx.set_label_slot(ctx.handles.subtitle, ctx.text_slots.subtitle, ctx.subtitle_text.c_str());
        ctx.set_label_slot(ctx.handles.bottom_title, ctx.text_slots.bottom_title, ctx.title_text.c_str());
        ctx.set_label_slot(ctx.handles.bottom_subtitle, ctx.text_slots.bottom_subtitle, ctx.subtitle_text.c_str());
        pump_frame();

        const bool long_text_ok = ui_ci_now_playing_text_sync_ok(scene, ctx)
            && ui_ci_rect_valid(scene, ctx.handles.title, 200, 24)
            && ui_ci_rect_valid(scene, ctx.handles.subtitle, 120, 20)
            && ui_ci_rect_valid(scene, ctx.handles.now_progress_group, 300, 40)
            && ui_ci_rect_valid(scene, ctx.handles.btn_pause, 90, 90);

        ctx.title_text = saved_title;
        ctx.subtitle_text = saved_subtitle;
        ctx.set_label_slot(ctx.handles.title, ctx.text_slots.title, ctx.title_text.c_str());
        ctx.set_label_slot(ctx.handles.subtitle, ctx.text_slots.subtitle, ctx.subtitle_text.c_str());
        ctx.set_label_slot(ctx.handles.bottom_title, ctx.text_slots.bottom_title, ctx.title_text.c_str());
        ctx.set_label_slot(ctx.handles.bottom_subtitle, ctx.text_slots.bottom_subtitle, ctx.subtitle_text.c_str());
        pump_frame();
        return long_text_ok;
    }

    template<typename SceneT, typename ContextT, typename PumpFrameFn>
    bool ui_ci_now_playing_library_sync_ok(const SceneT& scene, ContextT& ctx, PumpFrameFn& pump_frame) {
        if (!ctx.storage.tracks || ctx.storage.tracks->size() == 0) {
            return true;
        }
        const int target = (ctx.storage.tracks->size() > 1) ? 1 : 0;
        ctx.select_track_index(target);
        pump_frame();
        const int expected_active = ctx.track_index_to_list(ctx.track_index);
        const int active = ctx.access.list_view_active(ctx.handles.list);
        const bool ok = ctx.track_index == target
            && ui_ci_now_playing_text_sync_ok(scene, ctx)
            && ui_ci_now_playing_cover_state_ok(scene, ctx)
            && active == expected_active;
        if (!ok) {
            std::printf("[ui-ci] now_playing_library_sync target=%d track_index=%d active=%d expected_active=%d "
                        "list_selected=%d list_count=%d\n",
                        target,
                        ctx.track_index,
                        active,
                        expected_active,
                        ctx.last_list_selected,
                        ctx.last_list_count);
        }
        return ok;
    }

    template<typename SceneT>
    bool ui_ci_rect_contains_x(const SceneT& scene, WidgetHandle outer, WidgetHandle inner) noexcept {
        const Rect o = scene.world_rect(outer);
        const Rect i = scene.world_rect(inner);
        return o.w > 0 && o.h > 0 && i.w > 0 && i.h > 0
            && o.x <= i.x
            && (o.x + o.w) >= (i.x + i.w);
    }

    // ProgressSection seek cases validate the Player contract while the UI is
    // still implemented as Slider + transparent hitbox rather than a Vivid widget.
    template<typename SceneT, typename ContextT>
    bool ui_ci_now_playing_seek_rect_hitbox_ok(const SceneT& scene, const ContextT& ctx) noexcept {
        const Rect visual = scene.world_rect(ctx.handles.progress_visual);
        const Rect hit = scene.world_rect(ctx.handles.progress);
        const Rect group = scene.world_rect(ctx.handles.now_progress_group);
        const bool ok = visual.w >= 240
            && visual.h >= 12
            && hit.w >= visual.w
            && hit.h >= 36
            && group.w >= hit.w
            && ui_ci_rect_contains_x(scene, ctx.handles.progress, ctx.handles.progress_visual);
        if (!ok) {
            std::printf("[ui-ci] seek_rect visual=%d,%d,%d,%d hit=%d,%d,%d,%d group=%d,%d,%d,%d\n",
                        visual.x, visual.y, visual.w, visual.h,
                        hit.x, hit.y, hit.w, hit.h,
                        group.x, group.y, group.w, group.h);
        }
        return ok;
    }

    template<typename ContextT>
    struct UiCiSeekStateGuard {
        ContextT& ctx;
        bool progress_dragging{};
        int progress_drag_value{};
        int progress_drag_sec{};
        int progress_committed_sec{};
        std::uint8_t progress_committed_hold_frames{};
        player::ProgressInteractionState progress_state{};
        bool track_preloaded{};
        int last_time_sec{};
        int last_time_total_sec{};
        int current_sec{};
        int duration_sec{};
        const char* track_path{};
        bool track_ready{};
        std::string last_info_text{};

        explicit UiCiSeekStateGuard(ContextT& in) noexcept
            : ctx(in),
              progress_dragging(in.progress_dragging),
              progress_drag_value(in.progress_drag_value),
              progress_drag_sec(in.progress_drag_sec),
              progress_committed_sec(in.progress_committed_sec),
              progress_committed_hold_frames(in.progress_committed_hold_frames),
              progress_state(in.progress_interaction_state),
              track_preloaded(in.track_preloaded),
              last_time_sec(in.last_time_sec),
              last_time_total_sec(in.last_time_total_sec),
              current_sec(in.playback.current_sec()),
              duration_sec(in.playback.duration_sec()),
              track_path(in.track_path()),
              track_ready(in.track_ready()),
              last_info_text(in.last_info_text.view()) {}

        void restore() {
            ctx.progress_dragging = progress_dragging;
            ctx.progress_drag_value = progress_drag_value;
            ctx.progress_drag_sec = progress_drag_sec;
            ctx.progress_committed_sec = progress_committed_sec;
            ctx.progress_committed_hold_frames = progress_committed_hold_frames;
            ctx.progress_interaction_state = progress_state;
            ctx.track_preloaded = track_preloaded;
            ctx.playback.set_track_path(track_path);
            ctx.playback.set_track_ready(track_ready);
            ctx.playback.set_duration_from_probe_preserving_current(duration_sec);
            ctx.playback.set_current_sec(current_sec);
            ctx.last_time_sec = -1;
            ctx.last_time_total_sec = -1;
            ctx.last_info_text.clear();
            ctx.set_time_label(last_time_sec < 0 ? current_sec : last_time_sec);
            ctx.set_info_label(last_info_text);
            ctx.sync_progress_idle(current_sec);
        }
    };

    template<typename ContextT>
    void ui_ci_prepare_seek_duration(ContextT& ctx, int duration_sec, int current_sec) {
        ctx.track_preloaded = true;
        if (!ctx.track_path()) {
            ctx.playback.set_track_path("/ui-ci/seek-fixture.wav");
        }
        ctx.playback.set_track_ready(true);
        ctx.playback.set_duration_from_probe(duration_sec);
        ctx.playback.set_current_sec(current_sec);
        ctx.last_time_sec = -1;
        ctx.last_time_total_sec = -1;
        ctx.last_info_text.clear();
        ctx.sync_progress_idle(current_sec);
    }

    template<typename SceneT, typename ContextT, typename PumpFrameFn>
    bool ui_ci_now_playing_closure_fallback_no_duration_ok(const SceneT& scene,
                                                           ContextT& ctx,
                                                           PumpFrameFn& pump_frame) {
        const Rect stage_before = ui_ci_handle_rect(scene, ctx.handles.now_cover_plate);
        const Rect cover_before = ui_ci_handle_rect(scene, ctx.handles.cover);
        const Rect text_before = ui_ci_handle_rect(scene, ctx.handles.now_text_group);
        const Rect progress_before = ui_ci_handle_rect(scene, ctx.handles.now_progress_group);
        const Rect controls_before = ui_ci_handle_rect(scene, ctx.handles.controls);
        const Rect aux_before = ui_ci_handle_rect(scene, ctx.handles.now_aux_group);
        const auto saved_cover = ctx.current_cover;
        const auto saved_ready = ctx.cover_ready;
        const auto saved_cover_path = ctx.cover_path;
        const auto saved_embedded = ctx.cover_embedded_path;
        const auto saved_folder = ctx.cover_folder_path;
        const auto saved_failed_embedded = ctx.cover_failed_embedded_path;
        const auto saved_failed_folder = ctx.cover_failed_folder_path;
        const auto saved_tint_path = ctx.cover_tint_path;
        const auto saved_cover_theme = ctx.cover_theme;
        const auto saved_roles = ctx.now_playing_roles;
        UiCiSeekStateGuard seek_guard{ctx};

        ctx.show_default_current_cover();
        ctx.apply_cover_theme_from_current_image();
        ctx.track_preloaded = false;
        ctx.reset_duration();
        ctx.restore_now_playing_group_visibility();
        ctx.restore_bottom_bar_content_visibility();
        pump_frame();

        const Rect progress_after = ui_ci_handle_rect(scene, ctx.handles.now_progress_group);
        const bool ok = !ctx.current_cover.valid()
            && ctx.progress_interaction_state == player::ProgressInteractionState::Unavailable
            && ctx.access.value(ctx.handles.progress_visual) == 0
            && std::string_view{ui_ci_text_value(scene, ctx.handles.time_left)} == "0:00"
            && std::string_view{ui_ci_text_value(scene, ctx.handles.time_right)} == "0:00"
            && ui_ci_rect_equal(stage_before, ui_ci_handle_rect(scene, ctx.handles.now_cover_plate), 0)
            && ui_ci_rect_equal(cover_before, ui_ci_handle_rect(scene, ctx.handles.cover), 0)
            && ui_ci_rect_equal(text_before, ui_ci_handle_rect(scene, ctx.handles.now_text_group), 0)
            && ui_ci_rect_equal(progress_before, progress_after, 0)
            && ui_ci_rect_equal(controls_before, ui_ci_handle_rect(scene, ctx.handles.controls), 0)
            && ui_ci_rect_equal(aux_before, ui_ci_handle_rect(scene, ctx.handles.now_aux_group), 0);
        if (!ok) {
            std::printf("[ui-ci] closure_fallback_no_duration state=%u value=%d left=%s right=%s "
                        "cover_valid=%d before_progress=%d,%d,%d,%d after_progress=%d,%d,%d,%d\n",
                        static_cast<unsigned>(ctx.progress_interaction_state),
                        ctx.access.value(ctx.handles.progress_visual),
                        ui_ci_text_value(scene, ctx.handles.time_left),
                        ui_ci_text_value(scene, ctx.handles.time_right),
                        ctx.current_cover.valid() ? 1 : 0,
                        progress_before.x, progress_before.y, progress_before.w, progress_before.h,
                        progress_after.x, progress_after.y, progress_after.w, progress_after.h);
        }

        seek_guard.restore();
        ctx.current_cover = saved_cover;
        ctx.cover_ready = saved_ready;
        ctx.cover_path = saved_cover_path;
        ctx.cover_embedded_path = saved_embedded;
        ctx.cover_folder_path = saved_folder;
        ctx.cover_failed_embedded_path = saved_failed_embedded;
        ctx.cover_failed_folder_path = saved_failed_folder;
        ctx.cover_tint_path = saved_tint_path;
        ctx.cover_theme = saved_cover_theme;
        ctx.now_playing_roles = saved_roles;
        if (ctx.current_cover.valid()) {
            ctx.access.set_image(ctx.handles.cover, ctx.current_cover.image_id);
            ctx.access.set_image(ctx.handles.bottom_cover, ctx.current_cover.image_id);
        } else {
            ctx.show_default_current_cover();
        }
        ctx.refresh_cover_theme_dependent_visuals();
        ctx.restore_now_playing_group_visibility();
        ctx.restore_bottom_bar_content_visibility();
        pump_frame();
        return ok;
    }

    template<typename RuntimeT, typename SceneT, typename ContextT, typename PumpFrameFn>
    bool ui_ci_now_playing_seek_drag_preview_ok(RuntimeT& runtime,
                                                const SceneT& scene,
                                                ContextT& ctx,
                                                PumpFrameFn& pump_frame) {
        UiCiSeekStateGuard guard{ctx};
        ui_ci_prepare_seek_duration(ctx, 240, 30);
        pump_frame();
        const Rect r = scene.world_rect(ctx.handles.progress);
        if (r.w <= 0 || r.h <= 0) {
            guard.restore();
            pump_frame();
            return false;
        }
        ui_ci_pointer(runtime, 10, player::PlayerPointerAction::Down, true, r.x + r.w / 4, r.y + r.h / 2);
        pump_frame();
        ui_ci_pointer(runtime, 11, player::PlayerPointerAction::Move, true, r.x + (r.w * 3) / 4, r.y + r.h / 2);
        pump_frame();
        const int value = ctx.access.value(ctx.handles.progress_visual);
        const bool ok = ctx.progress_dragging
            && ctx.progress_interaction_state == player::ProgressInteractionState::DraggingPreview
            && value >= 70
            && value <= 80
            && std::string_view{ui_ci_text_value(scene, ctx.handles.info_tag)} == "Seeking..."
            && std::string_view{ui_ci_text_value(scene, ctx.handles.time_left)} == "3:00"
            && std::string_view{ui_ci_text_value(scene, ctx.handles.time_right)} == "4:00";
        if (!ok) {
            std::printf("[ui-ci] seek_drag_preview dragging=%d state=%u value=%d left=%s right=%s info=%s\n",
                        ctx.progress_dragging ? 1 : 0,
                        static_cast<unsigned>(ctx.progress_interaction_state),
                        value,
                        ui_ci_text_value(scene, ctx.handles.time_left),
                        ui_ci_text_value(scene, ctx.handles.time_right),
                        ui_ci_text_value(scene, ctx.handles.info_tag));
        }
        ui_ci_pointer(runtime, 12, player::PlayerPointerAction::Up, false, r.x + (r.w * 3) / 4, r.y + r.h / 2);
        guard.restore();
        pump_frame();
        return ok;
    }

    template<typename RuntimeT, typename SceneT, typename ContextT, typename PumpFrameFn>
    bool ui_ci_now_playing_seek_commit_ok(RuntimeT& runtime,
                                          const SceneT& scene,
                                          ContextT& ctx,
                                          PumpFrameFn& pump_frame) {
        UiCiSeekStateGuard guard{ctx};
        ui_ci_prepare_seek_duration(ctx, 240, 20);
        pump_frame();
        const Rect r = scene.world_rect(ctx.handles.progress);
        ui_ci_pointer(runtime, 20, player::PlayerPointerAction::Down, true, r.x + r.w / 8, r.y + r.h / 2);
        pump_frame();
        ui_ci_pointer(runtime, 21, player::PlayerPointerAction::Move, true, r.x + r.w / 2, r.y + r.h / 2);
        pump_frame();
        ui_ci_pointer(runtime, 22, player::PlayerPointerAction::Up, false, r.x + r.w / 2, r.y + r.h / 2);
        pump_frame();
        const int value = ctx.access.value(ctx.handles.progress_visual);
        const bool ok = !ctx.progress_dragging
            && ctx.progress_interaction_state == player::ProgressInteractionState::SeekCommitted
            && value >= 48
            && value <= 52
            && std::string_view{ui_ci_text_value(scene, ctx.handles.info_tag)} == "Seek queued";
        if (!ok) {
            std::printf("[ui-ci] seek_commit dragging=%d state=%u value=%d info=%s\n",
                        ctx.progress_dragging ? 1 : 0,
                        static_cast<unsigned>(ctx.progress_interaction_state),
                        value,
                        ui_ci_text_value(scene, ctx.handles.info_tag));
        }
        guard.restore();
        pump_frame();
        return ok;
    }

    template<typename SceneT, typename ContextT, typename PumpFrameFn>
    bool ui_ci_now_playing_seek_cancel_restore_ok(const SceneT& scene,
                                                  ContextT& ctx,
                                                  PumpFrameFn& pump_frame) {
        UiCiSeekStateGuard guard{ctx};
        ui_ci_prepare_seek_duration(ctx, 240, 45);
        pump_frame();
        const Rect r = scene.world_rect(ctx.handles.progress);
        player::PlayerController::PendingActions actions{};
        (void)ctx.handle_now_playing_progress_input(
            ctx.handles.progress, Event::Type::MouseDown, r.x + r.w / 5, actions);
        (void)ctx.handle_now_playing_progress_input(
            ctx.handles.progress, Event::Type::DragMove, r.x + (r.w * 4) / 5, actions);
        (void)ctx.handle_now_playing_progress_input(
            ctx.handles.progress, Event::Type::Cancel, r.x + (r.w * 4) / 5, actions);
        pump_frame();
        const int value = ctx.access.value(ctx.handles.progress_visual);
        const bool ok = !ctx.progress_dragging
            && !actions.seek
            && ctx.progress_interaction_state == player::ProgressInteractionState::Idle
            && value >= 18
            && value <= 19
            && std::string_view{ui_ci_text_value(scene, ctx.handles.time_left)} == "0:45";
        if (!ok) {
            std::printf("[ui-ci] seek_cancel dragging=%d state=%u seek=%d value=%d left=%s\n",
                        ctx.progress_dragging ? 1 : 0,
                        static_cast<unsigned>(ctx.progress_interaction_state),
                        actions.seek ? 1 : 0,
                        value,
                        ui_ci_text_value(scene, ctx.handles.time_left));
        }
        guard.restore();
        pump_frame();
        return ok;
    }

    template<typename SceneT, typename ContextT, typename PumpFrameFn>
    bool ui_ci_now_playing_seek_no_duration_fallback_ok(const SceneT& scene,
                                                        ContextT& ctx,
                                                        PumpFrameFn& pump_frame) {
        UiCiSeekStateGuard guard{ctx};
        ctx.track_preloaded = false;
        ctx.reset_duration();
        pump_frame();
        const bool ok = ctx.progress_interaction_state == player::ProgressInteractionState::Unavailable
            && ctx.access.value(ctx.handles.progress_visual) == 0
            && std::string_view{ui_ci_text_value(scene, ctx.handles.time_left)} == "0:00"
            && std::string_view{ui_ci_text_value(scene, ctx.handles.time_right)} == "0:00"
            && std::string_view{ui_ci_text_value(scene, ctx.handles.info_tag)} == "";
        if (!ok) {
            std::printf("[ui-ci] seek_no_duration state=%u value=%d left=%s right=%s info=%s\n",
                        static_cast<unsigned>(ctx.progress_interaction_state),
                        ctx.access.value(ctx.handles.progress_visual),
                        ui_ci_text_value(scene, ctx.handles.time_left),
                        ui_ci_text_value(scene, ctx.handles.time_right),
                        ui_ci_text_value(scene, ctx.handles.info_tag));
        }
        guard.restore();
        pump_frame();
        return ok;
    }

    template<typename SceneT, typename ContextT, typename PumpFrameFn>
    bool ui_ci_now_playing_seek_time_label_sync_ok(const SceneT& scene,
                                                   ContextT& ctx,
                                                   PumpFrameFn& pump_frame) {
        UiCiSeekStateGuard guard{ctx};
        ui_ci_prepare_seek_duration(ctx, 240, 90);
        pump_frame();
        const bool initial_ok = std::string_view{ui_ci_text_value(scene, ctx.handles.time_left)} == "1:30"
            && std::string_view{ui_ci_text_value(scene, ctx.handles.time_right)} == "4:00"
            && ctx.access.value(ctx.handles.progress_visual) >= 37
            && ctx.access.value(ctx.handles.progress_visual) <= 38;
        ctx.playback.set_duration_from_probe_preserving_current(300);
        ctx.last_time_total_sec = -1;
        ctx.sync_progress_idle(90);
        pump_frame();
        const bool updated_ok = std::string_view{ui_ci_text_value(scene, ctx.handles.time_left)} == "1:30"
            && std::string_view{ui_ci_text_value(scene, ctx.handles.time_right)} == "5:00"
            && ctx.access.value(ctx.handles.progress_visual) == 30;
        if (!initial_ok || !updated_ok) {
            std::printf("[ui-ci] seek_time_sync initial=%d updated=%d value=%d left=%s right=%s\n",
                        initial_ok ? 1 : 0,
                        updated_ok ? 1 : 0,
                        ctx.access.value(ctx.handles.progress_visual),
                        ui_ci_text_value(scene, ctx.handles.time_left),
                        ui_ci_text_value(scene, ctx.handles.time_right));
        }
        guard.restore();
        pump_frame();
        return initial_ok && updated_ok;
    }

    bool ui_ci_now_playing_seek_ready_state_contract_ok() noexcept {
        const bool ok = !player::player_state_allows_seek(audio::PlayerState::idle)
            && !player::player_state_allows_seek(audio::PlayerState::opening)
            && player::player_state_allows_seek(audio::PlayerState::buffering)
            && player::player_state_allows_seek(audio::PlayerState::playing)
            && player::player_state_allows_seek(audio::PlayerState::paused)
            && !player::player_state_allows_seek(audio::PlayerState::stopping)
            && !player::player_state_allows_seek(audio::PlayerState::error);
        if (!ok) {
            std::printf("[ui-ci] seek_ready_state_contract paused=%d playing=%d buffering=%d idle=%d\n",
                        player::player_state_allows_seek(audio::PlayerState::paused) ? 1 : 0,
                        player::player_state_allows_seek(audio::PlayerState::playing) ? 1 : 0,
                        player::player_state_allows_seek(audio::PlayerState::buffering) ? 1 : 0,
                        player::player_state_allows_seek(audio::PlayerState::idle) ? 1 : 0);
        }
        return ok;
    }

    bool ui_ci_write_vfs_file(std::string_view path, std::span<const util::u8> bytes) {
        fs::File file{};
        auto st = fs::vfs_open(path, file,
                               static_cast<fs::OpenFlags>(
                                   static_cast<util::u32>(fs::OpenFlags::write)
                                   | static_cast<util::u32>(fs::OpenFlags::create)
                                   | static_cast<util::u32>(fs::OpenFlags::trunc)));
        if (!st) {
            std::printf("[ui-ci] lyrics_write_open path=%.*s err=%d\n",
                        static_cast<int>(path.size()), path.data(), static_cast<int>(st.err));
            return false;
        }
        st = fs::vfs_write(file, bytes);
        (void)fs::vfs_close(file);
        if (!st) {
            std::printf("[ui-ci] lyrics_write path=%.*s err=%d\n",
                        static_cast<int>(path.size()), path.data(), static_cast<int>(st.err));
            return false;
        }
        return true;
    }

#if CHARM_PLAYER_LYRICS
    bool ui_ci_write_vfs_text(std::string_view path, std::string_view text) {
        return ui_ci_write_vfs_file(path, std::span<const util::u8>{
            reinterpret_cast<const util::u8*>(text.data()),
            text.size(),
        });
    }

    std::string ui_ci_track_sidecar_path(std::string_view track_path, std::string_view ext) {
        std::string out{track_path};
        const auto slash = out.find_last_of("/\\");
        const auto dot = out.find_last_of('.');
        if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) {
            out.resize(dot);
        }
        out.append(ext);
        return out;
    }

    std::string ui_ci_track_lyrics_dir_path(std::string_view track_path, std::string_view ext) {
        std::string stem{track_path};
        const auto slash = stem.find_last_of("/\\");
        if (slash != std::string::npos) {
            stem.erase(0, slash + 1);
        }
        const auto dot = stem.find_last_of('.');
        if (dot != std::string::npos) {
            stem.resize(dot);
        }
        std::string out{"/Lyrics/"};
        out.append(stem);
        out.append(ext);
        return out;
    }

    std::string ui_ci_track_dir_temp_path(std::string_view track_path, std::string_view name) {
        std::string out{track_path};
        const auto slash = out.find_last_of("/\\");
        if (slash == std::string::npos) {
            out.assign("/");
        } else {
            out.resize(slash + 1);
        }
        out.append(name);
        return out;
    }

    bool ui_ci_prepare_lyrics_dir() {
        auto st = fs::vfs_mkdir("/Lyrics");
        return st || st.err == fs::Errc::exist;
    }

    template<typename SceneT, typename ContextT, typename PumpFrameFn>
    bool ui_ci_now_playing_lyrics_toggle_fallback_ok(const SceneT& scene,
                                                     ContextT& ctx,
                                                     PumpFrameFn& pump_frame) {
        const bool saved_visible = ctx.lyrics_visible;
        ctx.lyrics_visible = false;
        ctx.sync_lyrics_visibility();
        ctx.clear_current_lyrics();
        pump_frame();
        ctx.toggle_lyrics_panel();
        pump_frame();
        const bool ok = ctx.lyrics_visible
            && std::string_view{ui_ci_text_value(scene, ctx.handles.now_lyrics_current)} == "No lyrics";
        if (!ok) {
            std::printf("[ui-ci] lyrics_toggle visible=%d current=%s\n",
                        ctx.lyrics_visible ? 1 : 0,
                        ui_ci_text_value(scene, ctx.handles.now_lyrics_current));
        }
        if (ctx.lyrics_visible != saved_visible) {
            ctx.toggle_lyrics_panel();
        }
        ctx.lyrics_visible = saved_visible;
        ctx.sync_lyrics_visibility();
        pump_frame();
        return ok;
    }

    template<typename SceneT, typename ContextT, typename PumpFrameFn>
    bool ui_ci_now_playing_sidecar_lrc_ok(const SceneT& scene,
                                          ContextT& ctx,
                                          PumpFrameFn& pump_frame) {
        if (!ctx.storage.tracks || ctx.storage.tracks->empty()) return true;
        const std::string track{(*ctx.storage.tracks)[ctx.track_index].view()};
        const std::string sidecar = ui_ci_track_sidecar_path(track, ".lrc");
        constexpr std::string_view kLrc =
            "[00:00.00]Intro line\n"
            "[00:10.50]Current line\n"
            "[00:20.000]Next line\n";
        if (!ui_ci_write_vfs_text(sidecar, kLrc)) return false;
        ctx.load_current_track_lyrics(track);
        ctx.lyrics_visible = true;
        ctx.sync_lyrics_visibility();
        ctx.track_preloaded = true;
        ctx.playback.set_duration_from_probe(60);
        ctx.playback.set_current_sec(11);
        ctx.refresh_lyrics_window(true);
        pump_frame();
        const bool ok = ctx.lyrics_result.source == player::LyricsSourceKind::Sidecar
            && ctx.lyrics_result.synced
            && std::string_view{ui_ci_text_value(scene, ctx.handles.now_lyrics_prev)} == "Intro line"
            && std::string_view{ui_ci_text_value(scene, ctx.handles.now_lyrics_current)} == "Current line"
            && std::string_view{ui_ci_text_value(scene, ctx.handles.now_lyrics_next)} == "Next line";
        (void)fs::vfs_unlink(sidecar);
        ctx.lyrics_visible = false;
        ctx.sync_lyrics_visibility();
        pump_frame();
        return ok;
    }

    template<typename SceneT, typename ContextT, typename PumpFrameFn>
    bool ui_ci_now_playing_lyrics_dir_lrc_ok(const SceneT& scene,
                                             ContextT& ctx,
                                             PumpFrameFn& pump_frame) {
        if (!ctx.storage.tracks || ctx.storage.tracks->empty()) return true;
        if (!ui_ci_prepare_lyrics_dir()) return false;
        const std::string track{(*ctx.storage.tracks)[ctx.track_index].view()};
        const std::string sidecar = ui_ci_track_sidecar_path(track, ".lrc");
        const std::string lyrics_path = ui_ci_track_lyrics_dir_path(track, ".lrc");
        (void)fs::vfs_unlink(sidecar);
        constexpr std::string_view kLrc =
            "[00:00.00]Lyrics dir intro\n"
            "[00:03.00]Lyrics dir current\n";
        if (!ui_ci_write_vfs_text(lyrics_path, kLrc)) return false;
        ctx.load_current_track_lyrics(track);
        ctx.playback.set_current_sec(4);
        ctx.refresh_lyrics_window(true);
        pump_frame();
        const bool ok = ctx.lyrics_result.source == player::LyricsSourceKind::LyricsDir
            && std::string_view{ui_ci_text_value(scene, ctx.handles.now_lyrics_current)}
                == "Lyrics dir current";
        (void)fs::vfs_unlink(lyrics_path);
        return ok;
    }

    template<typename SceneT, typename ContextT, typename PumpFrameFn>
    bool ui_ci_now_playing_txt_unsynced_ok(const SceneT& scene,
                                           ContextT& ctx,
                                           PumpFrameFn& pump_frame) {
        if (!ctx.storage.tracks || ctx.storage.tracks->empty()) return true;
        const std::string track{(*ctx.storage.tracks)[ctx.track_index].view()};
        const std::string sidecar = ui_ci_track_sidecar_path(track, ".txt");
        constexpr std::string_view kTxt =
            "First plain line\n"
            "Second plain line\n";
        if (!ui_ci_write_vfs_text(sidecar, kTxt)) return false;
        ctx.load_current_track_lyrics(track);
        ctx.playback.set_current_sec(20);
        ctx.refresh_lyrics_window(true);
        pump_frame();
        const bool ok = ctx.lyrics_result.source == player::LyricsSourceKind::Sidecar
            && !ctx.lyrics_result.synced
            && std::string_view{ui_ci_text_value(scene, ctx.handles.now_lyrics_current)}
                == "First plain line"
            && std::string_view{ui_ci_text_value(scene, ctx.handles.now_lyrics_next)}
                == "Second plain line";
        (void)fs::vfs_unlink(sidecar);
        return ok;
    }

    template<typename SceneT, typename ContextT, typename PumpFrameFn>
    bool ui_ci_now_playing_lyrics_seek_pause_sync_ok(const SceneT& scene,
                                                     ContextT& ctx,
                                                     PumpFrameFn& pump_frame) {
        if (!ctx.storage.tracks || ctx.storage.tracks->empty()) return true;
        const std::string track{(*ctx.storage.tracks)[ctx.track_index].view()};
        const std::string sidecar = ui_ci_track_sidecar_path(track, ".lrc");
        constexpr std::string_view kLrc =
            "[00:00.00]Start lyric\n"
            "[00:30.00]Seek lyric\n"
            "[00:45.00]Paused lyric\n";
        if (!ui_ci_write_vfs_text(sidecar, kLrc)) return false;
        ctx.load_current_track_lyrics(track);
        ctx.track_preloaded = true;
        ctx.playback.set_duration_from_probe(90);
        ctx.playback.set_current_sec(30);
        ctx.sync_progress_idle(30);
        pump_frame();
        const bool seek_ok =
            std::string_view{ui_ci_text_value(scene, ctx.handles.now_lyrics_current)} == "Seek lyric";
        ctx.pause_playback();
        ctx.playback.set_current_sec(46);
        ctx.sync_progress_idle(46);
        pump_frame();
        const bool pause_ok =
            std::string_view{ui_ci_text_value(scene, ctx.handles.now_lyrics_current)} == "Paused lyric";
        (void)fs::vfs_unlink(sidecar);
        return seek_ok && pause_ok;
    }

    std::array<util::u8, 96> ui_ci_make_flac_lyrics_file(std::string_view lyric, std::size_t& out_size) {
        std::array<util::u8, 96> bytes{};
        std::size_t pos = 0;
        auto put = [&](util::u8 v) { bytes[pos++] = v; };
        auto put_le32 = [&](std::uint32_t v) {
            put(static_cast<util::u8>(v & 0xFFu));
            put(static_cast<util::u8>((v >> 8) & 0xFFu));
            put(static_cast<util::u8>((v >> 16) & 0xFFu));
            put(static_cast<util::u8>((v >> 24) & 0xFFu));
        };
        constexpr std::string_view vendor{"ci"};
        constexpr std::string_view key{"LYRICS="};
        const std::uint32_t block_size =
            4 + static_cast<std::uint32_t>(vendor.size())
            + 4 + 4 + static_cast<std::uint32_t>(key.size() + lyric.size());
        put('f'); put('L'); put('a'); put('C');
        put(0x80u | 4u);
        put(static_cast<util::u8>((block_size >> 16) & 0xFFu));
        put(static_cast<util::u8>((block_size >> 8) & 0xFFu));
        put(static_cast<util::u8>(block_size & 0xFFu));
        put_le32(static_cast<std::uint32_t>(vendor.size()));
        for (char c : vendor) put(static_cast<util::u8>(c));
        put_le32(1);
        put_le32(static_cast<std::uint32_t>(key.size() + lyric.size()));
        for (char c : key) put(static_cast<util::u8>(c));
        for (char c : lyric) put(static_cast<util::u8>(c));
        out_size = pos;
        return bytes;
    }

    std::array<util::u8, 128> ui_ci_make_mp3_uslt_file(std::string_view lyric, std::size_t& out_size) {
        std::array<util::u8, 128> bytes{};
        std::size_t pos = 0;
        auto put = [&](util::u8 v) { bytes[pos++] = v; };
        auto put_be32 = [&](std::uint32_t v) {
            put(static_cast<util::u8>((v >> 24) & 0xFFu));
            put(static_cast<util::u8>((v >> 16) & 0xFFu));
            put(static_cast<util::u8>((v >> 8) & 0xFFu));
            put(static_cast<util::u8>(v & 0xFFu));
        };
        constexpr std::string_view desc{"ci"};
        const std::uint32_t frame_size =
            1 + 3 + static_cast<std::uint32_t>(desc.size()) + 1
            + static_cast<std::uint32_t>(lyric.size());
        const std::uint32_t tag_size = 10 + frame_size;
        put('I'); put('D'); put('3'); put(3); put(0); put(0);
        put(static_cast<util::u8>((tag_size >> 21) & 0x7Fu));
        put(static_cast<util::u8>((tag_size >> 14) & 0x7Fu));
        put(static_cast<util::u8>((tag_size >> 7) & 0x7Fu));
        put(static_cast<util::u8>(tag_size & 0x7Fu));
        put('U'); put('S'); put('L'); put('T');
        put_be32(frame_size);
        put(0); put(0);
        put(3);
        put('e'); put('n'); put('g');
        for (char c : desc) put(static_cast<util::u8>(c));
        put(0);
        for (char c : lyric) put(static_cast<util::u8>(c));
        out_size = pos;
        return bytes;
    }

    template<typename ContextT>
    bool ui_ci_lyrics_embedded_probe_ok(ContextT& ctx) {
        if (!ctx.storage.tracks || ctx.storage.tracks->empty()) return true;
        const std::string track{(*ctx.storage.tracks)[ctx.track_index].view()};
        const std::string flac_path = ui_ci_track_dir_temp_path(track, "ui-ci-lyrics.flac");
        const std::string mp3_path = ui_ci_track_dir_temp_path(track, "ui-ci-lyrics.mp3");

        std::size_t flac_size = 0;
        auto flac = ui_ci_make_flac_lyrics_file("[00:00.00]FLAC lyric\n", flac_size);
        if (!ui_ci_write_vfs_file(flac_path,
                                  std::span<const util::u8>{flac.data(), flac_size})) {
            return false;
        }
        const auto flac_result = player::load_lyrics_for_track(flac_path);
        const auto flac_window = player::resolve_lyrics_window(0);
        (void)fs::vfs_unlink(flac_path);

        std::size_t mp3_size = 0;
        auto mp3 = ui_ci_make_mp3_uslt_file("[00:00.00]MP3 lyric\n", mp3_size);
        if (!ui_ci_write_vfs_file(mp3_path,
                                  std::span<const util::u8>{mp3.data(), mp3_size})) {
            return false;
        }
        const auto mp3_result = player::load_lyrics_for_track(mp3_path);
        const auto mp3_window = player::resolve_lyrics_window(0);
        (void)fs::vfs_unlink(mp3_path);
        return flac_result.source == player::LyricsSourceKind::EmbeddedFlac
            && std::string_view{flac_window.current} == "FLAC lyric"
            && mp3_result.source == player::LyricsSourceKind::EmbeddedMp3
            && std::string_view{mp3_window.current} == "MP3 lyric";
    }
#endif

    template<typename RuntimeT,
             typename SceneT,
             typename ContextT,
             typename WaitForPageFn,
             typename SettleNowPlayingTransitionFn,
             typename PumpFrameFn>
    void run_now_playing_state_matrix_regressions(UiCiResult& res,
                                                  RuntimeT& runtime,
                                                  SceneT& scene,
                                                  ContextT& ctx,
                                                  WaitForPageFn& wait_for_page,
                                                  SettleNowPlayingTransitionFn& settle_now_playing_transition,
                                                  PumpFrameFn& pump_frame) {
        (void)runtime;

        auto record = [&](const char* name, bool ok, const char* reason) {
            ui_ci_emit(name, ok, reason);
            if (!ok) {
                res.ok = false;
                res.failed++;
            }
        };

        ctx.set_page(player::PlayerPage::NowPlaying);
        pump_frame();
        if (!wait_for_page(player::PlayerPage::NowPlaying)
            && !settle_now_playing_transition(player::PlayerPage::NowPlaying)) {
            record("now_playing_matrix_enter", false, "page_not_now");
            return;
        }
        record("now_playing_matrix_enter", true, nullptr);

        // Structure/state matrix: page wiring, text/cover sync, and play state.
        const char* structure_failure = ui_ci_now_playing_structure_failure(scene, ctx);
        record("now_playing_matrix_structure", structure_failure == nullptr, structure_failure);
        record("now_playing_matrix_text_sync",
               ui_ci_now_playing_text_sync_ok(scene, ctx),
               "text_sync");
        record("now_playing_matrix_cover_state",
               ui_ci_now_playing_cover_state_ok(scene, ctx),
               "cover_state");
        record("now_playing_matrix_play_pause",
               ui_ci_now_playing_play_state_ok(scene, ctx),
               "play_pause");
        record("now_playing_matrix_fallback_cover",
               ui_ci_now_playing_fallback_cover_ok(scene, ctx, pump_frame),
               "fallback_cover");
        record("now_playing_matrix_long_text",
               ui_ci_now_playing_long_text_ok(scene, ctx, pump_frame),
               "long_text");
        record("now_playing_matrix_library_sync",
               ui_ci_now_playing_library_sync_ok(scene, ctx, pump_frame),
               "library_sync");

        // Main-experience closure: layout stack, cover stage, title block, controls, fallback.
        const char* closure_layout_failure = ui_ci_now_playing_closure_layout_stack_failure(scene, ctx);
        record("now_playing_closure_layout_stack",
               closure_layout_failure == nullptr,
               closure_layout_failure);
        const char* closure_cover_failure = ui_ci_now_playing_closure_cover_stage_failure(scene, ctx);
        record("now_playing_closure_cover_stage",
               closure_cover_failure == nullptr,
               closure_cover_failure);
        const char* closure_title_failure = ui_ci_now_playing_closure_title_block_failure(scene, ctx);
        record("now_playing_closure_title_block",
               closure_title_failure == nullptr,
               closure_title_failure);
        const char* closure_controls_failure = ui_ci_now_playing_closure_control_hierarchy_failure(scene, ctx);
        record("now_playing_closure_control_hierarchy",
               closure_controls_failure == nullptr,
               closure_controls_failure);
        record("now_playing_closure_fallback_no_duration",
               ui_ci_now_playing_closure_fallback_no_duration_ok(scene, ctx, pump_frame),
               "fallback_no_duration");

        // ProgressSection seek contract: hitbox, drag preview, commit/cancel, no-duration fallback.
        record("now_playing_seek_rect_hitbox",
               ui_ci_now_playing_seek_rect_hitbox_ok(scene, ctx),
               "seek_rect_hitbox");
        record("now_playing_seek_drag_preview",
               ui_ci_now_playing_seek_drag_preview_ok(runtime, scene, ctx, pump_frame),
               "seek_drag_preview");
        record("now_playing_seek_commit",
               ui_ci_now_playing_seek_commit_ok(runtime, scene, ctx, pump_frame),
               "seek_commit");
        record("now_playing_seek_cancel_restore",
               ui_ci_now_playing_seek_cancel_restore_ok(scene, ctx, pump_frame),
               "seek_cancel_restore");
        record("now_playing_seek_no_duration_fallback",
               ui_ci_now_playing_seek_no_duration_fallback_ok(scene, ctx, pump_frame),
               "seek_no_duration_fallback");
        record("now_playing_seek_time_label_sync",
               ui_ci_now_playing_seek_time_label_sync_ok(scene, ctx, pump_frame),
               "seek_time_label_sync");
        record("now_playing_seek_ready_state_contract",
               ui_ci_now_playing_seek_ready_state_contract_ok(),
               "seek_ready_state_contract");

#if CHARM_PLAYER_LYRICS
        // Lyrics: local sidecar/Lyrics dir/TXT display plus embedded metadata probe.
        record("now_playing_lyrics_toggle_fallback",
               ui_ci_now_playing_lyrics_toggle_fallback_ok(scene, ctx, pump_frame),
               "lyrics_toggle_fallback");
        record("now_playing_lyrics_sidecar_lrc",
               ui_ci_now_playing_sidecar_lrc_ok(scene, ctx, pump_frame),
               "lyrics_sidecar_lrc");
        record("now_playing_lyrics_dir_lrc",
               ui_ci_now_playing_lyrics_dir_lrc_ok(scene, ctx, pump_frame),
               "lyrics_dir_lrc");
        record("now_playing_lyrics_txt",
               ui_ci_now_playing_txt_unsynced_ok(scene, ctx, pump_frame),
               "lyrics_txt");
        record("now_playing_lyrics_seek_pause_sync",
               ui_ci_now_playing_lyrics_seek_pause_sync_ok(scene, ctx, pump_frame),
               "lyrics_seek_pause_sync");
        record("now_playing_lyrics_embedded_probe",
               ui_ci_lyrics_embedded_probe_ok(ctx),
               "lyrics_embedded_probe");
#endif
    }
