    struct SdlHostRuntime {
        SDL_Window* window{nullptr};
        SDL_Renderer* renderer{nullptr};
        SDL_Texture* texture{nullptr};
        bool sdl_initialized{false};
    };

    struct PlayerLoopState {
        player::App* app{nullptr};
        player::PlayerPlatform* platform{nullptr};
        PlayerUiContext* ctx{nullptr};
        ::ui::scene::Scene* scene{nullptr};
        SDL_Renderer* renderer{nullptr};
        SDL_Texture* texture{nullptr};
        bool* running{nullptr};
        int* win_w{nullptr};
        int* win_h{nullptr};
        float t_sec{0.0f};
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
            return false;
        }
        runtime.sdl_initialized = true;

        runtime.window = SDL_CreateWindow("Charm Player", screen_width, screen_height, SDL_WINDOW_RESIZABLE);
        if (!runtime.window) {
            shutdown_sdl_host_runtime(runtime);
            return false;
        }

        runtime.renderer = SDL_CreateRenderer(runtime.window, nullptr);
        if (!runtime.renderer) {
            shutdown_sdl_host_runtime(runtime);
            return false;
        }

        runtime.texture = SDL_CreateTexture(runtime.renderer,
                                            SDL_PIXELFORMAT_RGB24,
                                            SDL_TEXTUREACCESS_STREAMING,
                                            screen_width,
                                            screen_height);
        if (!runtime.texture) {
            shutdown_sdl_host_runtime(runtime);
            return false;
        }
        SDL_SetTextureScaleMode(runtime.texture, SDL_SCALEMODE_NEAREST);
        return true;
    }

    void bootstrap_player_preview(const PreviewOptions& options) {
        charm::system::ClockCaps::TimeSource::bind(g_clock);
        player::ui::set_player_system_font_fallback_enabled(!options.disable_system_font_fallback);
        player::AppConfig app_cfg = make_app_config(options);
        g_app.emplace(std::move(app_cfg), g_clock);

        player::init_storage(player::default_storage_config());
        g_app->bind_player(g_ctx);
        g_ctx.bind_scene(g_platform.scene_ref());
        g_ctx.set_start_page(options.start_page);
        (void)g_app->scan_storage();
        g_ctx.apply_storage_view(g_app->storage_view(), false);
        g_platform.build_scene([&](::ui::scene::SceneBuilder& builder) {
            g_app->bind_ui(builder, g_ctx);
        });
        g_ctx.set_page(options.start_page);

        const bool has_track = g_app->bootstrap_player(g_ctx, options.track_index_override, false);
        apply_library_preview_options(options);
        if (has_track && !fs_seek_selftest(g_ctx.track_path())) {
            g_ctx.set_status("Fs seek selftest failed");
        }
    }

    void shutdown_player_preview(SdlHostRuntime& runtime) noexcept {
        if (g_app.has_value()) {
            g_app->shutdown(g_ctx);
        }
        shutdown_sdl_host_runtime(runtime);
    }
