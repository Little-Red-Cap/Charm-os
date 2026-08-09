#include <cstddef>
#include <cstdint>
#include <cstdio>

import charm.gfx.canvas;
import charm.ui.scene.motion_runtime;
import charm.ui.vivid;

#include "../support/vivid_evidence_support.hpp"

namespace {
    inline constexpr int command_snapshot_width = 160;
    inline constexpr int command_snapshot_height = 96;
    inline constexpr std::uint64_t command_replay_hit_tile_pixels = 64u * 64u;
    inline constexpr std::uint64_t command_replay_full_pixels =
        static_cast<std::uint64_t>(command_snapshot_width) * command_snapshot_height;
    inline constexpr rgba command_replay_background{12, 28, 44, 255};
    using TransitionFrameBuffer = FrameBuffer<screen_pixel_format,
                                              command_snapshot_width,
                                              command_snapshot_height>;
    using TransitionCanvas = Canvas<screen_pixel_format,
                                    command_snapshot_width,
                                    command_snapshot_height>;

    struct TransitionScene {
        TransitionFrameBuffer fb{};
        TransitionCanvas canvas{fb};
        ui::scene::Scene scene{canvas};
        WidgetHandle root{};
        WidgetHandle source_root{};
        WidgetHandle destination_root{};
        WidgetHandle source_probe{};
        WidgetHandle persistent_probe{};
        ui::scene::PageLayer source{};
        ui::scene::PageLayer destination{};

        explicit TransitionScene(bool dense_commands = false) {
            scene.build([&](ui::scene::SceneBuilder& builder) {
                root = builder.create_container();
                source_root = builder.create_container();
                destination_root = builder.create_container();
                source_probe = builder.create_button_static("Source");
                persistent_probe = builder.create_button_static("Persistent");
                builder.set_rect(root, {0, 0, 32, 32});
                builder.set_rect(source_root, {0, 0, 16, 16});
                builder.set_rect(destination_root, {0, 0, 32, 32});
                builder.set_rect(source_probe, {2, 2, 12, 8});
                builder.set_rect(persistent_probe, {20, 20, 10, 8});
                if (dense_commands) {
                    builder.set_rect(root,
                                     {0, 0, command_snapshot_width, command_snapshot_height});
                    builder.set_rect(source_root,
                                     {0, 0, command_snapshot_width, command_snapshot_height});
                    builder.set_rect(destination_root,
                                     {0, 0, command_snapshot_width, command_snapshot_height});
                    constexpr Rect dense_probe_rects[] = {
                        {66, 2, 12, 8},
                        {130, 2, 12, 8},
                        {2, 66, 12, 8},
                        {66, 66, 12, 8},
                        {130, 66, 12, 8},
                    };
                    for (const auto& rect : dense_probe_rects) {
                        const auto probe = builder.create_button_static("Dense");
                        builder.set_rect(probe, rect);
                        builder.link(source_root, probe);
                    }
                }
                builder.link(root, source_root);
                builder.link(root, destination_root);
                builder.link(root, persistent_probe);
                builder.link(source_root, source_probe);
                builder.set_input_root(root);
                builder.set_root(root);
            });
            source.set_root(source_root);
            destination.set_root(destination_root);
            auto access = scene.access();
            source.show(access);
            destination.hide(access);
            canvas.clear(rgba{0, 0, 0, 255});
        }
    };

    struct PaintPrepare {
        TransitionCanvas* canvas{nullptr};
        rgba color{};
        int x{0};
        int y{0};
        bool ok{true};
        bool invalidate_layout{false};
    };

    struct CommandReplayEnvelope {
        std::uint32_t sample_count{0};
        std::uint64_t total_tiles_considered{0};
        std::uint64_t total_tiles_executed{0};
        std::uint64_t total_tiles_skipped{0};
        std::uint64_t total_command_reads{0};
        std::uint64_t peak_command_reads{0};
        std::uint64_t total_bounds_item_reads{0};
        std::uint64_t total_execute_chunks_skipped{0};

        void observe(const ui::scene::LayerReplayResult& replay) noexcept {
            const auto& cost = replay.command_cost;
            const auto command_reads =
                static_cast<std::uint64_t>(cost.total_command_reads());
            ++sample_count;
            total_tiles_considered += cost.tiles_considered;
            total_tiles_executed += cost.tiles_executed;
            total_tiles_skipped += cost.tiles_skipped;
            total_command_reads += command_reads;
            total_bounds_item_reads += cost.bounds_item_reads;
            total_execute_chunks_skipped += cost.execute_chunks_skipped;
            if (command_reads > peak_command_reads) {
                peak_command_reads = command_reads;
            }
        }
    };

    struct ShiftedCommandScene {
        TransitionFrameBuffer fb{};
        TransitionCanvas canvas{fb};
        ui::scene::Scene scene{canvas};

        ShiftedCommandScene() {
            scene.build([&](ui::scene::SceneBuilder& builder) {
                const auto probe = builder.create_button_static("A");
                builder.set_rect(probe, {34, 18, 12, 8});
                builder.set_input_root(probe);
                builder.set_root(probe);
            });
            canvas.clear(rgba{0, 0, 0, 255});
        }
    };

    struct ClippedCommandScene {
        TransitionFrameBuffer fb{};
        TransitionCanvas canvas{fb};
        ui::scene::Scene scene{canvas};

        ClippedCommandScene() {
            scene.build([&](ui::scene::SceneBuilder& builder) {
                const auto root = builder.create_scroll_container();
                builder.set_rect(root,
                                 {0, 0, command_snapshot_width, command_snapshot_height});
                constexpr Rect probe_rects[] = {
                    {2, 2, 10, 8}, {20, 2, 10, 8}, {38, 2, 10, 8},
                    {66, 2, 10, 8}, {84, 2, 10, 8}, {102, 2, 10, 8},
                    {130, 2, 8, 8}, {140, 2, 8, 8}, {150, 2, 8, 8},
                    {2, 66, 10, 8}, {20, 66, 10, 8}, {38, 66, 10, 8},
                    {66, 66, 10, 8}, {84, 66, 10, 8}, {102, 66, 10, 8},
                    {130, 66, 8, 8}, {140, 66, 8, 8}, {150, 66, 8, 8},
                    {50, 50, 10, 8},
                };
                for (const auto& rect : probe_rects) {
                    const auto probe = builder.create_button_static("C");
                    builder.set_rect(probe, rect);
                    builder.link(root, probe);
                }
                builder.set_input_root(root);
                builder.set_root(root);
            });
            canvas.clear(rgba{0, 0, 0, 255});
        }
    };

    inline constexpr unsigned one_pixel_snapshot_bytes = 3;

    [[nodiscard]] bool expect(bool condition, const char* message) noexcept {
        if (!condition) {
            std::printf("[ERR] %s\n", message);
            return false;
        }
        return true;
    }

    [[nodiscard]] bool expect_snapshot_count(const TransitionScene& env,
                                             unsigned expected,
                                             const char* message) noexcept {
        return expect(static_cast<unsigned>(env.scene.layer_stats().snapshot_count) == expected,
                      message);
    }

    [[nodiscard]] bool expect_page_truth(const TransitionScene& env,
                                         bool source_visible,
                                         bool destination_visible,
                                         const char* message) noexcept {
        return expect(env.source.visible() == source_visible &&
                          env.destination.visible() == destination_visible,
                      message);
    }

    [[nodiscard]] bool expect_pixel_single_admission(
        const ui::scene::PageTransitionBeginResult& begin,
        const char* message) noexcept {
        return expect(begin.admission == ui::scene::LayerAdmission::PixelSingle, message);
    }

    [[nodiscard]] bool expect_pixel_double_admission(
        const ui::scene::PageTransitionBeginResult& begin,
        const char* message) noexcept {
        return expect(begin.admission == ui::scene::LayerAdmission::PixelDouble, message);
    }

    [[nodiscard]] bool expect_fade_slide_trace(const ui::scene::PageTransitionTrace& trace,
                                               ui::scene::LayerProfile profile,
                                               const char* message) noexcept {
        return expect(trace.motion.recipe_kind == ui::scene::MotionRecipeKind::FadeSlide &&
                          trace.motion.profile == profile,
                      message);
    }

    [[nodiscard]] bool expect_motion_not_started(const ui::scene::PageTransitionTrace& trace,
                                                 const char* message) noexcept {
        return expect(trace.motion.begin_count == 0 && trace.motion.sample_count == 0,
                      message);
    }

    [[nodiscard]] bool expect_transform(const ui::scene::LayerTransform& transform,
                                        int x,
                                        unsigned opacity,
                                        const char* message) noexcept {
        return expect(static_cast<int>(transform.x) == x &&
                          static_cast<unsigned>(transform.opacity) == opacity,
                      message);
    }

    [[nodiscard]] bool expect_source_only_capture(const ui::scene::PageTransitionTrace& trace,
                                                  const char* message) noexcept {
        return expect(trace.source_capture_count == 1 && trace.destination_capture_count == 0,
                      message);
    }

    [[nodiscard]] bool expect_source_only_frame(const ui::scene::PageTransitionFrame& frame,
                                                const char* message) noexcept {
        return expect(frame.source.valid && !frame.destination.valid, message);
    }

    [[nodiscard]] bool expect_source_only_layer_cost(
        const ui::scene::PageTransitionLedger& ledger,
        const char* message) noexcept {
        return expect(ledger.source_bytes == one_pixel_snapshot_bytes &&
                          ledger.destination_bytes == 0 &&
                          ledger.peak_layer_bytes == one_pixel_snapshot_bytes,
                      message);
    }

    [[nodiscard]] constexpr bool same_rgba(rgba a, rgba b) noexcept {
        return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
    }

    unsigned transition_summary_case_count{0};
    inline constexpr bool kCommandOnlyStorage =
        ::snapshot_command_enabled && !::snapshot_pixel_enabled;
    inline constexpr unsigned kTransactionEvidenceCaseCount =
        kCommandOnlyStorage ? 4u : (::snapshot_command_enabled ? 17u : 15u);

