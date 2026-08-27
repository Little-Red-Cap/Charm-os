    constexpr std::uint32_t ui_ci_cover_pixel(std::uint8_t a,
                                              std::uint8_t r,
                                              std::uint8_t g,
                                              std::uint8_t b) noexcept {
        return (static_cast<std::uint32_t>(b) << 24)
            | (static_cast<std::uint32_t>(g) << 16)
            | (static_cast<std::uint32_t>(r) << 8)
            | static_cast<std::uint32_t>(a);
    }

    const std::array<std::uint32_t, 4>& ui_ci_static_cover_pixels() noexcept {
        static constexpr std::array<std::uint32_t, 4> kPixels{
            ui_ci_cover_pixel(255, 240, 120, 80),
            ui_ci_cover_pixel(255, 60, 110, 210),
            ui_ci_cover_pixel(255, 96, 180, 130),
            ui_ci_cover_pixel(255, 250, 220, 120),
        };
        return kPixels;
    }

    player::PlayerCoverResourceRecordTableView ui_ci_static_cover_record_table() noexcept {
        static const auto& pixels = ui_ci_static_cover_pixels();
        static const std::array<player::PlayerCoverResourceRecord, 1> kRecords{{
            player::PlayerCoverResourceRecord{
                .path = "/ui-ci/static-cover",
                .kind = player::CoverResourceKind::EmbeddedTrack,
                .key = "/ui-ci/static-cover-resource",
                .argb = std::span<const std::uint32_t>(pixels.data(), pixels.size()),
                .width = 2,
                .height = 2,
            },
        }};
        return player::PlayerCoverResourceRecordTableView{
            .records = std::span<const player::PlayerCoverResourceRecord>(kRecords.data(), kRecords.size()),
        };
    }

    bool ui_ci_static_cover_provider(void*,
                                     const player::CoverResourceRequest& request,
                                     player::CoverResourceView& out) noexcept {
        const auto& pixels = ui_ci_static_cover_pixels();
        if (request.kind != player::CoverResourceKind::EmbeddedTrack) return false;
        if (request.path != std::string_view{"/ui-ci/static-cover"}) return false;
        out.key = "/ui-ci/static-cover-resource";
        out.argb = std::span<const std::uint32_t>(pixels.data(), pixels.size());
        out.width = 2;
        out.height = 2;
        return true;
    }

    bool run_cover_resource_provider_ci() noexcept {
        const auto previous = player::cover_resource_provider_binding();
        const auto table = ui_ci_static_cover_record_table();
        player::set_cover_resource_provider_binding(player::make_cover_resource_record_table_binding(table));

        player::ResolvedCover image{};
        player::CoverResourceRequest request{};
        request.path = "/ui-ci/static-cover";
        request.kind = player::CoverResourceKind::EmbeddedTrack;
        request.fallback_variant = player::DefaultCoverVariant::HomeHeroPill;
        const bool loaded = player::resolve_cover(request, image);
        const bool ok = loaded
            && ::ui::gfx::image_id_valid(image.image_id)
            && image.width == 2
            && image.height == 2
            && image.key.view() == "/ui-ci/static-cover-resource";

        player::release_resolved_cover(image);
        player::set_cover_resource_provider_binding(previous);
        return ok;
    }

    bool run_cover_resource_record_table_ci() noexcept {
        const auto table = ui_ci_static_cover_record_table();
        player::CoverResourceRequest request{};
        request.path = "/ui-ci/static-cover";
        request.kind = player::CoverResourceKind::EmbeddedTrack;
        request.fallback_variant = player::DefaultCoverVariant::HomeHeroPill;

        player::CoverResourceView out{};
        const bool ok = player::resolve_cover_resource_record_table(table, request, out)
            && out.key == "/ui-ci/static-cover-resource"
            && out.width == 2
            && out.height == 2
            && out.argb.size() == 4;
        return ok;
    }

    template<typename RuntimeT, typename PumpFrameFn>
    bool run_cover_resource_provider_runtime_binding_ci(RuntimeT& runtime,
                                                        PumpFrameFn& pump_frame) noexcept {
        auto* controller = runtime.controller();
        if (!controller) {
            return false;
        }

        const auto previous_binding = controller->cover_resource_binding;
        controller->cover_resource_binding = player::PlayerCoverResourceProviderBinding{
            .ctx = nullptr,
            .resolve_fn = &ui_ci_static_cover_provider,
        };

        controller->cover_strategy = player::CoverStrategy::embedded_only;
        controller->cover_ready = true;
        controller->cover_path.clear();
        controller->cover_embedded_path.assign("/ui-ci/static-cover");
        controller->cover_folder_path.clear();
        controller->cover_failed_embedded_path.clear();
        controller->cover_failed_folder_path.clear();
        controller->update_cover_image();
        pump_frame();

        const bool provider_used = ::ui::gfx::image_id_valid(controller->current_cover.image_id)
            && controller->current_cover.width == 2
            && controller->current_cover.height == 2
            && controller->current_cover.key.view() == "/ui-ci/static-cover-resource";

        controller->cover_resource_binding = previous_binding;
        const auto active_binding = controller->cover_resource_binding;
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

    bool run_font_resource_runtime_apply_ci(PlayerRuntime& runtime) noexcept {
        auto* controller = runtime.controller();
        if (!controller) {
            return false;
        }
        if constexpr (player::host_features::host_file_fonts) {
            const bool expects_runtime_bind = controller->fs_ready;
            const bool ok = !controller->font_ttf_path.empty()
                && controller->font_small_px == player::product_config::default_font_small_px
                && controller->font_normal_px == player::product_config::default_font_normal_px
                && controller->font_large_px == player::product_config::default_font_large_px
                && controller->font_retry_done == expects_runtime_bind
                && player::ui::font_package_bound() == expects_runtime_bind
                && font_provider_bound() == expects_runtime_bind;
            if (!ok) {
                std::printf("[ui-ci] font-runtime fs_ready=%d retry=%d package=%d provider=%d path_empty=%d px=%d/%d/%d expected=%d\n",
                            controller->fs_ready ? 1 : 0,
                            controller->font_retry_done ? 1 : 0,
                            player::ui::font_package_bound() ? 1 : 0,
                            font_provider_bound() ? 1 : 0,
                            controller->font_ttf_path.empty() ? 1 : 0,
                            controller->font_small_px,
                            controller->font_normal_px,
                            controller->font_large_px,
                            expects_runtime_bind ? 1 : 0);
            }
            return ok;
        } else {
            return controller->font_ttf_path.empty()
                && controller->font_fallback_ttf_path.empty()
                && controller->font_small_px == player::product_config::default_font_small_px
                && controller->font_normal_px == player::product_config::default_font_normal_px
                && controller->font_large_px == player::product_config::default_font_large_px
                && !player::ui::font_package_bound()
                && !font_provider_bound();
        }
    }

    bool run_media_library_path_derivation_artist_title_ci() noexcept {
        const auto view = player::derive_media_track("/music/Album One/Kalafina - Oblivious.flac");
        return view.path == "/music/Album One/Kalafina - Oblivious.flac"
            && view.file_name == "Kalafina - Oblivious.flac"
            && view.stem == "Kalafina - Oblivious"
            && view.title == "Oblivious"
            && view.artist == "Kalafina"
            && view.album == "Album One"
            && view.format == "FLAC"
            && view.has_artist;
    }

    bool run_media_library_path_derivation_unknown_artist_ci() noexcept {
        const auto view = player::derive_media_track("/music/Loose/Track 01.mp3");
        return view.title == "Track 01"
            && view.artist == player::kUnknownArtist
            && view.album == "Loose"
            && view.format == "MP3"
            && !view.has_artist;
    }

    bool run_media_library_path_derivation_compact_dash_ci() noexcept {
        const auto view = player::derive_media_track("/music/Loose/AC-DC.flac");
        return view.title == "AC-DC"
            && view.artist == player::kUnknownArtist
            && view.format == "FLAC"
            && !view.has_artist;
    }

    bool run_media_library_group_key_album_artist_ci() noexcept {
        const auto view = player::derive_media_track("/music/Ninth Wave/Artist Name - Track.wav");
        return player::media_group_key(view, player::MediaGroupKind::Albums) == "Ninth Wave"
            && player::media_group_key(view, player::MediaGroupKind::Artists) == "Artist Name";
    }

    bool run_media_library_sort_compare_ci() noexcept {
        return player::compare_media_text_ci("alpha", "Beta") < 0
            && player::compare_media_text_ci("Track", "track") == 0
            && player::compare_media_text_ci("Zoo", "album") > 0;
    }

    void run_media_library_helper_regressions(UiCiResult& res) noexcept {
        const bool artist_title = run_media_library_path_derivation_artist_title_ci();
        ui_ci_emit("media_library_path_derivation_artist_title",
                   artist_title,
                   artist_title ? nullptr : "artist_title");
        if (!artist_title) {
            res.ok = false;
            res.failed++;
        }

        const bool unknown_artist = run_media_library_path_derivation_unknown_artist_ci();
        ui_ci_emit("media_library_path_derivation_unknown_artist",
                   unknown_artist,
                   unknown_artist ? nullptr : "unknown_artist");
        if (!unknown_artist) {
            res.ok = false;
            res.failed++;
        }

        const bool compact_dash = run_media_library_path_derivation_compact_dash_ci();
        ui_ci_emit("media_library_path_derivation_compact_dash",
                   compact_dash,
                   compact_dash ? nullptr : "compact_dash");
        if (!compact_dash) {
            res.ok = false;
            res.failed++;
        }

        const bool group_key = run_media_library_group_key_album_artist_ci();
        ui_ci_emit("media_library_group_key_album_artist",
                   group_key,
                   group_key ? nullptr : "group_key");
        if (!group_key) {
            res.ok = false;
            res.failed++;
        }

        const bool sort_compare = run_media_library_sort_compare_ci();
        ui_ci_emit("media_library_sort_compare_ci",
                   sort_compare,
                   sort_compare ? nullptr : "sort_compare");
        if (!sort_compare) {
            res.ok = false;
            res.failed++;
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
        if (run_font_resource_config_ci()) {
            ui_ci_emit("font_resource_config_profile_mode", true, nullptr);
        } else {
            ui_ci_emit("font_resource_config_profile_mode", false, "font_resource_config");
            res.ok = false;
            res.failed++;
        }

        if (run_font_resource_runtime_apply_ci(runtime)) {
            ui_ci_emit("font_resource_runtime_apply_mode", true, nullptr);
        } else {
            ui_ci_emit("font_resource_runtime_apply_mode", false, "font_resource_runtime");
            res.ok = false;
            res.failed++;
        }

        run_media_library_helper_regressions(res);

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

        if (run_cover_resource_record_table_ci()) {
            ui_ci_emit("cover_resource_record_table_static_view", true, nullptr);
        } else {
            ui_ci_emit("cover_resource_record_table_static_view", false, "record_table");
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
