    struct SdlHostRuntime {
        SDL_Window* window{nullptr};
        SDL_Renderer* renderer{nullptr};
        SDL_Texture* texture{nullptr};
        SdlDisplaySinkState display_state{};
        player::PlayerDisplaySink display_sink{};
        bool sdl_initialized{false};
    };

    struct PlayerLoopState {
        PlayerRuntime* runtime{nullptr};
        player::PlayerDisplaySink* display_sink{nullptr};
        bool* running{nullptr};
        int* win_w{nullptr};
        int* win_h{nullptr};
        std::string screenshot_path{};
        std::string screenshot_gif_path{};
        player::PlayerPage screenshot_page{player::PlayerPage::Library};
        int home_scroll_y{-1};
        bool screenshot_verbose{false};
        int screenshot_wait_frames{0};
        bool screenshot_exit{false};
    };

    void shutdown_sdl_host_runtime(SdlHostRuntime& runtime) noexcept {
        if (runtime.texture) {
            SDL_DestroyTexture(runtime.texture);
            runtime.texture = nullptr;
        }
        if (runtime.renderer) {
            SDL_DestroyRenderer(runtime.renderer);
            runtime.renderer = nullptr;
        }
        if (runtime.window) {
            SDL_DestroyWindow(runtime.window);
            runtime.window = nullptr;
        }
        if (runtime.sdl_initialized) {
            SDL_Quit();
            runtime.sdl_initialized = false;
        }
    }

    bool init_sdl_host_runtime(SdlHostRuntime& runtime) noexcept {
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
            std::fprintf(stderr, "[player.host] SDL_Init failed: %s\n", SDL_GetError());
            return false;
        }
        runtime.sdl_initialized = true;

        runtime.window = SDL_CreateWindow("Charm Player", screen_width, screen_height, SDL_WINDOW_RESIZABLE);
        if (!runtime.window) {
            std::fprintf(stderr, "[player.host] SDL_CreateWindow failed: %s\n", SDL_GetError());
            shutdown_sdl_host_runtime(runtime);
            return false;
        }

        runtime.renderer = SDL_CreateRenderer(runtime.window, nullptr);
        if (!runtime.renderer) {
            std::fprintf(stderr, "[player.host] SDL_CreateRenderer failed: %s\n", SDL_GetError());
            shutdown_sdl_host_runtime(runtime);
            return false;
        }

        runtime.texture = SDL_CreateTexture(runtime.renderer,
                                            sdl_pixel_format(g_platform.surface_ref().pixel_format),
                                            SDL_TEXTUREACCESS_STREAMING,
                                            g_platform.width(),
                                            g_platform.height());
        if (!runtime.texture) {
            std::fprintf(stderr, "[player.host] SDL_CreateTexture failed: %s\n", SDL_GetError());
            shutdown_sdl_host_runtime(runtime);
            return false;
        }
        SDL_SetTextureScaleMode(runtime.texture, SDL_SCALEMODE_NEAREST);
        runtime.display_state = SdlDisplaySinkState{runtime.renderer, runtime.texture};
        runtime.display_sink = make_sdl_display_sink(runtime.display_state);
        return true;
    }

    void tick_player_preview_visual(void*, float t_sec, bool active) noexcept {
        update_spectrum(t_sec, active);
    }

    void bootstrap_player_preview(const PreviewOptions& options) {
        player::ui::set_player_system_font_fallback_enabled(!options.disable_system_font_fallback);
        player::AppConfig app_cfg = make_app_config(options);
        player::PlayerRuntimeConfig<player::PlayerPage> runtime_cfg{
            .app_config = std::move(app_cfg),
            .storage_config = player::default_storage_config(),
            .start_page = options.start_page,
            .initial_track_index = options.track_index_override,
            .auto_start = false,
            .clear_color = kUiBackground,
        };
        g_runtime.emplace(g_clock, g_platform, g_ctx, std::move(runtime_cfg));
        const bool has_track = g_runtime->bootstrap();
        apply_library_preview_options(options);
        if (has_track && !fs_seek_selftest(g_ctx.track_path())) {
            g_ctx.set_status("Fs seek selftest failed");
        }
    }

    void shutdown_player_preview(SdlHostRuntime& runtime) noexcept {
        if (g_runtime.has_value()) {
            g_runtime->shutdown();
            g_runtime.reset();
        }
        shutdown_sdl_host_runtime(runtime);
    }