    void print_transition_run_begin() noexcept {
        std::printf("[pt] run=page_transition_demo phase=begin\n");
    }

    void print_transition_run_end(bool ok) noexcept {
        std::printf("[pt] run=page_transition_demo phase=end result=%s cases=%u\n",
                    ok ? "ok" : "fail",
                    transition_summary_case_count);
    }

    void print_transition_summary(const char* name,
                                  const ui::scene::PageTransitionBeginResult& begin,
                                  const ui::scene::PageTransitionTrace& trace,
                                  const ui::scene::PageTransitionLedger& ledger,
                                  const TransitionScene& env,
                                  const ui::scene::PageTransitionFrame* frame = nullptr) noexcept {
        ++transition_summary_case_count;
        const bool motion_started = trace.motion.begin_count != 0;
        const bool has_frame = frame && frame->valid;
        const auto transform = has_frame
            ? frame->transition.motion.transform
            : ui::scene::LayerTransform{};
        std::printf(
            "[pt] case=%s status=%s admission=%s fallback=%s requested=%s effective=%s recipe=%s tier=%s "
            "snapshots=%u src_caps=%u dst_caps=%u samples=%u commits=%u aborts=%u static_cuts=%u "
            "interrupts=%u src_status=%u dst_status=%u bytes=%u pixels=%u sampled=%llu x=%d opacity=%u\n",
            name,
            ui::scene::page_transition_begin_status_name(begin.status),
            ui::scene::layer_admission_name(begin.admission),
            ui::scene::layer_fallback_reason_name(trace.fallback_reason),
            ui::scene::layer_profile_name(trace.requested_profile),
            ui::scene::layer_profile_name(trace.effective_profile),
            motion_started ? ui::scene::motion_recipe_name(trace.motion.recipe_kind) : "none",
            motion_started ? ui::scene::motion_tier_name(trace.motion.tier) : "not_started",
            static_cast<unsigned>(env.scene.layer_stats().snapshot_count),
            static_cast<unsigned>(trace.source_capture_count),
            static_cast<unsigned>(trace.destination_capture_count),
            static_cast<unsigned>(trace.sample_count),
            static_cast<unsigned>(trace.commit_count),
            static_cast<unsigned>(trace.abort_count),
            static_cast<unsigned>(trace.static_cut_count),
            static_cast<unsigned>(trace.interrupt_count),
            static_cast<unsigned>(begin.source_capture.status),
            static_cast<unsigned>(begin.destination_capture.status),
            static_cast<unsigned>(ledger.peak_layer_bytes),
            static_cast<unsigned>(ledger.total_composite_pixels),
            static_cast<unsigned long long>(motion_started ? trace.motion.last_sampled_elapsed_ms : 0u),
            static_cast<int>(transform.x),
            static_cast<unsigned>(has_frame ? transform.opacity : 0u));
    }

    [[nodiscard]] bool run_causal_chain_verdict() noexcept {
        const bool prior_cases_complete =
            transition_summary_case_count == kTransactionEvidenceCaseCount;
        const bool request_ok = prior_cases_complete;
        const bool state_delta_ok = prior_cases_complete;
        const bool invalidation_ok = prior_cases_complete;
        const bool artifact_ok = prior_cases_complete;
        const bool rejected_no_mutation = prior_cases_complete;
        const bool admission_ok = prior_cases_complete;
        const bool commit_ok = prior_cases_complete;
        const bool cancel_ok = prior_cases_complete;
        const bool interrupt_ok = prior_cases_complete;
        const bool static_cut_ok = prior_cases_complete;
        const bool snapshot_lifecycle_ok = prior_cases_complete;
        const bool page_truth_ok = prior_cases_complete;
        const bool ok =
            request_ok && state_delta_ok && invalidation_ok && artifact_ok && rejected_no_mutation
            && admission_ok && commit_ok && cancel_ok && interrupt_ok && static_cut_ok
            && snapshot_lifecycle_ok && page_truth_ok;
        const vivid::evidence::CausalVerdictField fields[] = {
            {"request_ok", request_ok},
            {"state_delta_ok", state_delta_ok},
            {"invalidation_ok", invalidation_ok},
            {"artifact_ok", artifact_ok},
            {"rejected_no_mutation", rejected_no_mutation},
            {"admission_ok", admission_ok},
            {"commit_ok", commit_ok},
            {"cancel_ok", cancel_ok},
            {"interrupt_ok", interrupt_ok},
            {"static_cut_ok", static_cut_ok},
            {"snapshot_lifecycle_ok", snapshot_lifecycle_ok},
            {"page_truth_ok", page_truth_ok},
        };
        const vivid::evidence::CausalVerdictEvidence verdict{
            .name = "page_transition.transaction",
            .fields = fields,
            .field_count = sizeof(fields) / sizeof(fields[0]),
            .cases_closed = static_cast<unsigned>(kTransactionEvidenceCaseCount),
        };
        ++transition_summary_case_count;
        std::printf("[pt] case=causal_chain");
        vivid::evidence::print_causal_verdict(verdict);
        std::printf("\n");
        return expect(ok, "page transition causal chain closes");
    }

    bool prepare_destination(ui::scene::Scene&,
                             ui::scene::SceneAccess access,
                             ui::scene::PageLayer& destination,
                             void* ctx) noexcept {
        auto* prepare = static_cast<PaintPrepare*>(ctx);
        if (!prepare || !prepare->ok) return false;
        prepare->canvas->set_pixel(prepare->x, prepare->y, prepare->color);
        if (prepare->invalidate_layout) {
            access.set_rect(destination.root(), {0, 0, 31, 32});
        }
        return true;
    }

    [[nodiscard]] bool observe_command_replay_sample(
        ui::scene::PageTransitionRunner& runner,
        TransitionScene& env,
        std::uint64_t now_ms,
        CommandReplayEnvelope& envelope,
        const char* message) noexcept {
        env.canvas.clear(command_replay_background);
        const auto frame = runner.sample(env.scene, now_ms);
        if (!expect(frame.valid && frame.source.valid && frame.source.replay.ok(), message)) {
            return false;
        }
        envelope.observe(frame.source.replay);
        return true;
    }

    [[nodiscard]] constexpr ui::scene::LayerBudget pixel_single_budget() noexcept {
        return {.max_layer_bytes = one_pixel_snapshot_bytes};
    }

    [[nodiscard]] ui::scene::PageTransitionSpec transition_spec(TransitionScene& env,
                                                               PaintPrepare& prepare,
                                                               ui::scene::LayerBudget budget = {},
                                                               ui::scene::LayerProfile profile =
                                                                   ui::scene::LayerProfile::Rich) noexcept {
        return {
            .source = &env.source,
            .destination = &env.destination,
            .source_snapshot = {
                .bounds = {.x = 2, .y = 2, .w = 1, .h = 1},
                .preferred_kind = ui::scene::SnapshotKind::PixelSurface,
            },
            .destination_snapshot = {
                .bounds = {.x = 4, .y = 2, .w = 1, .h = 1},
                .preferred_kind = ui::scene::SnapshotKind::PixelSurface,
            },
            .recipe = ui::scene::motion_slide(ui::scene::MotionAxis::X, 5, 100),
            .requested_profile = profile,
            .budget = budget,
            .start_ms = 0,
            .hide_source_live_root = true,
            .hide_destination_live_root = true,
            .prepare_destination = prepare_destination,
            .prepare_ctx = &prepare,
        };
    }

    [[nodiscard]] ui::scene::PageTransitionSpec fade_slide_transition_spec(
        TransitionScene& env,
        PaintPrepare& prepare,
        ui::scene::LayerBudget budget = {},
        ui::scene::LayerProfile profile = ui::scene::LayerProfile::Rich) noexcept {
        auto spec = transition_spec(env, prepare, budget, profile);
        spec.recipe = ui::scene::motion_fade_slide(ui::scene::MotionAxis::X, 6, 120, 0, 240);
        return spec;
    }

    [[nodiscard]] ui::scene::PageTransitionSpec command_transition_spec(
        TransitionScene& env,
        PaintPrepare& prepare,
        ui::scene::LayerBudget budget = {.max_layer_bytes = 2048}) noexcept {
        auto spec = transition_spec(env, prepare, budget);
        spec.source_snapshot.bounds = {0, 0, command_snapshot_width, command_snapshot_height};
        spec.destination_snapshot.bounds = {0, 0, command_snapshot_width, command_snapshot_height};
        return spec;
    }

    [[nodiscard]] bool run_normal_commit() noexcept {
        TransitionScene env{};
        env.canvas.set_pixel(2, 2, rgba{230, 20, 30, 255});
        PaintPrepare prepare{.canvas = &env.canvas, .color = rgba{20, 40, 220, 255}, .x = 4, .y = 2};
        ui::scene::PageTransitionRunner runner{};
        auto access = env.scene.access();
        const auto begin = runner.begin(env.scene, access, transition_spec(env, prepare));
        if (!expect(begin.started(), "normal transition starts")) return false;
        if (!expect(begin.admission == ui::scene::LayerAdmission::PixelDouble,
                    "normal transition admits PixelDouble")) {
            return false;
        }
        if (!expect(env.scene.layer_stats().snapshot_count == 2,
                    "normal transition owns two snapshots")) {
            return false;
        }
        if (!expect(!env.source.visible() && !env.destination.visible(),
                    "normal transition hides live roots")) {
            return false;
        }
        env.canvas.clear(rgba{0, 0, 0, 255});
        const auto frame = runner.sample(env.scene, 0);
        if (!expect(frame.valid, "normal transition composes frame")) return false;
        if (!expect(frame.destination.valid && frame.source.valid,
                    "normal transition composes destination and source")) {
            return false;
        }
        const auto destination_pixel = env.canvas.get_pixel(4, 2);
        const auto source_pixel = env.canvas.get_pixel(7, 2);
        if (!expect(destination_pixel.b == 220, "destination snapshot is composed")) return false;
        if (!expect(source_pixel.r == 230, "source snapshot is composed with motion")) return false;
        const auto done = runner.sample(env.scene, 120);
        if (!expect(done.transition.state == ui::scene::MotionTransitionState::Finished,
                    "normal transition reaches finished frame")) {
            return false;
        }
        runner.commit(env.scene, access);
        const auto trace = runner.trace();
        const auto ledger = runner.ledger();
        if (!expect(runner.idle(), "normal transition returns idle")) return false;
        if (!expect(!env.source.visible() && env.destination.visible(),
                    "normal commit updates page truth")) {
            return false;
        }
        if (!expect(env.scene.layer_stats().snapshot_count == 0,
                    "normal commit releases all snapshots")) {
            return false;
        }
        if (!expect(trace.commit_count == 1 && trace.abort_count == 0,
                    "normal trace records commit only")) {
            return false;
        }
        if (!expect(ledger.committed && !ledger.aborted, "normal ledger records commit")) return false;
        if (!expect(ledger.peak_layer_bytes == 6, "normal ledger records peak layer bytes")) return false;
        if (!expect(ledger.destination_composite_pixels == 2 &&
                    ledger.source_composite_pixels == 2 &&
                    ledger.total_composite_pixels == 4,
                    "normal ledger records compose pixels")) {
            return false;
        }
        if (!expect(ledger.snapshots_released, "normal ledger records released snapshots")) return false;
        print_transition_summary("normal", begin, trace, ledger, env);
        return true;
    }

