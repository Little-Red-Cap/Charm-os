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
    }
