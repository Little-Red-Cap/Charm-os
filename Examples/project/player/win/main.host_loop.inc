    void loop_poll_events(void* ctx, charm::system::ClockTick, charm::system::ClockTick) noexcept {
        auto* state = static_cast<PlayerLoopState*>(ctx);
        if (!state || !state->running || !state->runtime) {
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
            const auto input_batch = translate_sdl_player_input(evt);
            for (std::size_t i = 0; i < input_batch.count; ++i) {
                state->runtime->dispatch_input(input_batch.events[i]);
            }
        }
    }

    void loop_update(void* ctx, charm::system::ClockTick now_us, charm::system::ClockTick) noexcept {
        auto* state = static_cast<PlayerLoopState*>(ctx);
        if (!state || !state->runtime) {
            return;
        }
        state->runtime->tick(now_us, player::PlayerRuntimeHooks{nullptr, &tick_player_preview_visual});
    }

    void player_frame_overlay(::ui::scene::SceneOverlay& overlay, void* ctx) noexcept {
        auto* state = static_cast<PlayerLoopState*>(ctx);
        if (!state || !state->runtime || !state->runtime->controller() || !state->runtime->platform()) return;
        draw_library_fx(overlay, *state->runtime->controller(), state->runtime->scene_ref());
        draw_now_playing_fx(overlay,
                            *state->runtime->controller(),
                            state->runtime->scene_ref(),
                            state->runtime->t_sec());
    }

    void loop_render(void* ctx, charm::system::ClockTick, charm::system::ClockTick) noexcept {
        auto* state = static_cast<PlayerLoopState*>(ctx);
        if (!state || !state->runtime || !state->display_sink) {
            return;
        }
        prepare_screenshot_page(*state);
        (void)state->runtime->render(state->display_sink, &player_frame_overlay, state);
        if (flush_screenshot(*state)) {
            return;
        }
    }

    void run_interactive_preview_loop(SdlHostRuntime& runtime, PreviewOptions& options) {
        int win_w = screen_width;
        int win_h = screen_height;
        bool running = true;
        PlayerLoopState loop_state{
            .runtime = &(*g_runtime),
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