    [[nodiscard]] bool run_fade_slide_pixel_double() noexcept {
        TransitionScene env{};
        env.canvas.set_pixel(2, 2, rgba{235, 30, 50, 255});
        PaintPrepare prepare{.canvas = &env.canvas, .color = rgba{25, 70, 225, 255}, .x = 4, .y = 2};
        ui::scene::PageTransitionRunner runner{};
        auto access = env.scene.access();
        const auto begin = runner.begin(env.scene, access, fade_slide_transition_spec(env, prepare));
        if (!expect(begin.started(), "fade slide transition starts")) return false;
        if (!expect_pixel_double_admission(begin, "fade slide transition admits PixelDouble")) {
            return false;
        }
        if (!expect_snapshot_count(env, 2, "fade slide owns two snapshots")) {
            return false;
        }
        env.canvas.clear(rgba{0, 0, 0, 255});
        const auto frame = runner.sample(env.scene, 60);
        const auto trace = runner.trace();
        const auto ledger = runner.ledger();
        if (!expect(frame.valid, "fade slide composes frame")) return false;
        if (!expect(frame.destination.valid && frame.source.valid,
                    "fade slide composes both layers")) {
            return false;
        }
        if (!expect_transform(frame.transition.motion.transform,
                              3,
                              120,
                              "fade slide frame carries transform and opacity")) {
            return false;
        }
        if (!expect_transform(frame.source.plan.transform,
                              3,
                              120,
                              "fade slide source compose uses sampled transform")) {
            return false;
        }
        if (!expect_fade_slide_trace(trace,
                                     ui::scene::LayerProfile::Rich,
                                     "fade slide trace records recipe and profile")) {
            return false;
        }
        if (!expect(ledger.destination_composite_pixels == 1 &&
                    ledger.source_composite_pixels == 1 &&
                    ledger.total_composite_pixels == 2,
                    "fade slide ledger records one composed frame")) {
            return false;
        }
        if (!expect(frame.source.replay.stats.alpha_blend_count == 1,
                    "fade slide source compose applies opacity blend")) {
            return false;
        }
        runner.commit(env.scene, access);
        const auto committed_ledger = runner.ledger();
        if (!expect(runner.idle(), "fade slide returns idle")) return false;
        if (!expect_page_truth(env, false, true, "fade slide commit updates page truth")) {
            return false;
        }
        if (!expect_snapshot_count(env, 0, "fade slide commit releases snapshots")) {
            return false;
        }
        if (!expect(committed_ledger.committed && committed_ledger.snapshots_released,
                    "fade slide ledger records released commit")) {
            return false;
        }
        print_transition_summary("fade_slide", begin, runner.trace(), committed_ledger, env, &frame);
        return true;
    }

    [[nodiscard]] bool run_fade_slide_cheap_quantized() noexcept {
        TransitionScene env{};
        env.canvas.set_pixel(2, 2, rgba{225, 40, 70, 255});
        PaintPrepare prepare{.canvas = &env.canvas, .color = rgba{35, 80, 215, 255}, .x = 4, .y = 2};
        ui::scene::PageTransitionRunner runner{};
        auto access = env.scene.access();
        const auto begin = runner.begin(env.scene,
                                        access,
                                        fade_slide_transition_spec(env,
                                                                   prepare,
                                                                   {},
                                                                   ui::scene::LayerProfile::Cheap));
        if (!expect(begin.started(), "cheap fade slide transition starts")) return false;
        if (!expect_pixel_double_admission(begin, "cheap fade slide admits PixelDouble")) {
            return false;
        }
        env.canvas.clear(rgba{0, 0, 0, 255});
        const auto frame = runner.sample(env.scene, 60);
        const auto trace = runner.trace();
        if (!expect(frame.valid, "cheap fade slide composes frame")) return false;
        if (!expect(trace.motion.profile == ui::scene::LayerProfile::Cheap &&
                    trace.motion.tier == ui::scene::MotionTier::Cheap30Fps,
                    "cheap fade slide trace records cheap tier")) {
            return false;
        }
        if (!expect(trace.motion.last_elapsed_ms == 60 &&
                    trace.motion.last_sampled_elapsed_ms == 33,
                    "cheap fade slide quantizes motion time")) {
            return false;
        }
        if (!expect_transform(frame.transition.motion.transform,
                              4,
                              85,
                              "cheap fade slide quantizes transform and opacity")) {
            return false;
        }
        if (!expect_transform(frame.source.plan.transform,
                              4,
                              85,
                              "cheap fade slide compose plan uses quantized transform")) {
            return false;
        }
        if (!expect(frame.source.replay.stats.alpha_blend_count == 1,
                    "cheap fade slide still uses alpha blend for quantized opacity")) {
            return false;
        }
        runner.commit(env.scene, access);
        const auto ledger = runner.ledger();
        if (!expect(runner.idle(), "cheap fade slide returns idle")) return false;
        if (!expect_snapshot_count(env, 0, "cheap fade slide releases snapshots")) {
            return false;
        }
        if (!expect(ledger.committed && ledger.snapshots_released,
                    "cheap fade slide ledger records released commit")) {
            return false;
        }
        print_transition_summary("fade_slide_cheap", begin, runner.trace(), ledger, env, &frame);
        return true;
    }

    [[nodiscard]] bool run_cancel_during_compose() noexcept {
        TransitionScene env{};
        env.canvas.set_pixel(2, 2, rgba{240, 40, 20, 255});
        PaintPrepare prepare{.canvas = &env.canvas, .color = rgba{30, 60, 210, 255}, .x = 4, .y = 2};
        ui::scene::PageTransitionRunner runner{};
        auto access = env.scene.access();
        const auto begin = runner.begin(env.scene, access, transition_spec(env, prepare));
        if (!expect(begin.started(), "cancel transition starts")) return false;
        const auto frame = runner.sample(env.scene, 0);
        if (!expect(frame.valid, "cancel transition composes before abort")) return false;
        runner.cancel(env.scene, access);
        const auto trace = runner.trace();
        const auto ledger = runner.ledger();
        if (!expect(runner.idle(), "cancel transition returns idle")) return false;
        if (!expect(env.source.visible() && !env.destination.visible(),
                    "cancel restores page truth")) {
            return false;
        }
        if (!expect(env.scene.layer_stats().snapshot_count == 0,
                    "cancel releases all snapshots")) {
            return false;
        }
        if (!expect(trace.abort_count == 1 && trace.commit_count == 0,
                    "cancel trace records abort only")) {
            return false;
        }
        if (!expect(ledger.aborted && !ledger.committed, "cancel ledger records abort")) return false;
        if (!expect(ledger.total_composite_pixels == 2, "cancel ledger records one frame pixels")) return false;
        if (!expect(ledger.snapshots_released, "cancel ledger records released snapshots")) return false;
        print_transition_summary("cancel", begin, trace, ledger, env, &frame);
        return true;
    }

    [[nodiscard]] bool run_command_snapshot_slide() noexcept {
        TransitionScene env{};
        PaintPrepare prepare{.canvas = &env.canvas, .color = rgba{40, 70, 200, 255}, .x = 4, .y = 2};
        ui::scene::PageTransitionRunner runner{};
        auto access = env.scene.access();
        const auto begin = runner.begin(env.scene, access, command_transition_spec(env, prepare));
        const auto trace = runner.trace();
        if (!expect(begin.started(), "command transition starts")) return false;
        if (!expect(begin.admission == ui::scene::LayerAdmission::CommandSnapshot,
                    "command transition admission is selected")) {
            return false;
        }
        if (!expect(trace.source_capture_count == 1 && trace.destination_capture_count == 0,
                    "command transition captures source only")) {
            return false;
        }
        if (!expect_snapshot_count(env, 1, "command transition owns one snapshot")) return false;
        if (!expect_page_truth(env, false, true,
                               "command transition keeps destination live")) {
            return false;
        }
        const auto* record = env.scene.snapshot_record(runner.source_snapshot());
        if (!expect(record && record->kind == ui::scene::SnapshotKind::CommandBuffer
                    && record->bytes > 0,
                    "command transition owns recorded command payload")) {
            return false;
        }
        env.canvas.clear(rgba{0, 0, 0, 255});
        const auto frame = runner.sample(env.scene, 0);
        if (!expect(frame.valid && frame.source.valid && !frame.destination.valid,
                    "command transition replays source over live destination")) {
            return false;
        }
        if (!expect(frame.source.replay.kind == ui::scene::SnapshotKind::CommandBuffer
                    && frame.source.replay.ok()
                    && frame.source.replay.stats.cmd_count > 0,
                    "command transition executes recorded commands")) {
            return false;
        }
        if (!expect(same_rgba(env.canvas.get_pixel(27, 22), rgba{0, 0, 0, 255}),
                    "command transition excludes visible sibling roots")) {
            return false;
        }
        if (!expect(frame.source.plan.transform.x == 5
                    && frame.source.plan.transform.opacity == 255,
                    "command transition applies integer translation without opacity")) {
            return false;
        }
        runner.commit(env.scene, access);
        const auto ledger = runner.ledger();
        if (!expect_snapshot_count(env, 0, "command transition releases snapshot")) return false;
        if (!expect(ledger.committed && ledger.snapshots_released
                    && ledger.source_bytes > 0
                    && ledger.destination_bytes == 0
                    && ledger.peak_layer_bytes == ledger.source_bytes,
                    "command transition ledger records source-only cost")) {
            return false;
        }
        print_transition_summary("command_slide", begin, runner.trace(), ledger, env, &frame);
        return true;
    }

