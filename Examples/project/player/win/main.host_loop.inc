    void loop_poll_events(void* ctx, charm::system::ClockTick, charm::system::ClockTick) noexcept {
        auto* state = static_cast<PlayerLoopState*>(ctx);
        if (!state || !state->running || !state->app || !state->ctx || !state->platform) {
            return;
        }
        SDL_Event evt{};
        while (SDL_PollEvent(&evt)) {
            if (evt.type == SDL_EVENT_QUIT) {
                *state->running = false;
                break;
            }
            if (evt.type == SDL_EVENT_WINDOW_RESIZED) {
                if (state->win_w) {
                    *state->win_w = static_cast<int>(evt.window.data1);
                }
                if (state->win_h) {
                    *state->win_h = static_cast<int>(evt.window.data2);
                }
            }
            dispatch_sdl_event(state->platform->scene_ref(), *state->app, *state->ctx, evt);
        }
    }

    void loop_update(void* ctx, charm::system::ClockTick now_us, charm::system::ClockTick) noexcept {
        auto* state = static_cast<PlayerLoopState*>(ctx);
        if (!state || !state->app || !state->ctx) {
            return;
        }
        state->t_sec = static_cast<float>(now_us) * 0.000001f;
        state->app->tick();
        state->ctx->tick_player(state->app->player());
        update_spectrum(state->t_sec, state->ctx->is_playing());
    }

    void loop_render(void* ctx, charm::system::ClockTick, charm::system::ClockTick) noexcept {
        auto* state = static_cast<PlayerLoopState*>(ctx);
        if (!state || !state->platform || !state->ctx || !state->scene || !state->display_sink) {
            return;
        }
        prepare_screenshot_page(*state);
        state->platform->clear(kUiBackground);
        state->platform->begin_frame();
        state->platform->scene_ref().set_overlay(
            [](::ui::scene::SceneOverlay& overlay, void* ctx) noexcept {
                auto* state = static_cast<PlayerLoopState*>(ctx);
                if (!state || !state->ctx || !state->scene) return;
                draw_library_fx(overlay, *state->ctx, *state->scene);
                draw_now_playing_fx(overlay, *state->ctx, *state->scene, state->t_sec);
            },
            state);
        if (state->ctx->transition_needs_destination_snapshot()) {
            state->ctx->prepare_transition_destination_snapshot_scene();
            state->platform->render();
            state->ctx->finish_transition_destination_snapshot_capture();
            state->platform->clear(kUiBackground);
            state->platform->begin_frame();
        }
        state->ctx->compose_now_playing_transition_pixel_layer();
        state->platform->render();
        state->platform->end_frame();

        (void)state->display_sink->present(
            state->platform->surface_ref(),
            player::full_player_dirty_region(state->platform->surface_ref()));
        if (flush_screenshot(*state)) {
            return;
        }
    }

    void run_interactive_preview_loop(SdlHostRuntime& runtime, PreviewOptions& options) {
        int win_w = screen_width;
        int win_h = screen_height;
        bool running = true;
        PlayerLoopState loop_state{
            .app = &(*g_app),
            .platform = &g_platform,
            .ctx = &g_ctx,
            .scene = &g_platform.scene_ref(),
            .display_sink = &runtime.display_sink,
            .running = &running,
            .win_w = &win_w,
            .win_h = &win_h,
            .screenshot_path = std::move(options.screenshot_path),
            .screenshot_gif_path = std::move(options.screenshot_gif_path),
            .screenshot_page = options.start_page,
            .home_scroll_y = options.home_scroll_y,
            .screenshot_verbose = options.screenshot_verbose,
            .screenshot_wait_frames = options.screenshot_wait_frames,
            .screenshot_exit = options.screenshot_exit
        };
        charm::system::RunLoop<4> loop{};
        loop.bind_clock(g_clock);
        (void)loop.add_step(charm::system::LoopPhase::io,
                            charm::system::SubmitProjection::event,
                            &loop_poll_events,
                            &loop_state,
                            "player_io");
        (void)loop.add_step(charm::system::LoopPhase::update,
                            charm::system::SubmitProjection::event,
                            &loop_update,
                            &loop_state,
                            "player_update");
        (void)loop.add_step(charm::system::LoopPhase::render,
                            charm::system::SubmitProjection::event,
                            &loop_render,
                            &loop_state,
                            "player_render");
        std::array<char, 384> run_loop_audit{};
        (void)loop.format_audit_json(run_loop_audit.data(), run_loop_audit.size());
        std::printf("[runloop.audit] %s\n", run_loop_audit.data());
        while (running) {
            loop.run_once();
            (void)win_w;
            (void)win_h;
        }
    }
