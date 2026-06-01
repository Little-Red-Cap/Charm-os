    constexpr std::size_t kUiCmdBudget = 1200;
    constexpr std::uint64_t kUiAlphaBlendBudget = 1000000;
    constexpr std::uint32_t kUiLayerBytesBudget = 0;

    const std::array<std::uint32_t, 4>& ui_ci_static_cover_pixels() noexcept;
    player::PlayerCoverResourceRecordTableView ui_ci_static_cover_record_table() noexcept;
    bool ui_ci_static_cover_provider(void*,
                                     const player::CoverResourceRequest& request,
                                     player::CoverResourceView& out) noexcept;

    static FILE* g_ui_ci_perf_log{nullptr};
    static player::font_cache::Stats g_ui_ci_last_font_cache_stats{};
    static bool g_ui_ci_has_last_font_cache_stats{false};

    void ui_ci_perf_printf(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        va_list copy;
        va_copy(copy, args);
        std::vprintf(fmt, args);
        if (g_ui_ci_perf_log) {
            std::vfprintf(g_ui_ci_perf_log, fmt, copy);
            std::fflush(g_ui_ci_perf_log);
        }
        va_end(copy);
        va_end(args);
    }

    bool ui_ci_open_perf_profile_log() {
        std::error_code ec;
        const std::filesystem::path dir =
            std::filesystem::current_path(ec) / "generated" / "perf";
        if (ec) {
            std::fprintf(stderr, "[ui-ci.perf_profile] current_path failed: %s\n", ec.message().c_str());
            return false;
        }
        std::filesystem::create_directories(dir, ec);
        if (ec) {
            std::fprintf(stderr, "[ui-ci.perf_profile] create_directories failed: %s\n", ec.message().c_str());
            return false;
        }
        const auto path = dir / "player_md3_ui_perf_profile.log";
        g_ui_ci_perf_log = std::fopen(path.string().c_str(), "wb");
        if (!g_ui_ci_perf_log) {
            std::fprintf(stderr, "[ui-ci.perf_profile] open log failed: %s\n", path.string().c_str());
            return false;
        }
        std::printf("[ui-ci.perf_profile] log=%s\n", path.string().c_str());
        std::fprintf(g_ui_ci_perf_log, "[ui-ci.perf_profile] log=%s\n", path.string().c_str());
        std::fflush(g_ui_ci_perf_log);
        return true;
    }

    void ui_ci_close_perf_profile_log() noexcept {
        if (g_ui_ci_perf_log) {
            std::fclose(g_ui_ci_perf_log);
            g_ui_ci_perf_log = nullptr;
        }
    }

    struct UiCiPerfProfileLogScope {
        bool opened{false};

        explicit UiCiPerfProfileLogScope(bool enable) {
            if (enable) {
                opened = ui_ci_open_perf_profile_log();
            }
        }

        ~UiCiPerfProfileLogScope() {
            ui_ci_close_perf_profile_log();
        }
    };

    #include "main.ui_ci.portability.inc"

    #include "main.ui_ci.layer_snapshot.inc"

    #include "main.ui_ci.page_flow.inc"

    #include "main.ui_ci.library.inc"

    #include "main.ui_ci.now_playing_matrix.inc"

    #include "main.ui_ci.bootstrap.inc"

    template<typename SceneT>
    void ui_ci_emit_perf_evidence(const char* page, const SceneT& scene) {
        const auto evidence = ::ui::scene::make_scene_evidence(scene);
        ui_ci_perf_printf(
            "[ui-ci.perf] page=%s cmd=%u/%u bytes=%u text=%u/%u blob=%u/%u "
            "batch=%u/%u/%u exec=%u/%u fail=%u alpha=%u "
            "groups=%u/%u/%u/%u flush=%u clip_fail=%u kinds=%u/%u/%u/%u/%u/%u fail_detail=%u/%u/%u "
            "layer=%u/%u/%u/%u/%u/%u/%u\n",
            page ? page : "unknown",
            evidence.cmd.cmd_count,
            evidence.cmd.cmd_capacity,
            evidence.cmd.cmd_bytes,
            evidence.cmd.text_used,
            evidence.cmd.text_capacity,
            evidence.cmd.blob_used,
            evidence.cmd.blob_capacity,
            evidence.cmd.batch_shrink,
            evidence.cmd.batch_shrink_rect + evidence.cmd.batch_shrink_round + evidence.cmd.batch_shrink_focus,
            evidence.cmd.batch_shrink_line + evidence.cmd.batch_shrink_path + evidence.cmd.batch_shrink_image,
            evidence.exec.cmd_count,
            evidence.exec.cmd_bytes,
            evidence.exec.failed_cmds,
            evidence.exec.alpha_blend_count,
            evidence.exec.group_rect,
            evidence.exec.group_text,
            evidence.exec.group_image,
            evidence.exec.group_line + evidence.exec.group_path + evidence.exec.group_other,
            evidence.exec.batch_flushes,
            evidence.exec.clip_push_overflow
                + evidence.exec.clip_pop_underflow
                + evidence.exec.clip_invalid
                + evidence.exec.fail_clip,
            evidence.exec.cmd_rect,
            evidence.exec.cmd_text,
            evidence.exec.cmd_image,
            evidence.exec.cmd_line,
            evidence.exec.cmd_path,
            evidence.exec.cmd_other,
            evidence.exec.fail_text,
            evidence.exec.fail_image,
            evidence.exec.fail_blob + evidence.exec.fail_path + evidence.exec.fail_clip + evidence.exec.fail_other,
            evidence.layer.snapshot_count,
            evidence.layer.snapshot_rebuild_count,
            evidence.layer.stale_snapshot_count,
            evidence.layer.layer_bytes,
            evidence.layer.composite_pixels,
            evidence.layer.pixel_blit_count,
            evidence.layer.pixel_blit_pixels);
    }

    std::uint32_t ui_ci_ratio_x1000(std::uint32_t numerator, std::uint32_t denominator) noexcept {
        if (denominator == 0) return 0U;
        const std::uint64_t scaled = static_cast<std::uint64_t>(numerator) * 1000ULL;
        return static_cast<std::uint32_t>((scaled + denominator / 2U) / denominator);
    }

    void ui_ci_emit_perf_profile_evidence(const char* page,
                                          const ::ui::scene::SceneEvidence& evidence) {
        constexpr std::uint32_t screen_pixels =
            static_cast<std::uint32_t>(screen_width * screen_height);
        const std::uint32_t cmd_count = evidence.cmd.cmd_count;
        const std::uint32_t exec_count = evidence.exec.cmd_count;
        const std::uint32_t alpha_screen_x1000 =
            ui_ci_ratio_x1000(evidence.exec.alpha_blend_count, screen_pixels);
        const std::uint32_t flush_density_x1000 =
            ui_ci_ratio_x1000(evidence.exec.batch_flushes, cmd_count);
        const std::uint32_t exec_density_x1000 =
            ui_ci_ratio_x1000(exec_count, cmd_count);
        const std::uint32_t cmd_other_total =
            evidence.exec.cmd_line + evidence.exec.cmd_path + evidence.exec.cmd_other;
        const std::uint32_t group_other_total =
            evidence.exec.group_line + evidence.exec.group_path + evidence.exec.group_other;
        const std::uint32_t clip_fail =
            evidence.exec.clip_push_overflow
            + evidence.exec.clip_pop_underflow
            + evidence.exec.clip_invalid
            + evidence.exec.fail_clip;
        const std::uint32_t fail_other_total =
            evidence.exec.fail_blob
            + evidence.exec.fail_path
            + evidence.exec.fail_clip
            + evidence.exec.fail_other;

        ui_ci_perf_printf(
            "[ui-ci.perf_profile] page=%s alpha_screen_x1000=%u cmd_mix=%u/%u/%u/%u "
            "group_mix=%u/%u/%u/%u flush_density_x1000=%u exec_density_x1000=%u "
            "text_bytes=%u/%u blob_bytes=%u/%u fail_count=%u clip_fail=%u fail_mix=%u/%u/%u\n",
            page ? page : "unknown",
            alpha_screen_x1000,
            evidence.exec.cmd_rect,
            evidence.exec.cmd_text,
            evidence.exec.cmd_image,
            cmd_other_total,
            evidence.exec.group_rect,
            evidence.exec.group_text,
            evidence.exec.group_image,
            group_other_total,
            flush_density_x1000,
            exec_density_x1000,
            evidence.cmd.text_used,
            evidence.cmd.text_capacity,
            evidence.cmd.blob_used,
            evidence.cmd.blob_capacity,
            evidence.exec.failed_cmds,
            clip_fail,
            evidence.exec.fail_text,
            evidence.exec.fail_image,
            fail_other_total);
    }

    const char* ui_ci_draw_scope_name(std::uint16_t id) noexcept {
        switch (id) {
        case 0: return "default";
        case 1: return "home.hero";
        case 2: return "home.cards";
        case 3: return "home.collage";
        case 4: return "now.cover";
        case 5: return "now.text";
        case 6: return "now.progress";
        case 7: return "now.controls";
        case 8: return "now.transition";
        case 9: return "library.chrome";
        case 10: return "library.list";
        case 11: return "library.overlay";
        case 12: return "bottom_bar";
        default: return "unknown";
        }
    }

    template<typename EvidenceT, typename = void>
    struct UiCiHasDrawDetail : std::false_type {};

    template<typename EvidenceT>
    struct UiCiHasDrawDetail<EvidenceT, std::void_t<decltype(std::declval<const EvidenceT&>().draw_detail)>>
        : std::true_type {};

    void ui_ci_emit_perf_detail_evidence(const char* page,
                                         const ::ui::scene::SceneEvidence& evidence) {
        if constexpr (UiCiHasDrawDetail<::ui::scene::SceneEvidence>::value) {
            for (std::size_t i = 0; i < evidence.draw_detail.types.size(); ++i) {
                const auto& type = evidence.draw_detail.types[i];
                if (type.count == 0U) continue;
                ui_ci_perf_printf(
                    "[ui-ci.perf_cmd_detail] page=%s type=%s count=%u area=%u alpha=%u max_area=%u\n",
                    page ? page : "unknown",
                    ::ui::draw_cmd::cmd_type_name(static_cast<::ui::draw_cmd::CmdType>(i)),
                    type.count,
                    type.rect_area,
                    type.actual_alpha_pixels,
                    type.max_rect_area);
            }
            for (const auto& scope : evidence.draw_detail.scopes) {
                if (scope.active == 0U || scope.cmd_count == 0U) continue;
                ui_ci_perf_printf(
                    "[ui-ci.perf_scope_detail] page=%s scope=%s id=%u cmd=%u area=%u alpha=%u\n",
                    page ? page : "unknown",
                    ui_ci_draw_scope_name(scope.scope_id),
                    scope.scope_id,
                    scope.cmd_count,
                    scope.rect_area,
                    scope.actual_alpha_pixels);
            }
            if (evidence.draw_detail.scope_overflow != 0U) {
                ui_ci_perf_printf(
                    "[ui-ci.perf_scope_detail] page=%s scope_overflow=%u\n",
                    page ? page : "unknown",
                    evidence.draw_detail.scope_overflow);
            }
        }
    }

    struct UiCiFrameTiming {
        std::uint32_t available{0};
        std::uint32_t frame_us{0};
        std::uint32_t tick_us{0};
        std::uint32_t render_us{0};
        std::uint32_t record_us{0};
        std::uint32_t execute_us{0};
        std::uint32_t present_us{0};
    };

    struct UiCiTimedDisplaySink {
        player::PlayerDisplaySink* inner{nullptr};
        std::uint32_t present_us{0};
    };

    std::uint32_t ui_ci_delta_us(charm::system::ClockTick start,
                                 charm::system::ClockTick end) noexcept {
        return end >= start
            ? ::ui::scene::clamp_scene_timing_us(static_cast<std::uint64_t>(end - start))
            : 0U;
    }

    bool ui_ci_timed_present(void* ctx,
                             const player::PlayerDisplaySurface& surface,
                             player::PlayerDirtyRegion dirty) noexcept {
        auto* timed = static_cast<UiCiTimedDisplaySink*>(ctx);
        const auto start = g_clock.now_us();
        const bool ok = !timed || !timed->inner || timed->inner->present(surface, dirty);
        const auto end = g_clock.now_us();
        if (timed) {
            timed->present_us = ui_ci_delta_us(start, end);
        }
        return ok;
    }

    UiCiFrameTiming ui_ci_pump_frame_timed(PlayerRuntime& runtime,
                                           player::PlayerDisplaySink* display_sink) {
        UiCiFrameTiming out{};
        out.available = 1U;

        UiCiTimedDisplaySink timed_sink_state{display_sink, 0U};
        player::PlayerDisplaySink timed_sink{&timed_sink_state, &ui_ci_timed_present};
        auto* sink = display_sink ? &timed_sink : nullptr;

        const auto frame_start = g_clock.now_us();
        const auto tick_start = frame_start;
        runtime.tick(g_clock.now_us());
        const auto tick_end = g_clock.now_us();
        (void)runtime.render(sink);
        const auto frame_end = g_clock.now_us();

        const auto scene_timing = runtime.scene_ref().last_render_timing();
        out.frame_us = ui_ci_delta_us(frame_start, frame_end);
        out.tick_us = ui_ci_delta_us(tick_start, tick_end);
        out.render_us = ui_ci_delta_us(tick_end, frame_end);
        out.record_us = scene_timing.record_us;
        out.execute_us = scene_timing.execute_us;
        out.present_us = timed_sink_state.present_us;
        return out;
    }

    void ui_ci_emit_perf_time_evidence(const char* page,
                                       const UiCiFrameTiming& timing,
                                       const ::ui::scene::SceneEvidence& scene_evidence) {
        ui_ci_perf_printf(
            "[ui-ci.perf_time] page=%s available=%u frame=%u tick=%u render=%u "
            "record=%u execute=%u present=%u scene_available=%u\n",
            page ? page : "unknown",
            timing.available,
            timing.frame_us,
            timing.tick_us,
            timing.render_us,
            timing.record_us,
            timing.execute_us,
            timing.present_us,
            scene_evidence.timing.available);
    }

    void ui_ci_emit_record_profile_evidence(const char* page,
                                            const UiCiFrameTiming& timing,
                                            const ::ui::scene::SceneEvidence& evidence) {
        const bool font_cache_ready = player::font_cache::ready();
        const player::font_cache::Stats font_stats =
            font_cache_ready ? player::font_cache::stats()
                             : player::font_cache::Stats{};
        const player::font_cache::Stats font_delta =
            g_ui_ci_has_last_font_cache_stats
                ? player::font_cache::Stats{
                    font_stats.requests - g_ui_ci_last_font_cache_stats.requests,
                    font_stats.cached - g_ui_ci_last_font_cache_stats.cached,
                    font_stats.missing - g_ui_ci_last_font_cache_stats.missing,
                }
                : font_stats;
        g_ui_ci_last_font_cache_stats = font_stats;
        g_ui_ci_has_last_font_cache_stats = true;

        std::uint32_t draw_text_box_count = 0U;
        std::uint32_t draw_text_box_area = 0U;
        std::uint32_t glyph_run_count = 0U;
        std::uint32_t glyph_run_area = 0U;
        if constexpr (UiCiHasDrawDetail<::ui::scene::SceneEvidence>::value) {
            const auto text_type =
                static_cast<std::size_t>(::ui::draw_cmd::CmdType::DrawTextBox);
            const auto glyph_type =
                static_cast<std::size_t>(::ui::draw_cmd::CmdType::GlyphRun);
            if (text_type < evidence.draw_detail.types.size()) {
                draw_text_box_count = evidence.draw_detail.types[text_type].count;
                draw_text_box_area = evidence.draw_detail.types[text_type].rect_area;
            }
            if (glyph_type < evidence.draw_detail.types.size()) {
                glyph_run_count = evidence.draw_detail.types[glyph_type].count;
                glyph_run_area = evidence.draw_detail.types[glyph_type].rect_area;
            }
        }
        ui_ci_perf_printf(
            "[ui-ci.record_profile] page=%s available=%u record_us=%u "
            "cmd=%u text_bytes=%u/%u blob_bytes=%u/%u "
            "cmd_text=%u group_text=%u draw_text_box=%u/%u glyph_run=%u/%u "
            "text_overflow=%u blob_overflow=%u "
            "font_cache_ready=%u font_cache=%u/%u/%u font_cache_delta=%u/%u/%u\n",
            page ? page : "unknown",
            timing.available,
            timing.record_us,
            evidence.cmd.cmd_count,
            evidence.cmd.text_used,
            evidence.cmd.text_capacity,
            evidence.cmd.blob_used,
            evidence.cmd.blob_capacity,
            evidence.exec.cmd_text,
            evidence.exec.group_text,
            draw_text_box_count,
            draw_text_box_area,
            glyph_run_count,
            glyph_run_area,
            evidence.cmd.text_overflowed,
            evidence.cmd.blob_overflowed,
            font_cache_ready ? 1U : 0U,
            font_stats.requests,
            font_stats.cached,
            font_stats.missing,
            font_delta.requests,
            font_delta.cached,
            font_delta.missing);
    }

    template<typename SceneT>
    void ui_ci_emit_perf_evidence_bundle(const char* page,
                                         const SceneT& scene,
                                         const UiCiFrameTiming& timing) {
        const auto evidence = ::ui::scene::make_scene_evidence(scene);
        ui_ci_emit_perf_evidence(page, scene);
        ui_ci_emit_perf_time_evidence(page, timing, evidence);
        ui_ci_emit_perf_profile_evidence(page, evidence);
        ui_ci_emit_record_profile_evidence(page, timing, evidence);
        ui_ci_emit_perf_detail_evidence(page, evidence);
    }

    const char* ui_ci_page_name(player::PlayerPage page) noexcept {
        switch (page) {
        case player::PlayerPage::Probe:
            return "probe";
        case player::PlayerPage::Home:
            return "home";
        case player::PlayerPage::NowPlaying:
            return "now";
        case player::PlayerPage::Library:
            return "library";
        }
        return "unknown";
    }

    struct UiCiTransitionPerfSummary {
        std::uint32_t frames{0};
        std::uint32_t completed{0};
        std::uint64_t total_frame_us{0};
        std::uint64_t total_tick_us{0};
        std::uint64_t total_render_us{0};
        std::uint64_t total_record_us{0};
        std::uint64_t total_execute_us{0};
        std::uint64_t total_present_us{0};
        std::uint64_t total_cmds{0};
        std::uint64_t total_alpha{0};
        std::uint32_t max_frame_us{0};
        std::uint32_t max_execute_us{0};
        std::uint32_t max_present_us{0};
        std::uint32_t max_cmds{0};
        std::uint32_t max_alpha{0};
        std::uint32_t max_alpha_screen_x1000{0};
    };

    void ui_ci_accumulate_transition_summary(UiCiTransitionPerfSummary& summary,
                                             const UiCiFrameTiming& timing,
                                             const ::ui::scene::SceneEvidence& evidence) noexcept {
        summary.frames++;
        summary.total_frame_us += timing.frame_us;
        summary.total_tick_us += timing.tick_us;
        summary.total_render_us += timing.render_us;
        summary.total_record_us += timing.record_us;
        summary.total_execute_us += timing.execute_us;
        summary.total_present_us += timing.present_us;
        summary.total_cmds += evidence.exec.cmd_count;
        summary.total_alpha += evidence.exec.alpha_blend_count;
        summary.max_frame_us = std::max(summary.max_frame_us, timing.frame_us);
        summary.max_execute_us = std::max(summary.max_execute_us, timing.execute_us);
        summary.max_present_us = std::max(summary.max_present_us, timing.present_us);
        summary.max_cmds = std::max(summary.max_cmds, evidence.exec.cmd_count);
        summary.max_alpha = std::max(summary.max_alpha, evidence.exec.alpha_blend_count);
        summary.max_alpha_screen_x1000 = std::max(
            summary.max_alpha_screen_x1000,
            ui_ci_ratio_x1000(evidence.exec.alpha_blend_count,
                              static_cast<std::uint32_t>(screen_width * screen_height)));
    }

    void ui_ci_emit_transition_frame(const char* name,
                                     std::uint32_t frame_index,
                                     player::PlayerPage current_page,
                                     bool active,
                                     const UiCiFrameTiming& timing,
                                     const ::ui::scene::SceneEvidence& evidence) {
        ui_ci_perf_printf(
            "[ui-ci.perf_transition] name=%s frame=%u active=%u page=%s "
            "frame_us=%u tick_us=%u render_us=%u record_us=%u execute_us=%u present_us=%u "
            "cmd=%u alpha=%u alpha_screen_x1000=%u flush=%u layer_bytes=%u composite_pixels=%u\n",
            name ? name : "unknown",
            frame_index,
            active ? 1U : 0U,
            ui_ci_page_name(current_page),
            timing.frame_us,
            timing.tick_us,
            timing.render_us,
            timing.record_us,
            timing.execute_us,
            timing.present_us,
            evidence.exec.cmd_count,
            evidence.exec.alpha_blend_count,
            ui_ci_ratio_x1000(evidence.exec.alpha_blend_count,
                              static_cast<std::uint32_t>(screen_width * screen_height)),
            evidence.exec.batch_flushes,
            evidence.layer.layer_bytes,
            evidence.layer.composite_pixels);
    }

    template<typename ContextT>
    void ui_ci_emit_transition_capture(const char* name,
                                       const ContextT& ctx) {
        ui_ci_perf_printf(
            "[ui-ci.perf_transition_capture] name=%s "
            "capture_source_us=%u prepare_destination_us=%u "
            "capture_destination_us=%u first_compose_us=%u defer_frames=%u "
            "destination_capture_count=%u destination_snapshot=%u active=%u\n",
            name ? name : "unknown",
            ctx.layer_transition_capture_source_us,
            ctx.layer_transition_prepare_destination_us,
            ctx.layer_transition_capture_destination_us,
            ctx.layer_transition_first_compose_us,
            ctx.layer_transition_destination_capture_defer_frames,
            ctx.layer_transition_destination_capture_count,
            ctx.now_playing_transition.destination_snapshot ? 1U : 0U,
            ctx.now_playing_transition.active ? 1U : 0U);
    }

    void ui_ci_emit_transition_summary(const char* name,
                                       const UiCiTransitionPerfSummary& summary) {
        ui_ci_perf_printf(
            "[ui-ci.perf_transition_summary] name=%s frames=%u completed=%u "
            "total_frame_us=%llu total_tick_us=%llu total_render_us=%llu "
            "total_record_us=%llu total_execute_us=%llu total_present_us=%llu "
            "max_frame_us=%u max_execute_us=%u max_present_us=%u "
            "total_cmds=%llu max_cmds=%u total_alpha=%llu max_alpha=%u max_alpha_screen_x1000=%u\n",
            name ? name : "unknown",
            summary.frames,
            summary.completed,
            static_cast<unsigned long long>(summary.total_frame_us),
            static_cast<unsigned long long>(summary.total_tick_us),
            static_cast<unsigned long long>(summary.total_render_us),
            static_cast<unsigned long long>(summary.total_record_us),
            static_cast<unsigned long long>(summary.total_execute_us),
            static_cast<unsigned long long>(summary.total_present_us),
            summary.max_frame_us,
            summary.max_execute_us,
            summary.max_present_us,
            static_cast<unsigned long long>(summary.total_cmds),
            summary.max_cmds,
            static_cast<unsigned long long>(summary.total_alpha),
            summary.max_alpha,
            summary.max_alpha_screen_x1000);
    }

    template<typename SceneT, typename ContextT, typename TimedPumpFrameFn>
    UiCiTransitionPerfSummary ui_ci_capture_transition_perf(const char* name,
                                                           SceneT& scene,
                                                           ContextT& ctx,
                                                           TimedPumpFrameFn& timed_pump_frame,
                                                           player::PlayerDisplaySink* display_sink,
                                                           player::PlayerPage expected_page) {
        UiCiTransitionPerfSummary summary{};
        constexpr std::uint32_t kMaxTransitionFrames = 48;
        for (std::uint32_t frame = 0; frame < kMaxTransitionFrames; ++frame) {
            SDL_Delay(16);
            const auto timing = timed_pump_frame(display_sink);
            const auto evidence = ::ui::scene::make_scene_evidence(scene);
            const bool active = ctx.now_playing_transition.active;
            ui_ci_accumulate_transition_summary(summary, timing, evidence);
            ui_ci_emit_transition_frame(name, frame, ctx.current_page, active, timing, evidence);
            ui_ci_emit_transition_capture(name, ctx);
            char detail_page[64]{};
            std::snprintf(detail_page, sizeof(detail_page), "%s:frame%u", name ? name : "transition", frame);
            ui_ci_emit_perf_detail_evidence(detail_page, evidence);
            if (!active && ctx.current_page == expected_page) {
                summary.completed = 1U;
                break;
            }
        }
        ui_ci_emit_transition_summary(name, summary);
        return summary;
    }

    template<typename SceneT, typename ContextT, typename PumpFrameFn, typename TimedPumpFrameFn>
    void run_ui_ci_perf_evidence_log(SceneT& scene,
                                     ContextT& ctx,
                                     PumpFrameFn& pump_frame,
                                     TimedPumpFrameFn& timed_pump_frame,
                                     player::PlayerDisplaySink* display_sink) {
        ctx.set_page(player::PlayerPage::Home);
        auto home_timing = timed_pump_frame(display_sink);
        ui_ci_emit_perf_evidence_bundle("home", scene, home_timing);

        ctx.set_page(player::PlayerPage::NowPlaying);
        auto now_timing = timed_pump_frame(display_sink);
        ui_ci_emit_perf_evidence_bundle("now", scene, now_timing);

        ctx.set_page(player::PlayerPage::Library);
        auto library_timing = timed_pump_frame(display_sink);
        ui_ci_emit_perf_evidence_bundle("library", scene, library_timing);

        ctx.set_page(player::PlayerPage::Home);
        pump_frame();
    }

    template<typename SceneT, typename ContextT, typename TimedPumpFrameFn>
    void run_ui_ci_perf_only_log(SceneT& scene,
                                 ContextT& ctx,
                                 PlayerRuntime& runtime,
                                 TimedPumpFrameFn& timed_pump_frame,
                                 player::PlayerDisplaySink* display_sink) {
        auto settle = [&]() {
            for (int i = 0; i < 3; ++i) {
                (void)timed_pump_frame(display_sink);
            }
        };
        auto emit_stable = [&](const char* page) {
            settle();
            auto timing = timed_pump_frame(display_sink);
            ui_ci_emit_perf_evidence_bundle(page, scene, timing);
        };

        ctx.set_page(player::PlayerPage::Home);
        auto home_first_timing = timed_pump_frame(display_sink);
        ui_ci_emit_perf_evidence_bundle("home_first", scene, home_first_timing);
        emit_stable("home");

        ctx.set_page(player::PlayerPage::NowPlaying);
        emit_stable("now");

        ctx.set_page(player::PlayerPage::Library);
        emit_stable("library");

        const Rect list_rect = scene.world_rect(ctx.handles.list);
        if (list_rect.w > 0 && list_rect.h > 0) {
            runtime.dispatch_input(player::PlayerInputEvent::make_wheel(
                20,
                static_cast<float>(list_rect.x + list_rect.w / 2),
                static_cast<float>(list_rect.y + list_rect.h / 2),
                -4.0f));
        }
        emit_stable("library_stress");

        ctx.set_page(player::PlayerPage::Home);
        emit_stable("home_before_enter_now");
        const Rect bottom_rect = scene.world_rect(ctx.handles.bottom_hit);
        if (bottom_rect.w > 0 && bottom_rect.h > 0) {
            ui_ci_click(runtime,
                        bottom_rect.x + bottom_rect.w / 2,
                        bottom_rect.y + bottom_rect.h / 2);
            (void)ui_ci_capture_transition_perf(
                "enter_now", scene, ctx, timed_pump_frame, display_sink, player::PlayerPage::NowPlaying);
        } else {
            ui_ci_perf_printf("[ui-ci.perf_transition_summary] name=enter_now frames=0 completed=0 reason=invalid_bottom_hit\n");
        }
        emit_stable("now_after_enter");

        const Rect back_rect = scene.world_rect(ctx.handles.now_back);
        if (back_rect.w > 0 && back_rect.h > 0) {
            ui_ci_click(runtime,
                        back_rect.x + back_rect.w / 2,
                        back_rect.y + back_rect.h / 2);
            (void)ui_ci_capture_transition_perf(
                "exit_now", scene, ctx, timed_pump_frame, display_sink, player::PlayerPage::Home);
        } else {
            ui_ci_perf_printf("[ui-ci.perf_transition_summary] name=exit_now frames=0 completed=0 reason=invalid_now_back\n");
        }

        ctx.set_page(player::PlayerPage::Home);
        (void)timed_pump_frame(display_sink);
    }

    UiCiResult run_ui_ci(PlayerRuntime& runtime, player::PlayerDisplaySink* perf_display_sink = nullptr) {
        UiCiResult res{};
        auto& ctx = *runtime.controller();
        auto& scene = runtime.scene_ref();

        auto pump_frame = [&]() {
            ui_ci_pump_frame(runtime, nullptr);
        };
        auto timed_pump_frame = [&](player::PlayerDisplaySink* display_sink = nullptr) {
            return ui_ci_pump_frame_timed(runtime, display_sink);
        };
        auto wait_for_page = [&](player::PlayerPage expected_page, int max_frames = 36) {
            if (ctx.current_page == expected_page) return true;
            for (int i = 0; i < max_frames; ++i) {
                SDL_Delay(16);
                pump_frame();
                if (ctx.current_page == expected_page) {
                    return true;
                }
            }
            return ctx.current_page == expected_page;
        };
        auto settle_now_playing_transition = [&](player::PlayerPage expected_page) {
            if (!ctx.now_playing_transition.active) {
                return ctx.current_page == expected_page;
            }
            ctx.finish_now_playing_transition();
            pump_frame();
            return ctx.current_page == expected_page;
        };

        pump_frame();
        run_ui_ci_perf_evidence_log(scene, ctx, pump_frame, timed_pump_frame, perf_display_sink);

        run_object_tree_input_regressions(res);
        run_ui_ci_bootstrap_regressions(
            res, runtime, scene, ctx, wait_for_page, settle_now_playing_transition, pump_frame);

        run_layer_snapshot_regressions(res, scene, ctx, pump_frame);

        run_page_flow_regressions(
            res, runtime, scene, ctx, wait_for_page, settle_now_playing_transition, pump_frame);

        run_library_function_regressions(res, runtime, scene, ctx, pump_frame);

        run_now_playing_state_matrix_regressions(
            res, runtime, scene, ctx, wait_for_page, settle_now_playing_transition, pump_frame);

        std::printf("[ui-ci] done ok=%d failed=%d\n", res.ok ? 1 : 0, res.failed);
        return res;
    }

    int run_ui_ci_preview(SdlHostRuntime& runtime) noexcept {
        const UiCiResult result = run_ui_ci(*g_runtime, &runtime.display_sink);
        shutdown_player_preview(runtime);
        return result.ok ? 0 : 2;
    }

    int run_ui_ci_perf_only_preview(SdlHostRuntime& runtime) noexcept {
        UiCiPerfProfileLogScope log_scope{true};
        auto& ctx = *g_runtime->controller();
        auto& scene = g_runtime->scene_ref();
        auto timed_pump_frame = [&](player::PlayerDisplaySink* display_sink = nullptr) {
            return ui_ci_pump_frame_timed(*g_runtime, display_sink);
        };
        run_ui_ci_perf_only_log(scene, ctx, *g_runtime, timed_pump_frame, &runtime.display_sink);
        shutdown_player_preview(runtime);
        return 0;
    }