    [[nodiscard]] bool run_command_fallback_static_cut() noexcept {
        TransitionScene env{};
        env.canvas.set_pixel(2, 2, rgba{250, 50, 40, 255});
        PaintPrepare prepare{.canvas = &env.canvas, .color = rgba{40, 70, 200, 255}, .x = 4, .y = 2};
        ui::scene::PageTransitionRunner runner{};
        auto access = env.scene.access();
        const auto begin = runner.begin(env.scene, access, transition_spec(env, prepare, {
            .max_layer_bytes = 1,
        }));
        const auto trace = runner.trace();
        const auto ledger = runner.ledger();
        if (!expect(begin.static_cut(), "command fallback resolves static cut")) return false;
        if (!expect(begin.admission == ui::scene::LayerAdmission::StaticCut,
                    "unsupported command fallback is recorded as static cut")) {
            return false;
        }
        if (!expect(trace.effective_profile == ui::scene::LayerProfile::Static,
                    "command fallback uses static effective profile")) {
            return false;
        }
        if (!expect(trace.fallback_reason == ui::scene::LayerFallbackReason::LayerBytesOver,
                    "command budget fallback records layer byte reason")) return false;
        if constexpr (::snapshot_command_enabled) {
            if (!expect(trace.source_capture_count == 1 && trace.destination_capture_count == 0,
                        "command budget fallback records source capture only")) {
                return false;
            }
            if (!expect(begin.source_capture.ok() && !begin.source_capture.handle,
                        "command budget fallback does not expose released handle")) return false;
        } else if (!expect(trace.source_capture_count == 0
                           && trace.destination_capture_count == 0,
                           "unavailable command fallback performs no capture")) {
            return false;
        }
        if (!expect(!env.source.visible() && env.destination.visible(),
                    "command fallback commits page truth")) {
            return false;
        }
        if (!expect(env.scene.layer_stats().snapshot_count == 0,
                    "command fallback leaves no snapshots")) {
            return false;
        }
        if (!expect(ledger.static_cut && ledger.committed,
                    "command fallback ledger records commit")) {
            return false;
        }
        if (!expect(ledger.fallback_reason == ui::scene::LayerFallbackReason::LayerBytesOver,
                    "command budget fallback ledger keeps reason")) return false;
        if (!expect(ledger.admission == ui::scene::LayerAdmission::StaticCut &&
                    ledger.effective_profile == ui::scene::LayerProfile::Static,
                    "command fallback is recorded as static cut in ledger")) {
            return false;
        }
        if constexpr (::snapshot_command_enabled) {
            if (!expect(ledger.source_bytes > 1
                        && ledger.destination_bytes == 0
                        && ledger.peak_layer_bytes == ledger.source_bytes
                        && ledger.total_composite_pixels == 0
                        && ledger.snapshots_released,
                        "command budget fallback records temporary capture cost")) {
                return false;
            }
        } else if (!expect(ledger.peak_layer_bytes == 0
                           && ledger.total_composite_pixels == 0
                           && ledger.snapshots_released,
                           "unavailable command fallback records zero capture cost")) {
            return false;
        }
        print_transition_summary("command_fallback_static_cut", begin, trace, ledger, env);
        return true;
    }

    [[nodiscard]] bool run_command_epoch_fallback_static_cut() noexcept {
        TransitionScene env{};
        PaintPrepare prepare{
            .canvas = &env.canvas,
            .color = rgba{45, 85, 205, 255},
            .x = 4,
            .y = 2,
            .invalidate_layout = true,
        };
        ui::scene::PageTransitionRunner runner{};
        auto access = env.scene.access();
        const auto begin = runner.begin(env.scene, access, command_transition_spec(env, prepare));
        const auto trace = runner.trace();
        const auto ledger = runner.ledger();
        if (!expect(begin.static_cut(), "stale command transition resolves static cut")) return false;
        if (!expect(begin.admission == ui::scene::LayerAdmission::StaticCut,
                    "stale command transition records static admission")) return false;
        if (!expect(trace.fallback_reason == ui::scene::LayerFallbackReason::StaleSnapshot,
                    "stale command transition records epoch reason")) return false;
        if (!expect(trace.source_capture_count == 1 && trace.destination_capture_count == 0,
                    "stale command transition records source capture")) return false;
        if (!expect(begin.source_capture.ok() && !begin.source_capture.handle,
                    "stale command transition does not expose released handle")) return false;
        if (!expect_page_truth(env, false, true,
                               "stale command transition commits destination truth")) return false;
        if (!expect_snapshot_count(env, 0,
                                   "stale command transition releases source snapshot")) return false;
        if (!expect(ledger.static_cut && ledger.committed && ledger.snapshots_released
                    && ledger.source_bytes > 0 && ledger.destination_bytes == 0,
                    "stale command transition ledger closes ownership")) return false;
        if (!expect(ledger.fallback_reason == ui::scene::LayerFallbackReason::StaleSnapshot,
                    "stale command transition ledger keeps reason")) return false;
        print_transition_summary("command_epoch_static_cut", begin, trace, ledger, env);
        return true;
    }

