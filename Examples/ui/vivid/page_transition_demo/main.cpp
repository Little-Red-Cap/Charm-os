#include <cstdio>

import charm.gfx.canvas;
import charm.ui.vivid;

namespace {
    struct TransitionScene {
        DefaultFrameBuffer fb{};
        DefaultCanvas canvas{fb};
        ui::scene::Scene scene{canvas};
        WidgetHandle root{};
        WidgetHandle source_root{};
        WidgetHandle destination_root{};
        ui::scene::PageLayer source{};
        ui::scene::PageLayer destination{};

        TransitionScene() {
            scene.build([&](ui::scene::SceneBuilder& builder) {
                root = builder.create_container();
                source_root = builder.create_container();
                destination_root = builder.create_container();
                builder.set_rect(root, {0, 0, 32, 32});
                builder.set_rect(source_root, {0, 0, 32, 32});
                builder.set_rect(destination_root, {0, 0, 32, 32});
                builder.link(root, source_root);
                builder.link(root, destination_root);
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
        DefaultCanvas* canvas{nullptr};
        rgba color{};
        int x{0};
        int y{0};
        bool ok{true};
    };

    [[nodiscard]] bool expect(bool condition, const char* message) noexcept {
        if (!condition) {
            std::printf("[ERR] %s\n", message);
            return false;
        }
        return true;
    }

    bool prepare_destination(ui::scene::Scene&,
                             ui::scene::SceneAccess,
                             ui::scene::PageLayer&,
                             void* ctx) noexcept {
        auto* prepare = static_cast<PaintPrepare*>(ctx);
        if (!prepare || !prepare->ok) return false;
        prepare->canvas->set_pixel(prepare->x, prepare->y, prepare->color);
        return true;
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
        std::printf("[pt] normal status=%s admission=%s snapshots=%u commit=%u bytes=%u pixels=%u\n",
                    ui::scene::page_transition_begin_status_name(begin.status),
                    ui::scene::layer_admission_name(begin.admission),
                    static_cast<unsigned>(env.scene.layer_stats().snapshot_count),
                    static_cast<unsigned>(trace.commit_count),
                    static_cast<unsigned>(ledger.peak_layer_bytes),
                    static_cast<unsigned>(ledger.total_composite_pixels));
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
        std::printf("[pt] cancel status=%s samples=%u abort=%u snapshots=%u pixels=%u\n",
                    ui::scene::page_transition_begin_status_name(begin.status),
                    static_cast<unsigned>(trace.sample_count),
                    static_cast<unsigned>(trace.abort_count),
                    static_cast<unsigned>(env.scene.layer_stats().snapshot_count),
                    static_cast<unsigned>(ledger.total_composite_pixels));
        return true;
    }

    [[nodiscard]] bool run_low_budget_static_cut() noexcept {
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
        if (!expect(begin.static_cut(), "low budget resolves static cut")) return false;
        if (!expect(begin.admission != ui::scene::LayerAdmission::PixelDouble,
                    "low budget rejects PixelDouble admission")) {
            return false;
        }
        if (!expect(trace.source_capture_count == 0 && trace.destination_capture_count == 0,
                    "low budget performs no pixel captures")) {
            return false;
        }
        if (!expect(!env.source.visible() && env.destination.visible(),
                    "low budget cut commits page truth")) {
            return false;
        }
        if (!expect(env.scene.layer_stats().snapshot_count == 0,
                    "low budget leaves no snapshots")) {
            return false;
        }
        if (!expect(ledger.static_cut && ledger.committed, "low budget ledger records static cut commit")) {
            return false;
        }
        if (!expect(ledger.peak_layer_bytes == 0 && ledger.total_composite_pixels == 0,
                    "low budget ledger records no layer cost")) {
            return false;
        }
        std::printf("[pt] low_budget status=%s admission=%s static_cut=%u snapshots=%u bytes=%u\n",
                    ui::scene::page_transition_begin_status_name(begin.status),
                    ui::scene::layer_admission_name(begin.admission),
                    static_cast<unsigned>(trace.static_cut_count),
                    static_cast<unsigned>(env.scene.layer_stats().snapshot_count),
                    static_cast<unsigned>(ledger.peak_layer_bytes));
        return true;
    }

    [[nodiscard]] bool run_pixel_single_live_destination() noexcept {
        TransitionScene env{};
        env.canvas.set_pixel(2, 2, rgba{220, 70, 30, 255});
        PaintPrepare prepare{.canvas = &env.canvas, .color = rgba{30, 110, 230, 255}, .x = 4, .y = 2};
        ui::scene::PageTransitionRunner runner{};
        auto access = env.scene.access();
        const auto begin = runner.begin(env.scene, access, transition_spec(env, prepare, {
            .max_layer_bytes = 3,
        }));
        const auto trace = runner.trace();
        const auto ledger = runner.ledger();
        if (!expect(begin.started(), "pixel single transition starts")) return false;
        if (!expect(begin.admission == ui::scene::LayerAdmission::PixelSingle,
                    "pixel single admission is selected")) {
            return false;
        }
        if (!expect(trace.source_capture_count == 1 && trace.destination_capture_count == 0,
                    "pixel single captures source only")) {
            return false;
        }
        if (!expect(env.scene.layer_stats().snapshot_count == 1,
                    "pixel single owns one snapshot during transition")) {
            return false;
        }
        if (!expect(!env.source.visible() && env.destination.visible(),
                    "pixel single keeps destination live during transition")) {
            return false;
        }
        if (!expect(env.destination.live(), "pixel single destination is live")) return false;
        env.canvas.clear(rgba{0, 0, 0, 255});
        const auto frame = runner.sample(env.scene, 0);
        const auto after_sample = runner.ledger();
        if (!expect(frame.valid, "pixel single composes source frame")) return false;
        if (!expect(frame.source.valid && !frame.destination.valid,
                    "pixel single composes source over live destination")) {
            return false;
        }
        const auto source_pixel = env.canvas.get_pixel(7, 2);
        if (!expect(source_pixel.r == 220, "pixel single source snapshot is composed")) return false;
        if (!expect(after_sample.source_bytes == 3 && after_sample.destination_bytes == 0,
                    "pixel single ledger records source-only bytes")) {
            return false;
        }
        if (!expect(after_sample.peak_layer_bytes == 3,
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
        if (!expect(!env.source.visible() && env.destination.visible(),
                    "pixel single commit updates page truth")) {
            return false;
        }
        if (!expect(env.scene.layer_stats().snapshot_count == 0,
                    "pixel single commit releases source snapshot")) {
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
        std::printf("[pt] pixel_single status=%s admission=%s source_caps=%u dst_caps=%u snapshots=%u bytes=%u pixels=%u\n",
                    ui::scene::page_transition_begin_status_name(begin.status),
                    ui::scene::layer_admission_name(begin.admission),
                    static_cast<unsigned>(committed_trace.source_capture_count),
                    static_cast<unsigned>(committed_trace.destination_capture_count),
                    static_cast<unsigned>(env.scene.layer_stats().snapshot_count),
                    static_cast<unsigned>(committed_ledger.peak_layer_bytes),
                    static_cast<unsigned>(committed_ledger.total_composite_pixels));
        return true;
    }

    [[nodiscard]] bool run_pixel_single_cancel() noexcept {
        TransitionScene env{};
        env.canvas.set_pixel(2, 2, rgba{210, 90, 40, 255});
        PaintPrepare prepare{.canvas = &env.canvas, .color = rgba{40, 120, 230, 255}, .x = 4, .y = 2};
        ui::scene::PageTransitionRunner runner{};
        auto access = env.scene.access();
        const auto begin = runner.begin(env.scene, access, transition_spec(env, prepare, {
            .max_layer_bytes = 3,
        }));
        if (!expect(begin.started(), "pixel single cancel transition starts")) return false;
        if (!expect(begin.admission == ui::scene::LayerAdmission::PixelSingle,
                    "pixel single cancel admission is selected")) {
            return false;
        }
        if (!expect(env.scene.layer_stats().snapshot_count == 1,
                    "pixel single cancel owns one snapshot before abort")) {
            return false;
        }
        if (!expect(!env.source.visible() && env.destination.visible(),
                    "pixel single cancel has live destination before abort")) {
            return false;
        }
        const auto frame = runner.sample(env.scene, 0);
        if (!expect(frame.source.valid && !frame.destination.valid,
                    "pixel single cancel samples source-only frame")) {
            return false;
        }
        runner.cancel(env.scene, access);
        const auto trace = runner.trace();
        const auto ledger = runner.ledger();
        if (!expect(runner.idle(), "pixel single cancel returns idle")) return false;
        if (!expect(env.source.visible() && !env.destination.visible(),
                    "pixel single cancel restores page truth")) {
            return false;
        }
        if (!expect(env.scene.layer_stats().snapshot_count == 0,
                    "pixel single cancel releases source snapshot")) {
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
        if (!expect(ledger.source_bytes == 3 && ledger.destination_bytes == 0,
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
        std::printf("[pt] pixel_single_cancel status=%s admission=%s abort=%u snapshots=%u bytes=%u pixels=%u\n",
                    ui::scene::page_transition_begin_status_name(begin.status),
                    ui::scene::layer_admission_name(begin.admission),
                    static_cast<unsigned>(trace.abort_count),
                    static_cast<unsigned>(env.scene.layer_stats().snapshot_count),
                    static_cast<unsigned>(ledger.peak_layer_bytes),
                    static_cast<unsigned>(ledger.total_composite_pixels));
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
        std::printf("[pt] prepare_fail status=%s source_caps=%u abort=%u snapshots=%u bytes=%u\n",
                    ui::scene::page_transition_begin_status_name(begin.status),
                    static_cast<unsigned>(trace.source_capture_count),
                    static_cast<unsigned>(trace.abort_count),
                    static_cast<unsigned>(env.scene.layer_stats().snapshot_count),
                    static_cast<unsigned>(ledger.peak_layer_bytes));
        return true;
    }

    [[nodiscard]] bool run_source_capture_fail() noexcept {
        TransitionScene env{};
        env.canvas.set_pixel(2, 2, rgba{190, 40, 60, 255});
        PaintPrepare prepare{.canvas = &env.canvas, .color = rgba{40, 90, 220, 255}, .x = 4, .y = 2};
        auto spec = transition_spec(env, prepare);
        spec.source_snapshot.bounds = {.x = 40, .y = 40, .w = 1, .h = 1};
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
        std::printf("[pt] source_capture_fail status=%s capture=%u abort=%u snapshots=%u bytes=%u\n",
                    ui::scene::page_transition_begin_status_name(begin.status),
                    static_cast<unsigned>(begin.source_capture.status),
                    static_cast<unsigned>(trace.abort_count),
                    static_cast<unsigned>(env.scene.layer_stats().snapshot_count),
                    static_cast<unsigned>(ledger.peak_layer_bytes));
        return true;
    }

    [[nodiscard]] bool run_destination_capture_fail() noexcept {
        TransitionScene env{};
        env.canvas.set_pixel(2, 2, rgba{180, 50, 70, 255});
        PaintPrepare prepare{.canvas = &env.canvas, .color = rgba{50, 100, 230, 255}, .x = 4, .y = 2};
        auto spec = transition_spec(env, prepare);
        spec.destination_snapshot.bounds = {.x = 40, .y = 40, .w = 1, .h = 1};
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
        std::printf("[pt] destination_capture_fail status=%s src_caps=%u dst_capture=%u snapshots=%u bytes=%u\n",
                    ui::scene::page_transition_begin_status_name(begin.status),
                    static_cast<unsigned>(trace.source_capture_count),
                    static_cast<unsigned>(begin.destination_capture.status),
                    static_cast<unsigned>(env.scene.layer_stats().snapshot_count),
                    static_cast<unsigned>(ledger.peak_layer_bytes));
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
        std::printf("[pt] rebegin first=%s second=%s interrupts=%u snapshots=%u bytes=%u\n",
                    ui::scene::page_transition_begin_status_name(first.status),
                    ui::scene::page_transition_begin_status_name(second.status),
                    static_cast<unsigned>(canceled_trace.interrupt_count),
                    static_cast<unsigned>(env.scene.layer_stats().snapshot_count),
                    static_cast<unsigned>(ledger.peak_layer_bytes));
        return true;
    }

    [[nodiscard]] bool run_pixel_single_rebegin_interrupt() noexcept {
        TransitionScene env{};
        env.canvas.set_pixel(2, 2, rgba{230, 100, 30, 255});
        PaintPrepare first_prepare{.canvas = &env.canvas, .color = rgba{30, 100, 230, 255}, .x = 4, .y = 2};
        PaintPrepare second_prepare{.canvas = &env.canvas, .color = rgba{70, 140, 240, 255}, .x = 4, .y = 2};
        ui::scene::PageTransitionRunner runner{};
        auto access = env.scene.access();
        const auto budget = ui::scene::LayerBudget{.max_layer_bytes = 3};
        const auto first = runner.begin(env.scene, access, transition_spec(env, first_prepare, budget));
        if (!expect(first.started(), "pixel single rebegin first transition starts")) return false;
        if (!expect(first.admission == ui::scene::LayerAdmission::PixelSingle,
                    "pixel single rebegin first admission is selected")) {
            return false;
        }
        const auto first_frame = runner.sample(env.scene, 0);
        if (!expect(first_frame.source.valid && !first_frame.destination.valid,
                    "pixel single rebegin first transition composes source-only")) {
            return false;
        }
        if (!expect(env.scene.layer_stats().snapshot_count == 1,
                    "pixel single rebegin first transition owns one snapshot")) {
            return false;
        }
        const auto second = runner.begin(env.scene, access, transition_spec(env, second_prepare, budget));
        const auto trace = runner.trace();
        if (!expect(second.started(), "pixel single rebegin starts replacement transition")) return false;
        if (!expect(second.admission == ui::scene::LayerAdmission::PixelSingle,
                    "pixel single rebegin replacement admission is selected")) {
            return false;
        }
        if (!expect(trace.interrupt_count == 1, "pixel single rebegin trace records interrupt")) {
            return false;
        }
        if (!expect(trace.source_capture_count == 1 && trace.destination_capture_count == 0,
                    "pixel single rebegin replacement captures source only")) {
            return false;
        }
        if (!expect(env.scene.layer_stats().snapshot_count == 1,
                    "pixel single rebegin releases old snapshot before reacquiring")) {
            return false;
        }
        if (!expect(!env.source.visible() && env.destination.visible(),
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
        if (!expect(env.source.visible() && !env.destination.visible(),
                    "pixel single rebegin cancel restores original page truth")) {
            return false;
        }
        if (!expect(env.scene.layer_stats().snapshot_count == 0,
                    "pixel single rebegin cancel releases replacement snapshot")) {
            return false;
        }
        if (!expect(ledger.interrupted && ledger.aborted,
                    "pixel single rebegin ledger records interrupt abort")) {
            return false;
        }
        if (!expect(ledger.source_bytes == 3 && ledger.destination_bytes == 0 &&
                    ledger.peak_layer_bytes == 3,
                    "pixel single rebegin ledger records source-only layer cost")) {
            return false;
        }
        if (!expect(ledger.snapshots_released,
                    "pixel single rebegin ledger records released snapshot")) {
            return false;
        }
        std::printf("[pt] pixel_single_rebegin first=%s second=%s interrupts=%u snapshots=%u bytes=%u\n",
                    ui::scene::page_transition_begin_status_name(first.status),
                    ui::scene::page_transition_begin_status_name(second.status),
                    static_cast<unsigned>(canceled_trace.interrupt_count),
                    static_cast<unsigned>(env.scene.layer_stats().snapshot_count),
                    static_cast<unsigned>(ledger.peak_layer_bytes));
        return true;
    }
}

int main() {
    if (!run_normal_commit()) return 1;
    if (!run_cancel_during_compose()) return 1;
    if (!run_low_budget_static_cut()) return 1;
    if (!run_pixel_single_live_destination()) return 1;
    if (!run_pixel_single_cancel()) return 1;
    if (!run_prepare_fail()) return 1;
    if (!run_source_capture_fail()) return 1;
    if (!run_destination_capture_fail()) return 1;
    if (!run_rebegin_interrupt()) return 1;
    if (!run_pixel_single_rebegin_interrupt()) return 1;
    std::puts("[page_transition_demo] ok");
    return 0;
}
