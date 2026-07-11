    template<typename RuntimeT, typename SceneT, typename ContextT, typename PumpFrameFn>
    bool ui_ci_click_library_handle(RuntimeT& runtime,
                                    SceneT& scene,
                                    ContextT& ctx,
                                    PumpFrameFn& pump_frame,
                                    WidgetHandle handle) {
        if (!handle) return false;
        const Rect r = scene.world_rect(handle);
        if (r.w <= 0 || r.h <= 0) return false;
        ui_ci_click(runtime, r.x + r.w / 2, r.y + r.h / 2);
        pump_frame();
        (void)ctx;
        return true;
    }

    template<typename SceneT, typename ContextT>
    bool ui_ci_library_has_tracks(SceneT&, ContextT& ctx) {
        const auto* tracks = ctx.storage.tracks;
        return tracks && tracks->size() > 0;
    }

    template<typename SceneT>
    bool ui_ci_library_handle_has_rect(SceneT& scene, WidgetHandle handle) {
        if (!handle) return false;
        const Rect r = scene.world_rect(handle);
        return r.w > 0 && r.h > 0;
    }

    template<typename RuntimeT, typename SceneT, typename ContextT, typename PumpFrameFn>
    bool ui_ci_library_select_track_row(RuntimeT& runtime,
                                        SceneT& scene,
                                        ContextT& ctx,
                                        PumpFrameFn& pump_frame) {
        if (!ui_ci_library_has_tracks(scene, ctx)) return false;
        ctx.set_page(player::PlayerPage::Library);
        ctx.set_library_tab(player::LibraryTab::Songs);
        pump_frame();
        const Rect list = scene.world_rect(ctx.handles.list);
        if (list.w <= 0 || list.h <= 0) return false;
        ui_ci_click(runtime, list.x + 18, list.y + 18);
        pump_frame();
        return ctx.last_list_selected >= 0;
    }

    template<typename RuntimeT, typename SceneT, typename ContextT, typename PumpFrameFn>
    bool ui_ci_library_open_first_track_action(RuntimeT& runtime,
                                               SceneT& scene,
                                               ContextT& ctx,
                                               PumpFrameFn& pump_frame) {
        if (!ui_ci_library_has_tracks(scene, ctx)) return false;
        ctx.set_page(player::PlayerPage::Library);
        ctx.set_library_tab(player::LibraryTab::Songs);
        pump_frame();
        if (ctx.list_order.empty()) return false;

        const int row = 0;
        const Rect list = scene.world_rect(ctx.handles.list);
        if (list.w <= 0 || list.h <= 0) return false;
        const int row_y = list.y + row * ContextT::kLibraryListRowHeight - ctx.access.scroll_y(ctx.handles.list);
        ui_ci_click(runtime, list.x + list.w - 12, row_y + ContextT::kLibraryListRowHeight / 2);
        pump_frame();
        return ctx.list_action_menu_index == row
            && ctx.last_list_selected == row
            && ui_ci_library_handle_has_rect(scene, ctx.handles.list_action_card);
    }

    template<typename RuntimeT, typename SceneT, typename ContextT, typename PumpFrameFn>
    bool ui_ci_library_open_tail_action(RuntimeT& runtime,
                                        SceneT& scene,
                                        ContextT& ctx,
                                        PumpFrameFn& pump_frame) {
        if (!ui_ci_library_select_track_row(runtime, scene, ctx, pump_frame)) return false;
        const Rect list = scene.world_rect(ctx.handles.list);
        if (list.w <= 0 || list.h <= 0) return false;
        const int row = ctx.last_list_selected;
        const int row_y = list.y + row * ContextT::kLibraryListRowHeight - ctx.access.scroll_y(ctx.handles.list);
        ui_ci_click(runtime, list.x + list.w - 12, row_y + ContextT::kLibraryListRowHeight / 2);
        pump_frame();
        return ctx.list_action_menu_index == row
            && ui_ci_library_handle_has_rect(scene, ctx.handles.list_action_card);
    }

    template<typename RuntimeT, typename SceneT, typename ContextT, typename PumpFrameFn>
    bool ui_ci_library_tab_switch(RuntimeT& runtime,
                                  SceneT& scene,
                                  ContextT& ctx,
                                  PumpFrameFn& pump_frame) {
        ctx.set_page(player::PlayerPage::Library);
        ctx.set_library_tab(player::LibraryTab::Songs);
        pump_frame();
        if (!ui_ci_click_library_handle(runtime, scene, ctx, pump_frame, ctx.handles.list_tab_albums)) {
            return false;
        }
        const bool albums = ctx.library_tab == player::LibraryTab::Albums;
        if (!ui_ci_click_library_handle(runtime, scene, ctx, pump_frame, ctx.handles.list_tab_artist)) {
            return false;
        }
        const bool artists = ctx.library_tab == player::LibraryTab::Artists;
        if (!ui_ci_click_library_handle(runtime, scene, ctx, pump_frame, ctx.handles.list_tab_songs)) {
            return false;
        }
        return albums && artists && ctx.library_tab == player::LibraryTab::Songs;
    }

    template<typename RuntimeT, typename SceneT, typename ContextT, typename PumpFrameFn>
    bool ui_ci_library_sort_toggle(RuntimeT& runtime,
                                   SceneT& scene,
                                   ContextT& ctx,
                                   PumpFrameFn& pump_frame) {
        ctx.set_page(player::PlayerPage::Library);
        ctx.set_library_tab(player::LibraryTab::Songs);
        if (ctx.list_shuffle_enabled) {
            ctx.toggle_library_shuffle();
        }
        pump_frame();
        const auto before = ctx.list_sort;
        if (!ui_ci_click_library_handle(runtime, scene, ctx, pump_frame, ctx.handles.list_sort)) {
            return false;
        }
        const bool toggled = ctx.list_sort != before && !ctx.list_shuffle_enabled;
        if (!ui_ci_click_library_handle(runtime, scene, ctx, pump_frame, ctx.handles.list_shuffle)) {
            return false;
        }
        const bool shuffled = ctx.list_shuffle_enabled;
        if (!ui_ci_click_library_handle(runtime, scene, ctx, pump_frame, ctx.handles.list_sort)) {
            return false;
        }
        return toggled && shuffled && !ctx.list_shuffle_enabled && ctx.list_sort == before;
    }

    template<typename RuntimeT, typename SceneT, typename ContextT, typename PumpFrameFn>
    bool ui_ci_library_shuffle_toggle(RuntimeT& runtime,
                                      SceneT& scene,
                                      ContextT& ctx,
                                      PumpFrameFn& pump_frame) {
        ctx.set_page(player::PlayerPage::Library);
        ctx.set_library_tab(player::LibraryTab::Songs);
        if (ctx.list_shuffle_enabled) {
            ctx.toggle_library_shuffle();
        }
        pump_frame();
        if (!ui_ci_click_library_handle(runtime, scene, ctx, pump_frame, ctx.handles.list_shuffle)) {
            return false;
        }
        const bool enabled = ctx.list_shuffle_enabled;
        const auto seed = ctx.list_shuffle_seed;
        const bool can_step_back = ctx.library_can_step_back();
        if (!ui_ci_click_library_handle(runtime, scene, ctx, pump_frame, ctx.handles.list_shuffle)) {
            return false;
        }
        return enabled
            && !ctx.list_shuffle_enabled
            && seed != 0
            && can_step_back
            && ctx.last_list_count >= 0;
    }

    template<typename RuntimeT, typename SceneT, typename ContextT, typename PumpFrameFn>
    bool ui_ci_library_group_context(RuntimeT& runtime,
                                     SceneT& scene,
                                     ContextT& ctx,
                                     PumpFrameFn& pump_frame) {
        if (!ui_ci_library_has_tracks(scene, ctx)) return false;
        ctx.set_page(player::PlayerPage::Library);
        ctx.set_library_tab(player::LibraryTab::Albums);
        pump_frame();
        if (!ctx.library_shows_group_rows() || ctx.list_order.empty()) return false;
        const Rect list = scene.world_rect(ctx.handles.list);
        if (list.w <= 0 || list.h <= 0) return false;
        ui_ci_click(runtime,
                    list.x + list.w - 12,
                    list.y + ContextT::kLibraryListRowHeight / 2);
        pump_frame();
        return ctx.library_tab == player::LibraryTab::Albums
            && !ctx.library_context_key.empty()
            && ctx.last_list_count >= 0;
    }

    template<typename RuntimeT, typename SceneT, typename ContextT, typename PumpFrameFn>
    bool ui_ci_library_path_back(RuntimeT& runtime,
                                 SceneT& scene,
                                 ContextT& ctx,
                                 PumpFrameFn& pump_frame) {
        if (!ui_ci_library_group_context(runtime, scene, ctx, pump_frame)) return false;
        if (!ui_ci_library_handle_has_rect(scene, ctx.handles.list_path_hit)) return false;
        if (!ui_ci_click_library_handle(runtime, scene, ctx, pump_frame, ctx.handles.list_path_hit)) {
            return false;
        }
        return ctx.library_context_key.empty()
            && ctx.library_tab == player::LibraryTab::Albums;
    }

    template<typename RuntimeT, typename SceneT, typename ContextT, typename PumpFrameFn>
    bool ui_ci_library_action_menu_open_invoke(RuntimeT& runtime,
                                               SceneT& scene,
                                               ContextT& ctx,
                                               PumpFrameFn& pump_frame) {
        if (!ui_ci_library_open_first_track_action(runtime, scene, ctx, pump_frame)) return false;
        const int selected_before = ctx.last_list_selected;
        const int track_before = ctx.track_index;
        if (!ui_ci_click_library_handle(runtime, scene, ctx, pump_frame, ctx.handles.list_action_items[1])) {
            return false;
        }
        const bool info_opened = ctx.list_info_popup_track_index >= 0
            && ui_ci_library_handle_has_rect(scene, ctx.handles.list_info_card);
        ctx.close_list_info_popup();
        pump_frame();
        if (!ui_ci_library_open_tail_action(runtime, scene, ctx, pump_frame)) return false;
        if (!ui_ci_click_library_handle(runtime, scene, ctx, pump_frame, ctx.handles.list_action_items[0])) {
            return false;
        }
        return info_opened
            && ctx.list_action_menu_index < 0
            && ctx.track_index >= 0
            && ctx.last_list_selected == selected_before
            && (ctx.track_index == track_before || ctx.track_index == selected_before);
    }

    template<typename RuntimeT, typename SceneT, typename ContextT, typename PumpFrameFn>
    bool ui_ci_library_action_menu_keyboard(RuntimeT& runtime,
                                            SceneT& scene,
                                            ContextT& ctx,
                                            PumpFrameFn& pump_frame) {
        if (!ui_ci_library_open_first_track_action(runtime, scene, ctx, pump_frame)) return false;
        ctx.handle_input_command(player::PlayerInputCommand::Down);
        pump_frame();
        if (ctx.focused_list_action_menu_item() != 1) {
            ctx.handle_input_command(player::PlayerInputCommand::Down);
            pump_frame();
        }
        if (ctx.focused_list_action_menu_item() != 1) return false;
        ctx.handle_input_command(player::PlayerInputCommand::Enter);
        pump_frame();
        const bool info_opened = ctx.list_info_popup_track_index >= 0
            && ui_ci_library_handle_has_rect(scene, ctx.handles.list_info_card);
        ctx.handle_input_command(player::PlayerInputCommand::Back);
        pump_frame();
        if (ctx.list_info_popup_track_index >= 0) return false;
        if (!ui_ci_library_open_first_track_action(runtime, scene, ctx, pump_frame)) return false;
        ctx.handle_input_command(player::PlayerInputCommand::Back);
        pump_frame();
        return info_opened
            && ctx.list_action_menu_index < 0;
    }

    template<typename RuntimeT, typename SceneT, typename ContextT, typename PumpFrameFn>
    bool ui_ci_library_info_popup_open_close(RuntimeT& runtime,
                                             SceneT& scene,
                                             ContextT& ctx,
                                             PumpFrameFn& pump_frame) {
        if (!ui_ci_library_open_first_track_action(runtime, scene, ctx, pump_frame)) return false;
        if (!ui_ci_click_library_handle(runtime, scene, ctx, pump_frame, ctx.handles.list_action_items[1])) {
            return false;
        }
        const char* title = scene.text(ctx.handles.list_info_title);
        const char* meta = scene.text(ctx.handles.list_info_meta);
        const char* path = scene.text(ctx.handles.list_info_path);
        const bool opened = ctx.list_info_popup_track_index >= 0
            && ui_ci_library_handle_has_rect(scene, ctx.handles.list_info_card)
            && title && title[0] != '\0'
            && meta && meta[0] != '\0'
            && path && path[0] != '\0';
        if (!opened) return false;
        if (!ui_ci_click_library_handle(runtime, scene, ctx, pump_frame, ctx.handles.list_info_close)) {
            return false;
        }
        const bool closed_by_button = ctx.list_info_popup_track_index < 0
            && ctx.list_action_menu_index < 0;
        if (!ui_ci_library_open_first_track_action(runtime, scene, ctx, pump_frame)) return false;
        if (!ui_ci_click_library_handle(runtime, scene, ctx, pump_frame, ctx.handles.list_action_items[1])) {
            return false;
        }
        const Rect scrim = scene.world_rect(ctx.handles.list_info_scrim);
        if (scrim.w <= 0 || scrim.h <= 0) return false;
        ui_ci_click(runtime, scrim.x + 4, scrim.y + 4);
        pump_frame();
        const bool closed_by_scrim = ctx.list_info_popup_track_index < 0
            && ctx.list_action_menu_index < 0;
        return closed_by_button && closed_by_scrim;
    }

    template<typename ContextT>
    bool ui_ci_playback_queue_equals_list_order(const ContextT& ctx) noexcept {
        if (ctx.playback.queue_count() != static_cast<int>(ctx.list_order.size())) return false;
        for (std::size_t i = 0; i < ctx.list_order.size(); ++i) {
            if (ctx.playback.queue_track_at(static_cast<int>(i)) != ctx.list_order[i]) return false;
        }
        return true;
    }

    template<typename ContextT>
    bool ui_ci_playback_queue_contains_track(const ContextT& ctx, int track_index) noexcept {
        const int count = ctx.playback.queue_count();
        for (int i = 0; i < count; ++i) {
            if (ctx.playback.queue_track_at(i) == track_index) return true;
        }
        return false;
    }

    template<typename ContextT, typename PumpFrameFn>
    bool ui_ci_prepare_library_songs_queue_view(ContextT& ctx, PumpFrameFn& pump_frame) {
        ctx.set_page(player::PlayerPage::Library);
        ctx.set_library_tab(player::LibraryTab::Songs);
        if (ctx.list_shuffle_enabled) {
            ctx.toggle_library_shuffle();
        }
        pump_frame();
        return !ctx.list_order.empty();
    }

    template<typename SceneT, typename ContextT, typename PumpFrameFn>
    bool ui_ci_playback_queue_library_view_order(SceneT& scene,
                                                 ContextT& ctx,
                                                 PumpFrameFn& pump_frame) {
        if (!ui_ci_library_has_tracks(scene, ctx)) return false;
        if (!ui_ci_prepare_library_songs_queue_view(ctx, pump_frame)) return false;

        const int row = (ctx.list_order.size() > 1) ? 1 : 0;
        const int expected_track = ctx.list_order[static_cast<std::size_t>(row)];
        const auto generation_before = ctx.playback.queue_generation();
        ctx.activate_list_index(row);
        pump_frame();

        return ctx.track_index == expected_track
            && ctx.playback.queue_generation() != generation_before
            && ctx.playback.queue_position() == row
            && ui_ci_playback_queue_equals_list_order(ctx);
    }

    template<typename SceneT, typename ContextT, typename PumpFrameFn>
    bool ui_ci_playback_queue_next_prev_wrap(SceneT& scene,
                                             ContextT& ctx,
                                             PumpFrameFn& pump_frame) {
        if (!ui_ci_library_has_tracks(scene, ctx)) return false;
        if (!ui_ci_prepare_library_songs_queue_view(ctx, pump_frame)) return false;

        const int last_row = static_cast<int>(ctx.list_order.size()) - 1;
        const int first_track = ctx.list_order.front();
        const int last_track = ctx.list_order.back();
        ctx.activate_list_index(last_row);
        pump_frame();
        if (ctx.track_index != last_track) return false;

        ctx.switch_track(1);
        pump_frame();
        if (ctx.track_index != first_track || ctx.playback.queue_position() != 0) return false;

        ctx.switch_track(-1);
        pump_frame();
        return ctx.track_index == last_track
            && ctx.playback.queue_position() == last_row
            && ui_ci_playback_queue_equals_list_order(ctx);
    }

    template<typename SceneT, typename ContextT, typename PumpFrameFn>
    bool ui_ci_playback_queue_shuffle_capture(SceneT& scene,
                                              ContextT& ctx,
                                              PumpFrameFn& pump_frame) {
        if (!ui_ci_library_has_tracks(scene, ctx)) return false;
        ctx.set_page(player::PlayerPage::Library);
        ctx.set_library_tab(player::LibraryTab::Songs);
        if (!ctx.list_shuffle_enabled) {
            ctx.toggle_library_shuffle();
        }
        pump_frame();
        if (!ctx.list_shuffle_enabled || ctx.list_order.empty()) return false;

        ctx.activate_list_index(0);
        pump_frame();
        if (!ui_ci_playback_queue_equals_list_order(ctx)) return false;

        std::vector<int> captured{};
        const int captured_count = ctx.playback.queue_count();
        captured.reserve(static_cast<std::size_t>(captured_count));
        for (int i = 0; i < captured_count; ++i) {
            captured.push_back(ctx.playback.queue_track_at(i));
        }
        ctx.toggle_library_shuffle();
        pump_frame();
        if (ctx.list_shuffle_enabled) return false;
        if (ctx.playback.queue_count() != static_cast<int>(captured.size())) return false;
        for (std::size_t i = 0; i < captured.size(); ++i) {
            if (ctx.playback.queue_track_at(static_cast<int>(i)) != captured[i]) return false;
        }
        return true;
    }

    template<typename ContextT, typename PumpFrameFn>
    bool ui_ci_playback_queue_capture_first_context(ContextT& ctx,
                                                    PumpFrameFn& pump_frame,
                                                    player::LibraryTab tab) {
        ctx.set_page(player::PlayerPage::Library);
        ctx.set_library_tab(tab);
        if (ctx.list_shuffle_enabled) {
            ctx.toggle_library_shuffle();
        }
        pump_frame();
        if (!ctx.open_first_library_group_for_preview()) return false;
        pump_frame();
        if (ctx.library_shows_group_rows() || ctx.list_order.empty()) return false;

        const int expected_track = ctx.list_order.front();
        ctx.activate_list_index(0);
        pump_frame();
        return ctx.track_index == expected_track
            && ctx.playback.queue_position() == 0
            && ui_ci_playback_queue_equals_list_order(ctx);
    }

    template<typename SceneT, typename ContextT, typename PumpFrameFn>
    bool ui_ci_playback_queue_context_album_artist(SceneT& scene,
                                                   ContextT& ctx,
                                                   PumpFrameFn& pump_frame) {
        if (!ui_ci_library_has_tracks(scene, ctx)) return false;
        const bool album_ok = ui_ci_playback_queue_capture_first_context(
            ctx, pump_frame, player::LibraryTab::Albums);
        const bool artist_ok = ui_ci_playback_queue_capture_first_context(
            ctx, pump_frame, player::LibraryTab::Artists);
        return album_ok && artist_ok;
    }

    template<typename SceneT, typename ContextT, typename PumpFrameFn>
    bool ui_ci_playback_queue_mode_repeat_random_scope(SceneT& scene,
                                                       ContextT& ctx,
                                                       PumpFrameFn& pump_frame) {
        if (!ui_ci_library_has_tracks(scene, ctx)) return false;
        if (!ui_ci_prepare_library_songs_queue_view(ctx, pump_frame)) return false;

        ctx.activate_list_index(0);
        pump_frame();
        const int selected = ctx.track_index;
        ctx.set_play_mode(0);
        ctx.handle_track_end();
        pump_frame();
        const int expected_sequential = ctx.list_order.size() > 1
            ? ctx.list_order[1]
            : selected;
        if (ctx.track_index != expected_sequential) {
            ctx.stop_playback();
            pump_frame();
            return false;
        }
        if (ctx.list_order.size() > 1) {
            ctx.activate_list_index(static_cast<int>(ctx.list_order.size()) - 1);
            pump_frame();
            const int last = ctx.track_index;
            ctx.handle_track_end();
            pump_frame();
            if (ctx.track_index != last || ctx.has_active_playback()) {
                ctx.stop_playback();
                pump_frame();
                return false;
            }
            ctx.activate_list_index(0);
            pump_frame();
        }
        ctx.set_play_mode(1);
        ctx.handle_track_end();
        pump_frame();
        if (ctx.track_index != selected) {
            ctx.set_play_mode(0);
            ctx.stop_playback();
            pump_frame();
            return false;
        }

        ctx.set_play_mode(2);
        ctx.handle_track_end();
        pump_frame();
        const bool random_scoped = ui_ci_playback_queue_contains_track(ctx, ctx.track_index);
        ctx.set_play_mode(0);
        ctx.stop_playback();
        pump_frame();
        return random_scoped && ctx.playback.queue_count() > 0;
    }

    template<typename SceneT, typename ContextT, typename PumpFrameFn>
    bool ui_ci_playback_queue_skips_unavailable_on_end(SceneT& scene,
                                                       ContextT& ctx,
                                                       PumpFrameFn& pump_frame) {
        if (!ui_ci_library_has_tracks(scene, ctx)) return false;
        (void)ctx;
        (void)pump_frame;
        player::PlaybackSession<4> probe{};
        if (!probe.set_track_slot(0, "/ui-ci/first.wav", true)
            || !probe.set_track_slot(1, "/ui-ci/missing.wav", false)
            || !probe.set_track_slot(2, "/ui-ci/next.wav", true)) {
            return false;
        }
        const std::array<int, 3> order{{0, 1, 2}};
        if (!probe.set_queue_from_order(order, 0)) {
            return false;
        }
        player::FixedString<128> status{};
        if (!probe.load_track_index(0, &status)) {
            return false;
        }
        probe.set_playback_mode(player::PlaybackMode::Sequential);
        return probe.resolve_track_for_playback_mode() == 2
            && probe.track_for_queue_delta(1) == 1;
    }

    template<typename RuntimeT, typename SceneT, typename ContextT, typename PumpFrameFn>
    bool ui_ci_playback_queue_run_state_drop_advance(RuntimeT& runtime,
                                                     SceneT& scene,
                                                     ContextT& ctx,
                                                     PumpFrameFn& pump_frame) {
        if (!ui_ci_library_has_tracks(scene, ctx)) return false;
        auto* app = runtime.app();
        if (!app) return false;
        if (!ui_ci_prepare_library_songs_queue_view(ctx, pump_frame)) return false;
        if (ctx.list_order.size() < 2) return true;

        audio::PlayerConfig cfg{};
        cfg.capture_output = false;
        audio::AudioPlayer edge_player{cfg, g_clock};
        ctx.bind_player(edge_player);
        auto cleanup = [&](bool ok) {
            ctx.playback.stop_playback();
            ctx.set_play_mode(0);
            ctx.bind_player(app->player());
            pump_frame();
            return ok;
        };

        ctx.activate_list_index(0);
        pump_frame();
        const int expected_next = ctx.list_order[1];
        ctx.set_play_mode(0);
        player::FixedString<128> status{};
        if (!ctx.playback.start(status)) {
            return cleanup(false);
        }
        ctx.observe_player_run_state(true, audio::PlayerState::playing);
        ctx.observe_player_run_state(false, audio::PlayerState::idle);
        const bool advanced = ctx.track_index == expected_next
            && ctx.playback.playing()
            && ctx.playback.track_ready();
        if (!advanced) {
            std::printf("[ui-ci] run_state_drop_advance detail expected=%d current=%d queue=%d/%d playing=%d paused=%d ready=%d state=%u\n",
                        expected_next,
                        ctx.track_index,
                        ctx.playback.queue_position(),
                        ctx.playback.queue_count(),
                        ctx.playback.playing() ? 1 : 0,
                        ctx.playback.paused() ? 1 : 0,
                        ctx.playback.track_ready() ? 1 : 0,
                        static_cast<unsigned>(audio::PlayerState::idle));
        }
        return cleanup(advanced);
    }

    template<typename SceneT, typename ContextT, typename PumpFrameFn>
    bool ui_ci_library_unsupported_selection_preserves_active_track(SceneT& scene,
                                                                    ContextT& ctx,
                                                                    PumpFrameFn& pump_frame) {
        if (!ui_ci_library_has_tracks(scene, ctx)) return false;
        if (!ui_ci_prepare_library_songs_queue_view(ctx, pump_frame)) return false;

        const int playable_row = 0;
        const int playable_track = ctx.list_order[static_cast<std::size_t>(playable_row)];
        ctx.activate_list_index(playable_row);
        pump_frame();
        if (!ctx.playback.track_ready()) return false;

        const player::StorageView original_view = ctx.storage;
        const int original_track_index = ctx.track_index;
        const std::string original_path{ctx.track_path() ? ctx.track_path() : ""};
        const auto original_resume_mode = ctx.current_playback_resume_mode();
        const bool original_ready = ctx.playback.track_ready();

        player::StorageState fixture{};
        fixture.fs_ready = true;
        fixture.has_tracks = true;
        fixture.status.assign("Ready");
        fixture.mount_status.assign("Ready");
        fixture.media_stats = original_view.media_stats ? *original_view.media_stats : player::MediaLibraryStats{};

        const auto* tracks = original_view.tracks;
        if (!tracks || tracks->empty()) return false;
        for (std::size_t i = 0; i < tracks->size(); ++i) {
            player::FixedString<260> path{};
            path.assign((*tracks)[i].view());
            if (!fixture.tracks.push_back(path)) return false;
        }

        player::FixedString<260> unsupported_path{};
        unsupported_path.assign("/music/ui-ci/Unsupported Fixture.aac");
        if (!fixture.tracks.push_back(unsupported_path)) return false;
        fixture.has_tracks = fixture.tracks.size() > 0;
        fixture.media_stats.track_count = fixture.tracks.size();
        player::ensure_track_labels(fixture);

        ctx.apply_storage_view(player::make_storage_view(fixture), false);
        pump_frame();
        const int unsupported_track = static_cast<int>(fixture.tracks.size()) - 1;
        const int unsupported_row = ctx.track_index_to_list(unsupported_track);
        if (unsupported_row < 0) {
            ctx.apply_storage_view(original_view, false);
            pump_frame();
            return false;
        }

        const char* unsupported_tail = player::PlayerController::list_view_tail(
            &ctx,
            static_cast<std::uint16_t>(unsupported_row));
        const bool row_marked_unsupported =
            unsupported_tail && std::string_view{unsupported_tail} == "Unsupported";

        ctx.set_library_selected_index_for_preview(unsupported_row);
        pump_frame();
        const bool selection_explains_unsupported =
            ctx.last_status_text.view().find("Unsupported format") != std::string_view::npos;

        const bool popup_opened = ctx.open_library_info_popup_for_preview(unsupported_row);
        pump_frame();
        const char* popup_meta = scene.text(ctx.handles.list_info_meta);
        const bool popup_explains_unsupported = popup_opened
            && popup_meta
            && std::string_view{popup_meta}.find("Unsupported format") != std::string_view::npos;
        ctx.close_list_info_popup();
        pump_frame();

        const bool menu_opened = ctx.open_library_action_menu_for_preview(unsupported_row);
        pump_frame();
        const char* menu_primary = scene.text(ctx.handles.list_action_items[0]);
        const bool menu_marks_unsupported = menu_opened
            && menu_primary
            && std::string_view{menu_primary} == "Unsupported";
        ctx.close_list_action_menu();
        pump_frame();

        const std::size_t recent_count_before_unsupported =
            ctx.recent_track_history.tracks.size();
        ctx.activate_list_index(unsupported_row);
        pump_frame();

        const bool preserved = row_marked_unsupported
            && selection_explains_unsupported
            && popup_explains_unsupported
            && menu_marks_unsupported
            && ctx.track_index == original_track_index
            && ctx.playback.track_ready() == original_ready
            && std::string_view{ctx.track_path() ? ctx.track_path() : ""} == original_path
            && ctx.current_playback_resume_mode() == original_resume_mode
            && ctx.last_list_selected == unsupported_row
            && ctx.recent_track_history.tracks.size() == recent_count_before_unsupported
            && ctx.last_status_text.view() == std::string_view{"Unsupported format"}
            && ctx.playback.queue_count() == static_cast<int>(fixture.tracks.size())
            && ctx.playback.queue_track_at(ctx.playback.queue_count() - 1) == unsupported_track
            && playable_track == original_track_index;

        ctx.apply_storage_view(original_view, false);
        pump_frame();
        return preserved;
    }

    template<typename RuntimeT, typename SceneT, typename ContextT, typename PumpFrameFn>
    void run_library_function_regressions(UiCiResult& res,
                                          RuntimeT& runtime,
                                          SceneT& scene,
                                          ContextT& ctx,
                                          PumpFrameFn& pump_frame) {
        auto record = [&](const char* name, bool ok, const char* reason) {
            ui_ci_emit(name, ok, ok ? nullptr : reason);
            if (!ok) {
                res.ok = false;
                res.failed++;
            }
        };

        if (!ui_ci_library_has_tracks(scene, ctx)) {
            record("library_tab_switch", true, "skipped_no_tracks");
            record("library_sort_toggle", true, "skipped_no_tracks");
            record("library_shuffle_toggle", true, "skipped_no_tracks");
            record("library_path_back", true, "skipped_no_tracks");
            record("library_group_context", true, "skipped_no_tracks");
            record("library_action_menu_open_invoke", true, "skipped_no_tracks");
            record("library_action_menu_keyboard", true, "skipped_no_tracks");
            record("library_info_popup_open_close", true, "skipped_no_tracks");
            record("playback_queue_library_view_order", true, "skipped_no_tracks");
            record("playback_queue_next_prev_wrap", true, "skipped_no_tracks");
            record("playback_queue_shuffle_capture", true, "skipped_no_tracks");
            record("playback_queue_context_album_artist", true, "skipped_no_tracks");
            record("playback_queue_mode_repeat_random_scope", true, "skipped_no_tracks");
            record("playback_queue_skip_unavailable_on_end", true, "skipped_no_tracks");
            record("playback_queue_run_state_drop_advance", true, "skipped_no_tracks");
            record("library_unsupported_selection_preserves_active_track", true, "skipped_no_tracks");
            return;
        }

        record("library_tab_switch",
               ui_ci_library_tab_switch(runtime, scene, ctx, pump_frame),
               "tab_switch");
        record("library_sort_toggle",
               ui_ci_library_sort_toggle(runtime, scene, ctx, pump_frame),
               "sort_toggle");
        record("library_shuffle_toggle",
               ui_ci_library_shuffle_toggle(runtime, scene, ctx, pump_frame),
               "shuffle_toggle");
        record("library_group_context",
               ui_ci_library_group_context(runtime, scene, ctx, pump_frame),
               "group_context");
        record("library_path_back",
               ui_ci_library_path_back(runtime, scene, ctx, pump_frame),
               "path_back");
        record("library_action_menu_open_invoke",
               ui_ci_library_action_menu_open_invoke(runtime, scene, ctx, pump_frame),
               "action_menu_open_invoke");
        record("library_action_menu_keyboard",
               ui_ci_library_action_menu_keyboard(runtime, scene, ctx, pump_frame),
               "action_menu_keyboard");
        record("library_info_popup_open_close",
               ui_ci_library_info_popup_open_close(runtime, scene, ctx, pump_frame),
               "info_popup_open_close");
        record("playback_queue_library_view_order",
               ui_ci_playback_queue_library_view_order(scene, ctx, pump_frame),
               "library_view_order");
        record("playback_queue_next_prev_wrap",
               ui_ci_playback_queue_next_prev_wrap(scene, ctx, pump_frame),
               "next_prev_wrap");
        record("playback_queue_shuffle_capture",
               ui_ci_playback_queue_shuffle_capture(scene, ctx, pump_frame),
               "shuffle_capture");
        record("playback_queue_context_album_artist",
               ui_ci_playback_queue_context_album_artist(scene, ctx, pump_frame),
               "context_album_artist");
        record("playback_queue_mode_repeat_random_scope",
               ui_ci_playback_queue_mode_repeat_random_scope(scene, ctx, pump_frame),
               "mode_repeat_random_scope");
        record("playback_queue_skip_unavailable_on_end",
               ui_ci_playback_queue_skips_unavailable_on_end(scene, ctx, pump_frame),
               "skip_unavailable_on_end");
        record("playback_queue_run_state_drop_advance",
               ui_ci_playback_queue_run_state_drop_advance(runtime, scene, ctx, pump_frame),
               "run_state_drop_advance");
        record("library_unsupported_selection_preserves_active_track",
               ui_ci_library_unsupported_selection_preserves_active_track(scene, ctx, pump_frame),
               "unsupported_selection_preserves_active_track");
    }