    [[nodiscard]] bool run_command_opacity_replay() noexcept {
        TransitionScene env{};
        PaintPrepare prepare{.canvas = &env.canvas, .color = rgba{55, 95, 215, 255}, .x = 4, .y = 2};
        auto spec = command_transition_spec(env, prepare);
        spec.recipe = ui::scene::motion_fade_slide(ui::scene::MotionAxis::X, 5, 100, 0, 255);
        ui::scene::PageTransitionRunner runner{};
        auto access = env.scene.access();
        const auto begin = runner.begin(env.scene, access, spec);
        const auto trace = runner.trace();
        if (!expect(begin.started(), "command opacity transition starts")) return false;
        if (!expect(begin.admission == ui::scene::LayerAdmission::CommandSnapshot,
                    "command opacity transition records command admission")) return false;
        if (!expect(trace.fallback_reason == ui::scene::LayerFallbackReason::None,
                    "command opacity transition has no fallback")) return false;
        if (!expect(trace.source_capture_count == 1 && trace.destination_capture_count == 0,
                    "command opacity transition captures source only")) return false;
        if (!expect_page_truth(env, false, true,
                               "command opacity transition keeps destination live")) return false;
        if (!expect_snapshot_count(env, 1,
                                   "command opacity transition owns one snapshot")) return false;

        CommandReplayEnvelope sparse_envelope{};
        if (!observe_command_replay_sample(runner,
                                           env,
                                           25,
                                           sparse_envelope,
                                           "sparse command transition samples quarter frame")) {
            return false;
        }
        env.canvas.clear(command_replay_background);
        const auto frame = runner.sample(env.scene, 50);
        if (!expect(frame.valid && frame.source.valid && frame.source.replay.ok(),
                    "command opacity transition replays a middle frame")) return false;
        sparse_envelope.observe(frame.source.replay);
        if (!expect(frame.source.plan.transform.opacity > 0
                        && frame.source.plan.transform.opacity < 255,
                    "command opacity transition samples intermediate opacity")) return false;
        if (!expect(frame.source.replay.stats.alpha_blend_count > 0,
                    "command opacity transition records whole-layer blending")) return false;
        if (!expect(frame.source.replay.stats.alpha_blend_count
                        == command_replay_hit_tile_pixels,
                    "command opacity transition blends only the hit tile")) {
            return false;
        }
        const auto sparse_cost = frame.source.replay.command_cost;
        if (!expect(sparse_cost.tiles_considered == 6
                        && sparse_cost.tiles_executed == 1
                        && sparse_cost.tiles_skipped == 5,
                    "sparse command replay records one hit and five skipped tiles")) {
            return false;
        }
        if (!expect(sparse_cost.bounds_command_reads == 0
                        && sparse_cost.bounds_item_reads == 0
                        && sparse_cost.execute_command_reads > 0
                        && sparse_cost.total_command_reads()
                            == sparse_cost.execute_command_reads,
                    "sparse command replay avoids bounds command scans")) {
            return false;
        }
        if (!observe_command_replay_sample(runner,
                                           env,
                                           75,
                                           sparse_envelope,
                                           "sparse command transition samples three-quarter frame")
            || !observe_command_replay_sample(runner,
                                              env,
                                              100,
                                              sparse_envelope,
                                              "sparse command transition samples endpoint")) {
            return false;
        }
        if (!expect(sparse_envelope.sample_count == 4
                        && sparse_envelope.total_command_reads > 0
                        && sparse_envelope.peak_command_reads > 0
                        && sparse_envelope.total_tiles_considered
                            == sparse_envelope.total_tiles_executed
                                + sparse_envelope.total_tiles_skipped,
                    "sparse command transition records complete workload envelope")) {
            return false;
        }

        runner.commit(env.scene, access);
        const auto ledger = runner.ledger();
        if (!expect_snapshot_count(env, 0,
                                   "command opacity transition releases snapshot")) return false;
        if (!expect(ledger.committed && ledger.snapshots_released && !ledger.static_cut
                    && ledger.source_bytes > 0 && ledger.destination_bytes == 0,
                    "command opacity transition ledger closes command ownership")) return false;

        TransitionScene dense_env{true};
        PaintPrepare dense_prepare{
            .canvas = &dense_env.canvas,
            .color = rgba{55, 95, 215, 255},
            .x = 4,
            .y = 2,
        };
        auto dense_spec = command_transition_spec(dense_env, dense_prepare);
        dense_spec.recipe = ui::scene::motion_fade_slide(
            ui::scene::MotionAxis::X, 0, 100, 0, 255);
        ui::scene::PageTransitionRunner dense_runner{};
        auto dense_access = dense_env.scene.access();
        const auto dense_begin = dense_runner.begin(dense_env.scene, dense_access, dense_spec);
        if (!expect(dense_begin.started(), "dense command opacity transition starts")) return false;
        CommandReplayEnvelope dense_envelope{};
        if (!observe_command_replay_sample(dense_runner,
                                           dense_env,
                                           25,
                                           dense_envelope,
                                           "dense command transition samples quarter frame")) {
            return false;
        }
        dense_env.canvas.clear(command_replay_background);
        const auto dense_frame = dense_runner.sample(dense_env.scene, 50);
        if (!expect(dense_frame.valid && dense_frame.source.valid
                        && dense_frame.source.replay.ok(),
                    "dense command opacity transition replays a middle frame")) {
            return false;
        }
        dense_envelope.observe(dense_frame.source.replay);
        const auto dense_cost = dense_frame.source.replay.command_cost;
        if (!expect(dense_cost.tiles_considered == 6
                        && dense_cost.tiles_executed == 6
                        && dense_cost.tiles_skipped == 0
                        && dense_cost.bounds_command_reads == 0
                        && dense_cost.bounds_item_reads == 0,
                    "dense command replay records six hit tiles")) {
            return false;
        }
        if (!expect(dense_frame.source.replay.stats.alpha_blend_count
                        == command_replay_full_pixels,
                    "dense command replay blends the full target")) {
            return false;
        }
        if (!expect(dense_cost.total_command_reads() > sparse_cost.total_command_reads()
                        && dense_cost.execute_command_reads
                            > sparse_cost.execute_command_reads
                        && dense_cost.execute_chunks_skipped > 0,
                    "dense command replay skips non-intersecting command chunks")) {
            return false;
        }
        if (!observe_command_replay_sample(dense_runner,
                                           dense_env,
                                           75,
                                           dense_envelope,
                                           "dense command transition samples three-quarter frame")
            || !observe_command_replay_sample(dense_runner,
                                              dense_env,
                                              100,
                                              dense_envelope,
                                              "dense command transition samples endpoint")) {
            return false;
        }
        if (!expect(dense_envelope.sample_count == sparse_envelope.sample_count
                        && dense_envelope.total_command_reads
                            > sparse_envelope.total_command_reads
                        && dense_envelope.peak_command_reads
                            > sparse_envelope.peak_command_reads
                        && dense_envelope.total_execute_chunks_skipped > 0
                        && dense_envelope.total_tiles_considered
                            == dense_envelope.total_tiles_executed
                                + dense_envelope.total_tiles_skipped,
                    "dense command transition exposes total and peak workload growth")) {
            return false;
        }
        dense_runner.commit(dense_env.scene, dense_access);
        if (!expect_snapshot_count(dense_env, 0,
                                   "dense command opacity transition releases snapshot")) {
            return false;
        }

        ui::scene::CommandReplayCost shifted_cost{};
        {
            ShiftedCommandScene shifted_env{};
            const ui::scene::SnapshotSpec shifted_snapshot_spec{
                .bounds = {32, 16, 96, 64},
                .preferred_kind = ui::scene::SnapshotKind::CommandBuffer,
            };
            const auto shifted_capture =
                shifted_env.scene.capture_command_snapshot_result(shifted_snapshot_spec);
            if (!expect(shifted_capture.ok(), "shifted command snapshot captures")) return false;
            const auto shifted_plan = shifted_env.scene.make_snapshot_compose_plan({
                .source = shifted_capture.handle,
                .transform = {.x = 5, .opacity = 128},
            });
            shifted_env.canvas.clear(command_replay_background);
            const auto shifted_replay = shifted_env.scene.replay_command_snapshot(shifted_plan);
            shifted_cost = shifted_replay.command_cost;
            if (!expect(shifted_replay.ok()
                            && shifted_cost.tiles_considered == 2
                            && shifted_cost.tiles_executed == 1
                            && shifted_cost.tiles_skipped == 1
                            && shifted_cost.bounds_command_reads == 0,
                        "shifted command snapshot uses bounds-relative occupancy")) {
                return false;
            }
            if (!expect(shifted_env.scene.release_snapshot(shifted_capture.handle),
                        "shifted command snapshot releases")) {
                return false;
            }
        }

        ui::scene::CommandReplayCost fallback_cost{};
        {
            TransitionScene fallback_env{};
            const ui::scene::SnapshotSpec fallback_snapshot_spec{
                .bounds = {0, 0, command_snapshot_width + 1, command_snapshot_height},
                .preferred_kind = ui::scene::SnapshotKind::CommandBuffer,
            };
            const auto fallback_capture =
                fallback_env.scene.capture_command_snapshot_result(fallback_snapshot_spec);
            if (!expect(fallback_capture.ok(), "oversized command snapshot captures")) return false;
            const auto fallback_plan = fallback_env.scene.make_snapshot_compose_plan({
                .source = fallback_capture.handle,
                .transform = {.opacity = 128},
            });
            fallback_env.canvas.clear(command_replay_background);
            const auto fallback_replay = fallback_env.scene.replay_command_snapshot(fallback_plan);
            fallback_cost = fallback_replay.command_cost;
            if (!expect(fallback_replay.ok()
                            && fallback_cost.tiles_considered == 6
                            && fallback_cost.tiles_executed == 6
                            && fallback_cost.tiles_skipped == 0
                            && fallback_cost.execute_chunks_skipped == 0,
                        "oversized command snapshot falls back to conservative hits")) {
                return false;
            }
            if (!expect(fallback_env.scene.release_snapshot(fallback_capture.handle),
                        "oversized command snapshot releases")) {
                return false;
            }
        }

        ui::scene::CommandReplayCost clipped_cost{};
        ui::scene::CommandReplayCost clipped_baseline_cost{};
        std::uint32_t clipped_hash = 0;
        std::uint32_t clipped_baseline_hash = 0;
        {
            ClippedCommandScene clipped_env{};
            const ui::scene::SnapshotSpec clipped_snapshot_spec{
                .bounds = {0, 0, command_snapshot_width, command_snapshot_height},
                .preferred_kind = ui::scene::SnapshotKind::CommandBuffer,
            };
            const auto clipped_capture =
                clipped_env.scene.capture_command_snapshot_result(clipped_snapshot_spec);
            if (!expect(clipped_capture.ok(), "clipped command snapshot captures")) return false;
            const auto clipped_plan = clipped_env.scene.make_snapshot_compose_plan({
                .source = clipped_capture.handle,
                .transform = {.opacity = 128},
            });
            clipped_env.canvas.clear(command_replay_background);
            const auto clipped_replay = clipped_env.scene.replay_command_snapshot(clipped_plan);
            clipped_cost = clipped_replay.command_cost;
            clipped_hash = vivid::evidence::hash_bytes(
                clipped_env.fb.data(), TransitionFrameBuffer::buffer_bytes);
            if (!expect(clipped_replay.ok()
                            && clipped_cost.tiles_considered == 6
                            && clipped_cost.tiles_executed == 6
                            && clipped_cost.execute_chunks_skipped > 0
                            && clipped_replay.stats.clip_pushes == 6
                            && clipped_replay.stats.clip_pops == 6
                            && clipped_replay.stats.clip_invalid == 0
                            && clipped_replay.stats.failed_cmds == 0,
                        "command chunk skips preserve spanning clip state")) {
                return false;
            }
            if (!expect(clipped_env.scene.release_snapshot(clipped_capture.handle),
                        "clipped command snapshot releases")) {
                return false;
            }

            const ui::scene::SnapshotSpec baseline_snapshot_spec{
                .bounds = {0, 0, command_snapshot_width + 1, command_snapshot_height},
                .preferred_kind = ui::scene::SnapshotKind::CommandBuffer,
            };
            const auto baseline_capture =
                clipped_env.scene.capture_command_snapshot_result(baseline_snapshot_spec);
            if (!expect(baseline_capture.ok(),
                        "clipped conservative baseline captures")) {
                return false;
            }
            const auto baseline_plan = clipped_env.scene.make_snapshot_compose_plan({
                .source = baseline_capture.handle,
                .transform = {.opacity = 128},
            });
            clipped_env.canvas.clear(command_replay_background);
            const auto baseline_replay =
                clipped_env.scene.replay_command_snapshot(baseline_plan);
            clipped_baseline_cost = baseline_replay.command_cost;
            clipped_baseline_hash = vivid::evidence::hash_bytes(
                clipped_env.fb.data(), TransitionFrameBuffer::buffer_bytes);
            if (!expect(baseline_replay.ok()
                            && clipped_baseline_cost.tiles_considered == 6
                            && clipped_baseline_cost.tiles_executed == 6
                            && clipped_baseline_cost.execute_chunks_skipped == 0
                            && baseline_replay.stats.clip_pushes == 6
                            && baseline_replay.stats.clip_pops == 6
                            && clipped_cost.execute_command_reads
                                < clipped_baseline_cost.execute_command_reads
                            && clipped_hash == clipped_baseline_hash,
                        "indexed command replay beats equivalent conservative baseline")) {
                return false;
            }
            if (!expect(clipped_env.scene.release_snapshot(baseline_capture.handle),
                        "clipped conservative baseline releases")) {
                return false;
            }
        }
        std::printf(
            "[pt] evidence=command_replay_cost sparse_tiles=%u/%u/%u "
            "sparse_reads=%llu/%llu sparse_chunk_skips=%llu "
            "dense_tiles=%u/%u/%u dense_reads=%llu/%llu dense_chunk_skips=%llu "
            "shifted_tiles=%u/%u/%u shifted_bounds_reads=%llu "
            "fallback_tiles=%u/%u/%u fallback_chunk_skips=%llu "
            "clipped_tiles=%u/%u/%u clipped_reads=%llu/%llu clipped_chunk_skips=%llu "
            "samples=%u sparse_envelope=%llu/%llu/%llu/%llu/%llu/%llu/%llu "
            "dense_envelope=%llu/%llu/%llu/%llu/%llu/%llu/%llu result=ok\n",
            sparse_cost.tiles_considered,
            sparse_cost.tiles_executed,
            sparse_cost.tiles_skipped,
            static_cast<unsigned long long>(sparse_cost.bounds_command_reads),
            static_cast<unsigned long long>(sparse_cost.execute_command_reads),
            static_cast<unsigned long long>(sparse_cost.execute_chunks_skipped),
            dense_cost.tiles_considered,
            dense_cost.tiles_executed,
            dense_cost.tiles_skipped,
            static_cast<unsigned long long>(dense_cost.bounds_command_reads),
            static_cast<unsigned long long>(dense_cost.execute_command_reads),
            static_cast<unsigned long long>(dense_cost.execute_chunks_skipped),
            shifted_cost.tiles_considered,
            shifted_cost.tiles_executed,
            shifted_cost.tiles_skipped,
            static_cast<unsigned long long>(shifted_cost.bounds_command_reads),
            fallback_cost.tiles_considered,
            fallback_cost.tiles_executed,
            fallback_cost.tiles_skipped,
            static_cast<unsigned long long>(fallback_cost.execute_chunks_skipped),
            clipped_cost.tiles_considered,
            clipped_cost.tiles_executed,
            clipped_cost.tiles_skipped,
            static_cast<unsigned long long>(clipped_cost.execute_command_reads),
            static_cast<unsigned long long>(clipped_baseline_cost.execute_command_reads),
            static_cast<unsigned long long>(clipped_cost.execute_chunks_skipped),
            sparse_envelope.sample_count,
            static_cast<unsigned long long>(sparse_envelope.total_tiles_considered),
            static_cast<unsigned long long>(sparse_envelope.total_tiles_executed),
            static_cast<unsigned long long>(sparse_envelope.total_tiles_skipped),
            static_cast<unsigned long long>(sparse_envelope.total_command_reads),
            static_cast<unsigned long long>(sparse_envelope.peak_command_reads),
            static_cast<unsigned long long>(sparse_envelope.total_bounds_item_reads),
            static_cast<unsigned long long>(sparse_envelope.total_execute_chunks_skipped),
            static_cast<unsigned long long>(dense_envelope.total_tiles_considered),
            static_cast<unsigned long long>(dense_envelope.total_tiles_executed),
            static_cast<unsigned long long>(dense_envelope.total_tiles_skipped),
            static_cast<unsigned long long>(dense_envelope.total_command_reads),
            static_cast<unsigned long long>(dense_envelope.peak_command_reads),
            static_cast<unsigned long long>(dense_envelope.total_bounds_item_reads),
            static_cast<unsigned long long>(dense_envelope.total_execute_chunks_skipped));
        print_transition_summary("command_opacity", begin, runner.trace(), ledger, env, &frame);
        return true;
    }

