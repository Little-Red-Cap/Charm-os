    bool ui_ci_static_cover_provider(void*,
                                     const player::CoverResourceRequest& request,
                                     player::CoverResourceView& out) noexcept;

    struct UiCiTouchSampleScript {
        std::array<player::PlayerTouchSample, 4> samples{};
        std::size_t count{0};
        std::size_t index{0};
    };

    std::optional<player::PlayerTouchSample> ui_ci_read_touch_sample(void* ctx) noexcept {
        auto* script = static_cast<UiCiTouchSampleScript*>(ctx);
        if (!script || script->index >= script->count) {
            return std::nullopt;
        }
        return script->samples[script->index++];
    }

    bool run_board_port_bindings_ci() noexcept {
        static constexpr std::array<std::byte, 16> kFontPackage{
            std::byte{0x43}, std::byte{0x48}, std::byte{0x52}, std::byte{0x4d},
            std::byte{0x46}, std::byte{0x4f}, std::byte{0x4e}, std::byte{0x54},
            std::byte{0x00}, std::byte{0x01}, std::byte{0x02}, std::byte{0x03},
            std::byte{0x04}, std::byte{0x05}, std::byte{0x06}, std::byte{0x07},
        };
        static std::array<std::byte, player::PlayerOwnedDisplayBuffer::buffer_bytes> board_buffer{};
        static player::PlayerBoardRuntimeStorage<PlayerUiContext, player::PlayerPage> board_runtime_storage{};

        UiCiTouchSampleScript touch_script{
            .samples = {
                player::PlayerTouchSample{true, 10.0f, 20.0f, 0, 100},
                player::PlayerTouchSample{false, 10.0f, 20.0f, 0, 101},
                player::PlayerTouchSample{},
                player::PlayerTouchSample{},
            },
            .count = 2,
            .index = 0,
        };
        player::PlayerBoardPortConfig package_config{
            .framebuffer = player::PlayerBoardFramebuffer{
                .pixels = board_buffer.data(),
                .width = screen_width,
                .height = screen_height,
                .stride_bytes = player::PlayerOwnedDisplayBuffer::stride_bytes,
                .pixel_format = player::PlayerOwnedDisplayBuffer::pixel_format,
            },
            .display_callbacks = {},
            .touch_source = player::PlayerTouchSampleSource{&touch_script, &ui_ci_read_touch_sample},
            .font_package = player::PlayerBoardFontPackageView{
                .data = kFontPackage.data(),
                .size_bytes = kFontPackage.size(),
                .key = "ui-ci-board-font-package",
                .small_px = 13,
                .normal_px = 17,
                .large_px = 72,
            },
            .cover_resource_records = ui_ci_static_cover_record_table(),
            .player_config = g_player_cfg,
        };
        player::PlayerBoardPortConfig runtime_config_port = package_config;
        runtime_config_port.font_package = {};

        player::PlayerBoardDisplaySinkState binding_sink_state{};
        const auto bindings = player::make_player_board_port_bindings(package_config, binding_sink_state);
        player::PlayerTouchAdapterState touch_state{};
        const auto touch_batch = player::read_player_board_touch_events(package_config, touch_state);
        const auto& font = bindings.app_config.font_resources;

        PlayerUiContext board_ctx{};
        player::PlayerBoardRuntimeConfig<player::PlayerPage> runtime_config{
            .port = runtime_config_port,
            .storage_config = player::default_storage_config(),
            .start_page = player::PlayerPage::Home,
            .initial_track_index = 0,
            .auto_start = false,
            .clear_color = kUiBackground,
        };
        player::PlayerDisplaySink runtime_sink =
            player::make_player_board_runtime(g_clock, board_ctx, runtime_config, board_runtime_storage);
        if (board_runtime_storage.runtime) {
            (void)board_runtime_storage.runtime->bootstrap();
        }
        const bool bootstrap_ok = board_runtime_storage.runtime
            && board_runtime_storage.runtime->app() != nullptr;
        if (bootstrap_ok) {
            board_ctx.cover_strategy = player::CoverStrategy::embedded_only;
            board_ctx.cover_ready = true;
            board_ctx.cover_path.clear();
            board_ctx.cover_embedded_path.assign("/ui-ci/static-cover");
            board_ctx.cover_folder_path.clear();
            board_ctx.cover_failed_embedded_path.clear();
            board_ctx.cover_failed_folder_path.clear();
            board_ctx.update_cover_image();
        }
        if (board_runtime_storage.runtime) {
            board_runtime_storage.runtime->tick(g_clock.now_us());
        }
        const bool render_ok = board_runtime_storage.runtime
            && board_runtime_storage.runtime->render(&runtime_sink);
        const auto presented = board_runtime_storage.display_sink_state.last_surface;
        const bool runtime_ok = bootstrap_ok
            && render_ok
            && board_runtime_storage.platform.has_value()
            && presented.valid()
            && presented.pixels == board_buffer.data()
            && board_runtime_storage.display_sink_state.present_count == 1
            && ::ui::gfx::image_id_valid(board_ctx.cover_image.image_id)
            && board_ctx.cover_image.width == 2
            && board_ctx.cover_image.height == 2
            && board_ctx.cover_image.argb.size() == 4
            && board_ctx.cover_image.path == "/ui-ci/static-cover-resource";
        board_runtime_storage.reset();

        return bindings.valid()
            && bindings.surface.pixels == board_buffer.data()
            && bindings.surface.width == screen_width
            && bindings.surface.height == screen_height
            && bindings.surface.stride_bytes == player::PlayerOwnedDisplayBuffer::stride_bytes
            && bindings.surface.pixel_format == player::PlayerOwnedDisplayBuffer::pixel_format
            && font.kind == player::FontResourceKind::Package
            && font.has_package_resource()
            && font.package_data == kFontPackage.data()
            && font.package_size_bytes == kFontPackage.size()
            && font.package_key.view() == std::string_view{"ui-ci-board-font-package"}
            && font.small_px == 13
            && font.normal_px == 17
            && font.large_px == 72
            && touch_batch.count == 2
            && touch_batch.events[0].pointer_action == player::PlayerPointerAction::Down
            && touch_batch.events[1].pointer_action == player::PlayerPointerAction::Up
            && runtime_ok;
    }

    void ui_ci_pointer(PlayerRuntime& runtime,
                       std::uint32_t ms,
                       player::PlayerPointerAction action,
                       bool down,
                       int x,
                       int y) {
        runtime.dispatch_input(player::PlayerInputEvent::make_pointer(
            ms,
            action,
            player::PlayerPointerSample{
                down,
                static_cast<float>(x),
                static_cast<float>(y),
                0}));
    }

    void ui_ci_click(PlayerRuntime& runtime, int x, int y) {
        ui_ci_pointer(runtime, 0, player::PlayerPointerAction::Down, true, x, y);
        ui_ci_pointer(runtime, 1, player::PlayerPointerAction::Up, false, x, y);
    }

    void ui_ci_pump_frame(PlayerRuntime& runtime, player::PlayerDisplaySink* display_sink) {
        runtime.tick(g_clock.now_us());
        (void)runtime.render(display_sink);
    }

    struct RuntimeMemoryProbeHostStorage {
        static std::array<std::byte, player::PlayerOwnedDisplayBuffer::buffer_bytes> external_buffer;
        static player::PlayerPlatform memory_platform;
        static PlayerUiContext memory_ctx;
        static std::optional<PlayerRuntime> memory_runtime;
    };

    std::array<std::byte, player::PlayerOwnedDisplayBuffer::buffer_bytes> RuntimeMemoryProbeHostStorage::external_buffer{};
    player::PlayerPlatform RuntimeMemoryProbeHostStorage::memory_platform{player::PlayerDisplaySurface{
        RuntimeMemoryProbeHostStorage::external_buffer.data(),
        screen_width,
        screen_height,
        player::PlayerOwnedDisplayBuffer::stride_bytes,
        player::PlayerOwnedDisplayBuffer::pixel_format,
        player::PlayerDisplaySurfaceOwnership::Borrowed,
    }};
    PlayerUiContext RuntimeMemoryProbeHostStorage::memory_ctx{};
    std::optional<PlayerRuntime> RuntimeMemoryProbeHostStorage::memory_runtime{};

    player::PlayerRuntimeConfig<player::PlayerPage> make_runtime_probe_config(const PreviewOptions& options) {
        RuntimeMemoryProbeHostStorage::memory_ctx = PlayerUiContext{};
        return player::PlayerRuntimeConfig<player::PlayerPage>{
            .app_config = make_app_config(options),
            .storage_config = player::default_storage_config(),
            .cover_resource_provider = make_preview_cover_resource_provider_binding(),
            .start_page = options.start_page,
            .initial_track_index = options.track_index_override,
            .auto_start = false,
            .clear_color = kUiBackground,
        };
    }

    bool runtime_probe_result_matches_external_surface(const player::PlayerRuntimeMemoryProbeResult& result,
                                                       const player::PlayerDisplaySurface& presented) noexcept {
        return result.ok
            && presented.pixels == RuntimeMemoryProbeHostStorage::external_buffer.data()
            && presented.width == screen_width
            && presented.height == screen_height
            && presented.stride_bytes == player::PlayerOwnedDisplayBuffer::stride_bytes
            && presented.pixel_format == player::PlayerOwnedDisplayBuffer::pixel_format
            && result.dirty.w == screen_width
            && result.dirty.h == screen_height;
    }

    player::PlayerRuntimeMemoryProbeResult run_memory_display_sink_probe(const PreviewOptions& options = PreviewOptions{}) {
        auto memory_cfg = make_runtime_probe_config(options);
        player::MemoryDisplaySinkState sink_state{};
        player::PlayerDisplaySink sink = player::make_memory_display_sink(sink_state);
        auto result = player::run_player_runtime_memory_probe(g_clock,
                                                              RuntimeMemoryProbeHostStorage::memory_platform,
                                                              RuntimeMemoryProbeHostStorage::memory_ctx,
                                                              std::move(memory_cfg),
                                                              RuntimeMemoryProbeHostStorage::memory_runtime,
                                                              sink_state,
                                                              sink,
                                                              g_clock.now_us());
        result.ok = runtime_probe_result_matches_external_surface(result, sink_state.last_surface);
        return result;
    }

    struct UiCiBoardDisplayCallbackState {
        int clean_cache_count{0};
        int flush_dirty_count{0};
        int present_count{0};
        player::PlayerDirtyRegion last_dirty{};
    };

    bool ui_ci_board_clean_cache(void* ctx,
                                 const player::PlayerDisplaySurface&,
                                 player::PlayerDirtyRegion dirty) noexcept {
        auto* state = static_cast<UiCiBoardDisplayCallbackState*>(ctx);
        if (!state || dirty.empty()) return false;
        state->clean_cache_count += 1;
        state->last_dirty = dirty;
        return true;
    }

    bool ui_ci_board_flush_dirty(void* ctx,
                                 const player::PlayerDisplaySurface&,
                                 player::PlayerDirtyRegion dirty) noexcept {
        auto* state = static_cast<UiCiBoardDisplayCallbackState*>(ctx);
        if (!state || dirty.empty()) return false;
        state->flush_dirty_count += 1;
        state->last_dirty = dirty;
        return true;
    }

    bool ui_ci_board_present(void* ctx,
                             const player::PlayerDisplaySurface&,
                             player::PlayerDirtyRegion dirty) noexcept {
        auto* state = static_cast<UiCiBoardDisplayCallbackState*>(ctx);
        if (!state || dirty.empty()) return false;
        state->present_count += 1;
        state->last_dirty = dirty;
        return true;
    }

    player::PlayerRuntimeMemoryProbeResult run_board_display_sink_probe(const PreviewOptions& options = PreviewOptions{}) {
        auto memory_cfg = make_runtime_probe_config(options);
        UiCiBoardDisplayCallbackState callback_state{};
        player::PlayerBoardDisplaySinkState sink_state{};
        player::PlayerDisplaySink sink = player::make_board_display_sink(
            sink_state,
            player::PlayerBoardDisplayCallbacks{
                .ctx = &callback_state,
                .clean_cache = &ui_ci_board_clean_cache,
                .flush_dirty = &ui_ci_board_flush_dirty,
                .present = &ui_ci_board_present,
            });
        auto result = player::run_player_runtime_memory_probe(g_clock,
                                                              RuntimeMemoryProbeHostStorage::memory_platform,
                                                              RuntimeMemoryProbeHostStorage::memory_ctx,
                                                              std::move(memory_cfg),
                                                              RuntimeMemoryProbeHostStorage::memory_runtime,
                                                              sink_state,
                                                              sink,
                                                              g_clock.now_us());
        result.ok = runtime_probe_result_matches_external_surface(result, sink_state.last_surface)
            && sink_state.last_clean_cache_ok
            && sink_state.last_flush_dirty_ok
            && sink_state.last_present_ok
            && callback_state.clean_cache_count == 1
            && callback_state.flush_dirty_count == 1
            && callback_state.present_count == 1
            && callback_state.last_dirty.w == screen_width
            && callback_state.last_dirty.h == screen_height;
        return result;
    }

    bool ui_ci_dispatch_touch_script(PlayerRuntime& runtime, UiCiTouchSampleScript& script) {
        player::PlayerTouchAdapterState state{};
        player::PlayerTouchSampleSource source{&script, &ui_ci_read_touch_sample};
        bool dispatched = false;
        while (script.index < script.count) {
            const auto batch = player::read_player_touch_events(source, state);
            if (batch.count == 0) {
                break;
            }
            for (std::size_t i = 0; i < batch.count; ++i) {
                runtime.dispatch_input(batch.events[i]);
                dispatched = true;
            }
        }
        return dispatched;
    }

    int run_runtime_memory_smoke(const PreviewOptions& options) noexcept {
        player::ui::set_player_system_font_fallback_enabled(!options.disable_system_font_fallback);
        const auto result = run_memory_display_sink_probe(options);
        const bool ok = result.ok;
        std::printf("[runtime-memory-smoke] done ok=%d failed=%d\n", ok ? 1 : 0, ok ? 0 : 1);
        return ok ? 0 : 2;
    }
