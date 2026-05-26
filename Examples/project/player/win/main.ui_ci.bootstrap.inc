    constexpr std::uint32_t ui_ci_cover_pixel(std::uint8_t a,
                                              std::uint8_t r,
                                              std::uint8_t g,
                                              std::uint8_t b) noexcept {
        return (static_cast<std::uint32_t>(b) << 24)
            | (static_cast<std::uint32_t>(g) << 16)
            | (static_cast<std::uint32_t>(r) << 8)
            | static_cast<std::uint32_t>(a);
    }

    bool ui_ci_static_cover_provider(void*,
                                     const player::CoverResourceRequest& request,
                                     player::CoverResourceView& out) noexcept {
        static constexpr std::array<std::uint32_t, 4> kPixels{
            ui_ci_cover_pixel(255, 240, 120, 80),
            ui_ci_cover_pixel(255, 60, 110, 210),
            ui_ci_cover_pixel(255, 96, 180, 130),
            ui_ci_cover_pixel(255, 250, 220, 120),
        };
        if (request.kind != player::CoverResourceKind::EmbeddedTrack) return false;
        if (request.path != std::string_view{"/ui-ci/static-cover"}) return false;
        out.key = "/ui-ci/static-cover-resource";
        out.argb = std::span<const std::uint32_t>(kPixels.data(), kPixels.size());
        out.width = 2;
        out.height = 2;
        return true;
    }

    bool run_cover_resource_provider_ci() noexcept {
        const auto previous = player::cover_resource_provider_binding();
        player::set_cover_resource_provider_binding(player::PlayerCoverResourceProviderBinding{
            .ctx = nullptr,
            .resolve_fn = &ui_ci_static_cover_provider,
        });

        player::CoverImage image{};
        player::CoverResourceRequest request{};
        request.path = "/ui-ci/static-cover";
        request.kind = player::CoverResourceKind::EmbeddedTrack;
        request.fallback_variant = player::DefaultCoverVariant::HomeHeroPill;
        const bool loaded = player::load_cover_image(request, image);
        const bool ok = loaded
            && ::ui::gfx::image_id_valid(image.image_id)
            && image.width == 2
            && image.height == 2
            && image.argb.size() == 4
            && image.path == "/ui-ci/static-cover-resource";

        player::release_cover_image(image);
        player::set_cover_resource_provider_binding(previous);
        return ok;
    }

    template<typename RuntimeT, typename PumpFrameFn>
    bool run_cover_resource_provider_runtime_binding_ci(RuntimeT& runtime,
                                                        PumpFrameFn& pump_frame) noexcept {
        auto* controller = runtime.controller();
        if (!controller) {
            return false;
        }

        const auto previous_binding = player::cover_resource_provider_binding();
        player::set_cover_resource_provider_binding(player::PlayerCoverResourceProviderBinding{
            .ctx = nullptr,
            .resolve_fn = &ui_ci_static_cover_provider,
        });

        controller->cover_strategy = player::CoverStrategy::embedded_only;
        controller->cover_ready = true;
        controller->cover_path.clear();
        controller->cover_embedded_path.assign("/ui-ci/static-cover");
        controller->cover_folder_path.clear();
        controller->cover_failed_embedded_path.clear();
        controller->cover_failed_folder_path.clear();
        controller->update_cover_image();
        pump_frame();

        const bool provider_used = ::ui::gfx::image_id_valid(controller->cover_image.image_id)
            && controller->cover_image.width == 2
            && controller->cover_image.height == 2
            && controller->cover_image.argb.size() == 4
            && controller->cover_image.path == "/ui-ci/static-cover-resource";

        player::set_cover_resource_provider_binding(previous_binding);
        const auto active_binding = player::cover_resource_provider_binding();
        const bool restored = active_binding.ctx == previous_binding.ctx
            && active_binding.resolve_fn == previous_binding.resolve_fn;
        return provider_used && restored;
    }

    bool run_font_resource_config_ci() noexcept {
        const PreviewOptions options{};
        const auto app_cfg = make_app_config(options);
        const auto& font = app_cfg.font_resources;
        if constexpr (player::host_features::host_file_fonts) {
            return font.kind == player::FontResourceKind::FilePath
                && font.has_file_resource()
                && !font.primary_path.empty()
                && font.small_px == player::product_config::default_font_small_px
                && font.normal_px == player::product_config::default_font_normal_px
                && font.large_px == player::product_config::default_font_large_px;
        } else {
            return font.kind == player::FontResourceKind::Builtin
                && font.is_builtin()
                && !font.has_file_resource()
                && font.primary_path.empty()
                && font.small_px == player::product_config::default_font_small_px
                && font.normal_px == player::product_config::default_font_normal_px
                && font.large_px == player::product_config::default_font_large_px;
        }
    }

    template<typename RuntimeT,
             typename SceneT,
             typename ContextT,
             typename WaitForPageFn,
             typename SettleNowPlayingTransitionFn,
             typename PumpFrameFn>
    void run_ui_ci_bootstrap_regressions(UiCiResult& res,
                                         RuntimeT& runtime,
                                         SceneT& scene,
                                         ContextT& ctx,
                                         WaitForPageFn& wait_for_page,
                                         SettleNowPlayingTransitionFn& settle_now_playing_transition,
                                         PumpFrameFn& pump_frame) {
        if (run_memory_display_sink_probe().ok) {
            ui_ci_emit("memory_display_sink_player_ui_external_surface", true, nullptr);
        } else {
            ui_ci_emit("memory_display_sink_player_ui_external_surface", false, "memory_display_sink");
            res.ok = false;
            res.failed++;
        }
        if (run_board_display_sink_probe().ok) {
            ui_ci_emit("board_display_sink_player_ui_external_surface", true, nullptr);
        } else {
            ui_ci_emit("board_display_sink_player_ui_external_surface", false, "board_display_sink");
            res.ok = false;
            res.failed++;
        }
        if (run_board_port_bindings_ci()) {
            ui_ci_emit("board_port_bindings_framebuffer_touch_font", true, nullptr);
        } else {
            ui_ci_emit("board_port_bindings_framebuffer_touch_font", false, "board_port_bindings");
            res.ok = false;
            res.failed++;
        }
        pump_frame();

        if (run_cover_resource_provider_ci()) {
            ui_ci_emit("cover_resource_provider_static_view", true, nullptr);
        } else {
            ui_ci_emit("cover_resource_provider_static_view", false, "static_view");
            res.ok = false;
            res.failed++;
        }

        if (run_cover_resource_provider_runtime_binding_ci(runtime, pump_frame)) {
            ui_ci_emit("cover_resource_provider_runtime_binding", true, nullptr);
        } else {
            ui_ci_emit("cover_resource_provider_runtime_binding", false, "runtime_binding");
            res.ok = false;
            res.failed++;
        }

        if (run_font_resource_config_ci()) {
            ui_ci_emit("font_resource_config_profile_mode", true, nullptr);
        } else {
            ui_ci_emit("font_resource_config_profile_mode", false, "font_resource_config");
            res.ok = false;
            res.failed++;
        }

        {
            ctx.set_page(player::PlayerPage::Home);
            pump_frame();
            const Rect r = scene.world_rect(ctx.handles.bottom_hit);
            if (r.w > 0 && r.h > 0) {
                const int cx = r.x + r.w / 2;
                const int cy = r.y + r.h / 2;
                UiCiTouchSampleScript script{
                    .samples = {
                        player::PlayerTouchSample{true, static_cast<float>(cx), static_cast<float>(cy), 0, 10},
                        player::PlayerTouchSample{true, static_cast<float>(cx + 3), static_cast<float>(cy + 2), 0, 11},
                        player::PlayerTouchSample{false, static_cast<float>(cx + 3), static_cast<float>(cy + 2), 0, 12},
                        player::PlayerTouchSample{},
                    },
                    .count = 3,
                    .index = 0,
                };
                if (ui_ci_dispatch_touch_script(runtime, script)
                    && (wait_for_page(player::PlayerPage::NowPlaying)
                        || settle_now_playing_transition(player::PlayerPage::NowPlaying))) {
                    ui_ci_emit("board_touch_source_home_to_now", true, nullptr);
                } else {
                    ui_ci_emit("board_touch_source_home_to_now", false, "page_not_now");
                    res.ok = false;
                    res.failed++;
                }
            } else {
                ui_ci_emit("board_touch_source_home_to_now", false, "bottom_hit_zero");
                res.ok = false;
                res.failed++;
            }
            ctx.set_page(player::PlayerPage::Home);
            pump_frame();
        }

        {
            ctx.set_page(player::PlayerPage::Home);
            pump_frame();
            const Rect r = scene.world_rect(ctx.handles.home_scroll);
            const int before_scroll = ctx.access.scroll_y(ctx.handles.home_scroll);
            if (r.w > 0 && r.h > 0) {
                runtime.dispatch_input(player::PlayerInputEvent::make_wheel(
                    20,
                    static_cast<float>(r.x + r.w / 2),
                    static_cast<float>(r.y + r.h / 2),
                    -3.0f));
                pump_frame();
                const int after_scroll = ctx.access.scroll_y(ctx.handles.home_scroll);
                if (after_scroll != before_scroll) {
                    ui_ci_emit("input_boundary_wheel_home_scroll", true, nullptr);
                } else {
                    ui_ci_emit("input_boundary_wheel_home_scroll", false, "scroll_unchanged");
                    res.ok = false;
                    res.failed++;
                }
            } else {
                ui_ci_emit("input_boundary_wheel_home_scroll", false, "home_scroll_zero");
                res.ok = false;
                res.failed++;
            }
        }
    }