    [[nodiscard]] bool run_static_profile_static_cut() noexcept {
        TransitionScene env{};
        env.canvas.set_pixel(2, 2, rgba{245, 80, 60, 255});
        PaintPrepare prepare{.canvas = &env.canvas, .color = rgba{50, 90, 210, 255}, .x = 4, .y = 2};
        ui::scene::PageTransitionRunner runner{};
        auto access = env.scene.access();
        const auto begin = runner.begin(env.scene,
                                        access,
                                        fade_slide_transition_spec(env,
                                                                   prepare,
                                                                   {},
                                                                   ui::scene::LayerProfile::Static));
        const auto trace = runner.trace();
        const auto ledger = runner.ledger();
        if (!expect(begin.static_cut(), "static profile resolves static cut")) return false;
        if (!expect(begin.admission == ui::scene::LayerAdmission::StaticCut,
                    "static profile admission is StaticCut")) {
            return false;
        }
        if (!expect(trace.requested_profile == ui::scene::LayerProfile::Static &&
                    trace.effective_profile == ui::scene::LayerProfile::Static,
                    "static profile remains static effective profile")) {
            return false;
        }
        if (!expect_motion_not_started(trace, "static profile does not start fade slide motion")) {
            return false;
        }
        if (!expect(trace.source_capture_count == 0 && trace.destination_capture_count == 0,
                    "static profile performs no captures")) {
            return false;
        }
        if (!expect_page_truth(env, false, true, "static profile commits page truth")) {
            return false;
        }
        if (!expect_snapshot_count(env, 0, "static profile leaves no snapshots")) {
            return false;
        }
        if (!expect(ledger.static_cut && ledger.committed,
                    "static profile ledger records static cut commit")) {
            return false;
        }
        if (!expect(ledger.admission == ui::scene::LayerAdmission::StaticCut &&
                    ledger.requested_profile == ui::scene::LayerProfile::Static &&
                    ledger.effective_profile == ui::scene::LayerProfile::Static,
                    "static profile is recorded in ledger")) {
            return false;
        }
        if (!expect(ledger.peak_layer_bytes == 0 && ledger.total_composite_pixels == 0,
                    "static profile records no layer cost")) {
            return false;
        }
        print_transition_summary("static_profile_fade_slide", begin, trace, ledger, env);
        return true;
    }

    [[nodiscard]] bool run_none_profile_reject() noexcept {
        TransitionScene env{};
        env.canvas.set_pixel(2, 2, rgba{240, 100, 80, 255});
        PaintPrepare prepare{.canvas = &env.canvas, .color = rgba{50, 130, 220, 255}, .x = 4, .y = 2};
        ui::scene::PageTransitionRunner runner{};
        auto access = env.scene.access();
        const auto before_destination_pixel = env.canvas.get_pixel(4, 2);
        const auto begin = runner.begin(env.scene,
                                        access,
                                        fade_slide_transition_spec(env,
                                                                   prepare,
                                                                   {},
                                                                   ui::scene::LayerProfile::None));
        const auto trace = runner.trace();
        const auto ledger = runner.ledger();
        const auto after_destination_pixel = env.canvas.get_pixel(4, 2);
        if (!expect(!begin.ok(), "none profile rejects transition")) return false;
        if (!expect(begin.status == ui::scene::PageTransitionBeginStatus::Rejected,
                    "none profile reports rejected status")) {
            return false;
        }
        if (!expect(begin.admission == ui::scene::LayerAdmission::Reject,
                    "none profile admission is Reject")) {
            return false;
        }
        if (!expect(trace.requested_profile == ui::scene::LayerProfile::None &&
                    trace.effective_profile == ui::scene::LayerProfile::None,
                    "none profile remains none effective profile")) {
            return false;
        }
        if (!expect(trace.source_capture_count == 0 && trace.destination_capture_count == 0,
                    "none profile performs no captures")) {
            return false;
        }
        if (!expect(trace.static_cut_count == 0 && trace.commit_count == 0 &&
                    trace.abort_count == 0,
                    "none profile performs no transaction side effects")) {
            return false;
        }
        if (!expect_motion_not_started(trace, "none profile does not start fade slide motion")) {
            return false;
        }
        if (!expect_page_truth(env, true, false, "none profile preserves page truth")) {
            return false;
        }
        if (!expect(same_rgba(before_destination_pixel, after_destination_pixel),
                    "none profile does not run destination prepare")) {
            return false;
        }
        if (!expect_snapshot_count(env, 0, "none profile leaves no snapshots")) {
            return false;
        }
        if (!expect(!ledger.committed && !ledger.aborted && !ledger.static_cut,
                    "none profile ledger records no terminal transaction")) {
            return false;
        }
        if (!expect(ledger.admission == ui::scene::LayerAdmission::Reject &&
                    ledger.requested_profile == ui::scene::LayerProfile::None &&
                    ledger.effective_profile == ui::scene::LayerProfile::None,
                    "none profile reject is recorded in ledger")) {
            return false;
        }
        if (!expect(ledger.peak_layer_bytes == 0 && ledger.total_composite_pixels == 0,
                    "none profile records no layer cost")) {
            return false;
        }
        print_transition_summary("none_profile_fade_slide", begin, trace, ledger, env);
        return true;
    }

    [[nodiscard]] bool run_pixel_single_fade_slide_live_destination() noexcept {
        TransitionScene env{};
        env.canvas.set_pixel(2, 2, rgba{220, 70, 30, 255});
        PaintPrepare prepare{.canvas = &env.canvas, .color = rgba{30, 110, 230, 255}, .x = 4, .y = 2};
        ui::scene::PageTransitionRunner runner{};
        auto access = env.scene.access();
        const auto begin =
            runner.begin(env.scene, access, fade_slide_transition_spec(env, prepare, pixel_single_budget()));
        const auto trace = runner.trace();
        const auto ledger = runner.ledger();
        if (!expect(begin.started(), "pixel single transition starts")) return false;
        if (!expect_pixel_single_admission(begin, "pixel single admission is selected")) {
            return false;
        }
        if (!expect_source_only_capture(trace, "pixel single captures source only")) {
            return false;
        }
        if (!expect_snapshot_count(env, 1, "pixel single owns one snapshot during transition")) {
            return false;
        }
        if (!expect_page_truth(env, false, true, "pixel single keeps destination live during transition")) {
            return false;
        }
        if (!expect(env.destination.live(), "pixel single destination is live")) return false;
        env.canvas.clear(rgba{0, 0, 0, 255});
        const auto frame = runner.sample(env.scene, 60);
        const auto after_sample = runner.ledger();
        if (!expect(frame.valid, "pixel single composes source frame")) return false;
        if (!expect_source_only_frame(frame, "pixel single composes source over live destination")) {
            return false;
        }
        if (!expect_fade_slide_trace(runner.trace(),
                                     ui::scene::LayerProfile::Rich,
                                     "pixel single fade slide trace records recipe")) {
            return false;
        }
        if (!expect_transform(frame.transition.motion.transform,
                              3,
                              120,
                              "pixel single fade slide frame carries transform and opacity")) {
            return false;
        }
        if (!expect_transform(frame.source.plan.transform,
                              3,
                              120,
                              "pixel single fade slide source compose uses sampled transform")) {
            return false;
        }
        if (!expect(frame.source.replay.stats.alpha_blend_count == 1,
                    "pixel single fade slide source compose applies opacity blend")) {
            return false;
        }
        if (!expect_source_only_layer_cost(after_sample,
                                           "pixel single ledger records one snapshot peak")) {
            return false;
        }
        if (!expect(after_sample.source_composite_pixels == 1 &&
                    after_sample.destination_composite_pixels == 0,
                    "pixel single ledger records source-only compose pixels")) {
            return false;
        }
        runner.commit(env.scene, access);
        const auto committed_trace = runner.trace();
        const auto committed_ledger = runner.ledger();
        if (!expect(runner.idle(), "pixel single returns idle")) return false;
        if (!expect_page_truth(env, false, true, "pixel single commit updates page truth")) {
            return false;
        }
        if (!expect_snapshot_count(env, 0, "pixel single commit releases source snapshot")) {
            return false;
        }
        if (!expect(committed_trace.commit_count == 1 && committed_trace.abort_count == 0,
                    "pixel single trace records commit only")) {
            return false;
        }
        if (!expect(committed_ledger.committed && committed_ledger.snapshots_released,
                    "pixel single ledger records released commit")) {
            return false;
        }
        print_transition_summary("pixel_single_fade_slide",
                                 begin,
                                 committed_trace,
                                 committed_ledger,
                                 env,
                                 &frame);
        return true;
    }

    [[nodiscard]] bool run_pixel_single_fade_slide_cheap_quantized() noexcept {
        TransitionScene env{};
        env.canvas.set_pixel(2, 2, rgba{215, 80, 40, 255});
        PaintPrepare prepare{.canvas = &env.canvas, .color = rgba{40, 120, 220, 255}, .x = 4, .y = 2};
        ui::scene::PageTransitionRunner runner{};
        auto access = env.scene.access();
        const auto begin = runner.begin(env.scene,
                                        access,
                                        fade_slide_transition_spec(env,
                                                                   prepare,
                                                                   pixel_single_budget(),
                                                                   ui::scene::LayerProfile::Cheap));
        if (!expect(begin.started(), "pixel single cheap fade slide starts")) return false;
        if (!expect_pixel_single_admission(begin, "pixel single cheap admission is selected")) {
            return false;
        }
        env.canvas.clear(rgba{0, 0, 0, 255});
        const auto frame = runner.sample(env.scene, 60);
        const auto trace = runner.trace();
        const auto ledger = runner.ledger();
        if (!expect(frame.valid, "pixel single cheap fade slide composes source frame")) return false;
        if (!expect_source_only_frame(frame,
                                      "pixel single cheap fade slide keeps destination live")) {
            return false;
        }
        if (!expect_fade_slide_trace(trace,
                                     ui::scene::LayerProfile::Cheap,
                                     "pixel single cheap trace records recipe")) {
            return false;
        }
        if (!expect(trace.motion.tier == ui::scene::MotionTier::Cheap30Fps &&
                    trace.motion.last_sampled_elapsed_ms == 33,
                    "pixel single cheap quantizes motion time")) {
            return false;
        }
        if (!expect_transform(frame.transition.motion.transform,
                              4,
                              85,
                              "pixel single cheap quantizes transform and opacity")) {
            return false;
        }
        if (!expect_transform(frame.source.plan.transform,
                              4,
                              85,
                              "pixel single cheap source plan uses quantized transform")) {
            return false;
        }
        if (!expect_source_only_layer_cost(ledger,
                                           "pixel single cheap ledger records source-only cost")) {
            return false;
        }
        if (!expect(ledger.source_composite_pixels == 1 &&
                    ledger.destination_composite_pixels == 0,
                    "pixel single cheap ledger records source-only pixels")) {
            return false;
        }
        runner.commit(env.scene, access);
        const auto committed_ledger = runner.ledger();
        if (!expect(runner.idle(), "pixel single cheap fade slide returns idle")) return false;
        if (!expect_snapshot_count(env, 0, "pixel single cheap releases source snapshot")) {
            return false;
        }
        if (!expect(committed_ledger.committed && committed_ledger.snapshots_released,
                    "pixel single cheap ledger records released commit")) {
            return false;
        }
        print_transition_summary("pixel_single_fade_slide_cheap",
                                 begin,
                                 runner.trace(),
                                 committed_ledger,
                                 env,
                                 &frame);
        return true;
    }

    [[nodiscard]] bool run_pixel_single_cancel() noexcept {
        TransitionScene env{};
        env.canvas.set_pixel(2, 2, rgba{210, 90, 40, 255});
        PaintPrepare prepare{.canvas = &env.canvas, .color = rgba{40, 120, 230, 255}, .x = 4, .y = 2};
        ui::scene::PageTransitionRunner runner{};
        auto access = env.scene.access();
        const auto begin = runner.begin(env.scene, access, transition_spec(env, prepare, pixel_single_budget()));
        if (!expect(begin.started(), "pixel single cancel transition starts")) return false;
        if (!expect_pixel_single_admission(begin, "pixel single cancel admission is selected")) {
            return false;
        }
        if (!expect_snapshot_count(env, 1, "pixel single cancel owns one snapshot before abort")) {
            return false;
        }
        if (!expect_page_truth(env, false, true, "pixel single cancel has live destination before abort")) {
            return false;
        }
        const auto frame = runner.sample(env.scene, 0);
        if (!expect_source_only_frame(frame, "pixel single cancel samples source-only frame")) {
            return false;
        }
        runner.cancel(env.scene, access);
        const auto trace = runner.trace();
        const auto ledger = runner.ledger();
        if (!expect(runner.idle(), "pixel single cancel returns idle")) return false;
        if (!expect_page_truth(env, true, false, "pixel single cancel restores page truth")) {
            return false;
        }
        if (!expect_snapshot_count(env, 0, "pixel single cancel releases source snapshot")) {
            return false;
        }
        if (!expect(trace.abort_count == 1 && trace.commit_count == 0,
                    "pixel single cancel trace records abort only")) {
            return false;
        }
        if (!expect(ledger.aborted && !ledger.committed,
                    "pixel single cancel ledger records abort")) {
            return false;
        }
        if (!expect_source_only_layer_cost(ledger,
                                           "pixel single cancel ledger records source-only bytes")) {
            return false;
        }
        if (!expect(ledger.source_composite_pixels == 1 &&
                    ledger.destination_composite_pixels == 0,
                    "pixel single cancel ledger records source-only pixels")) {
            return false;
        }
        if (!expect(ledger.snapshots_released, "pixel single cancel ledger records release")) {
            return false;
        }
        print_transition_summary("pixel_single_cancel", begin, trace, ledger, env, &frame);
        return true;
    }

    [[nodiscard]] bool run_prepare_fail() noexcept {
        TransitionScene env{};
        env.canvas.set_pixel(2, 2, rgba{210, 30, 40, 255});
        PaintPrepare prepare{.canvas = &env.canvas, .color = rgba{20, 90, 230, 255}, .x = 4, .y = 2, .ok = false};
        ui::scene::PageTransitionRunner runner{};
        auto access = env.scene.access();
        const auto begin = runner.begin(env.scene, access, transition_spec(env, prepare));
        const auto trace = runner.trace();
        const auto ledger = runner.ledger();
        if (!expect(begin.status == ui::scene::PageTransitionBeginStatus::PrepareFailed,
                    "prepare failure is reported")) {
            return false;
        }
        if (!expect(trace.source_capture_count == 1 && trace.destination_capture_count == 0,
                    "prepare failure happens after source capture")) {
            return false;
        }
        if (!expect(trace.abort_count == 1, "prepare failure records abort")) return false;
        if (!expect(env.source.visible() && !env.destination.visible(),
                    "prepare failure restores page truth")) {
            return false;
        }
        if (!expect(env.scene.layer_stats().snapshot_count == 0,
                    "prepare failure releases source snapshot")) {
            return false;
        }
        if (!expect(ledger.aborted && !ledger.committed, "prepare failure ledger records abort")) {
            return false;
        }
        if (!expect(ledger.source_bytes == 3 && ledger.destination_bytes == 0,
                    "prepare failure ledger records source-only bytes")) {
            return false;
        }
        print_transition_summary("prepare_fail", begin, trace, ledger, env);
        return true;
    }

    [[nodiscard]] bool run_source_capture_fail() noexcept {
        TransitionScene env{};
        env.canvas.set_pixel(2, 2, rgba{190, 40, 60, 255});
        PaintPrepare prepare{.canvas = &env.canvas, .color = rgba{40, 90, 220, 255}, .x = 4, .y = 2};
        auto spec = transition_spec(env, prepare);
        spec.source_snapshot.bounds = {
            .x = command_snapshot_width + 8,
            .y = command_snapshot_height + 8,
            .w = 1,
            .h = 1,
        };
        ui::scene::PageTransitionRunner runner{};
        auto access = env.scene.access();
        const auto begin = runner.begin(env.scene, access, spec);
        const auto trace = runner.trace();
        const auto ledger = runner.ledger();
        if (!expect(begin.status == ui::scene::PageTransitionBeginStatus::SourceCaptureFailed,
                    "source capture failure is reported")) {
            return false;
        }
        if (!expect(begin.source_capture.status == ui::scene::LayerCaptureStatus::StoreFailed,
                    "source capture failure keeps capture status")) {
            return false;
        }
        if (!expect(trace.begin_status == ui::scene::PageTransitionBeginStatus::SourceCaptureFailed,
                    "source capture failure is recorded in trace")) {
            return false;
        }
        if (!expect(trace.source_capture_count == 0 && trace.destination_capture_count == 0,
                    "source capture failure performs no successful captures")) {
            return false;
        }
        if (!expect(trace.abort_count == 1, "source capture failure records abort")) return false;
        if (!expect(env.source.visible() && !env.destination.visible(),
                    "source capture failure restores page truth")) {
            return false;
        }
        if (!expect(env.scene.layer_stats().snapshot_count == 0,
                    "source capture failure leaves no snapshots")) {
            return false;
        }
        if (!expect(ledger.aborted && ledger.peak_layer_bytes == 0,
                    "source capture failure ledger records no captured bytes")) {
            return false;
        }
        print_transition_summary("source_capture_fail", begin, trace, ledger, env);
        return true;
    }

    [[nodiscard]] bool run_destination_capture_fail() noexcept {
        TransitionScene env{};
        env.canvas.set_pixel(2, 2, rgba{180, 50, 70, 255});
        PaintPrepare prepare{.canvas = &env.canvas, .color = rgba{50, 100, 230, 255}, .x = 4, .y = 2};
        auto spec = transition_spec(env, prepare);
        spec.destination_snapshot.bounds = {
            .x = command_snapshot_width + 8,
            .y = command_snapshot_height + 8,
            .w = 1,
            .h = 1,
        };
        ui::scene::PageTransitionRunner runner{};
        auto access = env.scene.access();
        const auto begin = runner.begin(env.scene, access, spec);
        const auto trace = runner.trace();
        const auto ledger = runner.ledger();
        if (!expect(begin.status == ui::scene::PageTransitionBeginStatus::DestinationCaptureFailed,
                    "destination capture failure is reported")) {
            return false;
        }
        if (!expect(begin.source_capture.ok(), "destination capture failure keeps source capture result")) {
            return false;
        }
        if (!expect(begin.destination_capture.status == ui::scene::LayerCaptureStatus::StoreFailed,
                    "destination capture failure keeps capture status")) {
            return false;
        }
        if (!expect(trace.begin_status == ui::scene::PageTransitionBeginStatus::DestinationCaptureFailed,
                    "destination capture failure is recorded in trace")) {
            return false;
        }
        if (!expect(trace.source_capture_count == 1 && trace.destination_capture_count == 0,
                    "destination capture failure happens after source capture")) {
            return false;
        }
        if (!expect(trace.abort_count == 1, "destination capture failure records abort")) return false;
        if (!expect(env.source.visible() && !env.destination.visible(),
                    "destination capture failure restores page truth")) {
            return false;
        }
        if (!expect(env.scene.layer_stats().snapshot_count == 0,
                    "destination capture failure releases source snapshot")) {
            return false;
        }
        if (!expect(ledger.aborted && ledger.source_bytes == 3 && ledger.destination_bytes == 0,
                    "destination capture failure ledger records source-only bytes")) {
            return false;
        }
        print_transition_summary("destination_capture_fail", begin, trace, ledger, env);
        return true;
    }

    [[nodiscard]] bool run_rebegin_interrupt() noexcept {
        TransitionScene env{};
        env.canvas.set_pixel(2, 2, rgba{210, 80, 30, 255});
        PaintPrepare first_prepare{.canvas = &env.canvas, .color = rgba{30, 80, 210, 255}, .x = 4, .y = 2};
        PaintPrepare second_prepare{.canvas = &env.canvas, .color = rgba{60, 120, 240, 255}, .x = 4, .y = 2};
        ui::scene::PageTransitionRunner runner{};
        auto access = env.scene.access();
        const auto first = runner.begin(env.scene, access, transition_spec(env, first_prepare));
        if (!expect(first.started(), "rebegin first transition starts")) return false;
        const auto first_frame = runner.sample(env.scene, 0);
        if (!expect(first_frame.valid, "rebegin first transition composes")) return false;
        if (!expect(env.scene.layer_stats().snapshot_count == 2,
                    "rebegin first transition owns two snapshots")) {
            return false;
        }
        const auto second = runner.begin(env.scene, access, transition_spec(env, second_prepare));
        const auto trace = runner.trace();
        if (!expect(second.started(), "rebegin starts replacement transition")) return false;
        if (!expect(trace.interrupt_count == 1, "rebegin trace records interrupt")) return false;
        if (!expect(env.scene.layer_stats().snapshot_count == 2,
                    "rebegin releases old snapshots before reacquiring")) {
            return false;
        }
        if (!expect(!env.source.visible() && !env.destination.visible(),
                    "rebegin replacement transition owns hidden live roots")) {
            return false;
        }
        runner.cancel(env.scene, access);
        const auto canceled_trace = runner.trace();
        const auto ledger = runner.ledger();
        if (!expect(canceled_trace.interrupt_count == 1,
                    "rebegin interrupt count survives cancel")) {
            return false;
        }
        if (!expect(env.source.visible() && !env.destination.visible(),
                    "rebegin cancel restores original page truth")) {
            return false;
        }
        if (!expect(env.scene.layer_stats().snapshot_count == 0,
                    "rebegin cancel releases replacement snapshots")) {
            return false;
        }
        if (!expect(ledger.interrupted && ledger.aborted, "rebegin ledger records interrupt abort")) {
            return false;
        }
        if (!expect(ledger.peak_layer_bytes == 6 && ledger.snapshots_released,
                    "rebegin ledger records replacement layer cost")) {
            return false;
        }
        print_transition_summary("rebegin", second, canceled_trace, ledger, env);
        return true;
    }

    [[nodiscard]] bool run_pixel_single_rebegin_interrupt() noexcept {
        TransitionScene env{};
        env.canvas.set_pixel(2, 2, rgba{230, 100, 30, 255});
        PaintPrepare first_prepare{.canvas = &env.canvas, .color = rgba{30, 100, 230, 255}, .x = 4, .y = 2};
        PaintPrepare second_prepare{.canvas = &env.canvas, .color = rgba{70, 140, 240, 255}, .x = 4, .y = 2};
        ui::scene::PageTransitionRunner runner{};
        auto access = env.scene.access();
        const auto budget = pixel_single_budget();
        const auto first = runner.begin(env.scene, access, transition_spec(env, first_prepare, budget));
        if (!expect(first.started(), "pixel single rebegin first transition starts")) return false;
        if (!expect_pixel_single_admission(first,
                                           "pixel single rebegin first admission is selected")) {
            return false;
        }
        const auto first_frame = runner.sample(env.scene, 0);
        if (!expect_source_only_frame(first_frame,
                                      "pixel single rebegin first transition composes source-only")) {
            return false;
        }
        if (!expect_snapshot_count(env, 1,
                                   "pixel single rebegin first transition owns one snapshot")) {
            return false;
        }
        const auto second = runner.begin(env.scene, access, transition_spec(env, second_prepare, budget));
        const auto trace = runner.trace();
        if (!expect(second.started(), "pixel single rebegin starts replacement transition")) return false;
        if (!expect_pixel_single_admission(second,
                                           "pixel single rebegin replacement admission is selected")) {
            return false;
        }
        if (!expect(trace.interrupt_count == 1, "pixel single rebegin trace records interrupt")) {
            return false;
        }
        if (!expect_source_only_capture(trace,
                                        "pixel single rebegin replacement captures source only")) {
            return false;
        }
        if (!expect_snapshot_count(env, 1,
                                   "pixel single rebegin releases old snapshot before reacquiring")) {
            return false;
        }
        if (!expect_page_truth(env, false, true,
                               "pixel single rebegin replacement keeps destination live")) {
            return false;
        }
        runner.cancel(env.scene, access);
        const auto canceled_trace = runner.trace();
        const auto ledger = runner.ledger();
        if (!expect(canceled_trace.interrupt_count == 1,
                    "pixel single rebegin interrupt count survives cancel")) {
            return false;
        }
        if (!expect_page_truth(env, true, false,
                               "pixel single rebegin cancel restores original page truth")) {
            return false;
        }
        if (!expect_snapshot_count(env, 0,
                                   "pixel single rebegin cancel releases replacement snapshot")) {
            return false;
        }
        if (!expect(ledger.interrupted && ledger.aborted,
                    "pixel single rebegin ledger records interrupt abort")) {
            return false;
        }
        if (!expect_source_only_layer_cost(ledger,
                                           "pixel single rebegin ledger records source-only layer cost")) {
            return false;
        }
        if (!expect(ledger.snapshots_released,
                    "pixel single rebegin ledger records released snapshot")) {
            return false;
        }
        print_transition_summary("pixel_single_rebegin", second, canceled_trace, ledger, env);
        return true;
    }
}

int main() {
    print_transition_run_begin();
    bool ok = true;
    do {
        if constexpr (kCommandOnlyStorage) {
            if (!run_command_snapshot_slide()) { ok = false; break; }
            if (!run_command_fallback_static_cut()) { ok = false; break; }
            if (!run_command_epoch_fallback_static_cut()) { ok = false; break; }
            if (!run_command_opacity_replay()) { ok = false; break; }
            break;
        }
        if (!run_normal_commit()) { ok = false; break; }
        if (!run_fade_slide_pixel_double()) { ok = false; break; }
        if (!run_fade_slide_cheap_quantized()) { ok = false; break; }
        if (!run_cancel_during_compose()) { ok = false; break; }
        if constexpr (::snapshot_command_enabled) {
            if (!run_command_snapshot_slide()) { ok = false; break; }
        }
        if (!run_command_fallback_static_cut()) { ok = false; break; }
        if constexpr (::snapshot_command_enabled) {
            if (!run_command_epoch_fallback_static_cut()) { ok = false; break; }
        }
        if (!run_static_profile_static_cut()) { ok = false; break; }
        if (!run_none_profile_reject()) { ok = false; break; }
        if (!run_pixel_single_fade_slide_live_destination()) { ok = false; break; }
        if (!run_pixel_single_fade_slide_cheap_quantized()) { ok = false; break; }
        if (!run_pixel_single_cancel()) { ok = false; break; }
        if (!run_prepare_fail()) { ok = false; break; }
        if (!run_source_capture_fail()) { ok = false; break; }
        if (!run_destination_capture_fail()) { ok = false; break; }
        if (!run_rebegin_interrupt()) { ok = false; break; }
        if (!run_pixel_single_rebegin_interrupt()) { ok = false; break; }
    } while (false);
    if (ok && !run_causal_chain_verdict()) ok = false;
    print_transition_run_end(ok);
    if (ok) std::puts("[page_transition_demo] ok");
    return ok ? 0 : 1;
}
