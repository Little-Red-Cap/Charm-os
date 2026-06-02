#include <array>
#include <cstddef>
#include <cstdio>

import charm.gfx.canvas;
import charm.ui.scene.motion_runtime;
import charm.ui.vivid;

#include "../support/vivid_evidence_support.hpp"

namespace {
    constexpr Rect kRootBounds{0, 0, 32, 32};
    constexpr Rect kActionBounds{1, 1, 12, 8};
    constexpr unsigned kSemanticTransitionCases = 8;

    struct TransitionScene {
        DefaultFrameBuffer fb{};
        DefaultCanvas canvas{fb};
        ui::scene::Scene scene{canvas};
        WidgetHandle root{};
        WidgetHandle source_root{};
        WidgetHandle destination_root{};
        WidgetHandle action{};
        ui::scene::PageLayer source{};
        ui::scene::PageLayer destination{};

        TransitionScene() {
            scene.build([&](ui::scene::SceneBuilder& builder) {
                root = builder.create_container();
                source_root = builder.create_container();
                destination_root = builder.create_container();
                action = builder.create_button_static("Open");

                builder.set_rect(root, kRootBounds);
                builder.set_rect(source_root, kRootBounds);
                builder.set_rect(destination_root, kRootBounds);
                builder.set_rect(action, kActionBounds);

                builder.link(root, source_root);
                builder.link(root, destination_root);
                builder.link(source_root, action);

                builder.set_semantic(source_root, SemanticRole::Container, "page.now", "Now page");
                builder.set_semantic(destination_root, SemanticRole::Container, "page.library", "Library page");
                builder.set_semantic_default(action, "nav.library.open", "Open library");
                builder.set_input_root(root);
                builder.set_focus_scope(source_root, action, true);
                builder.set_root(root);
            });

            source.set_root(source_root);
            destination.set_root(destination_root);
            auto access = scene.access();
            source.show(access);
            destination.hide(access);
            access.set_focusable(action, true);
            canvas.clear(rgba{0, 0, 0, 255});
            canvas.set_pixel(2, 2, rgba{230, 20, 30, 255});
        }
    };

    struct PaintPrepare {
        DefaultCanvas* canvas{nullptr};
        rgba color{};
        int x{0};
        int y{0};
        bool ok{true};
    };

    struct EvidenceVerdict {
        bool request_ok{false};
        bool event_ok{false};
        bool admission_ok{false};
        bool commit_ok{false};
        bool snapshot_lifecycle_ok{false};
        bool page_truth_ok{false};
        bool rejected_no_mutation{false};
    };

    bool prepare_destination(ui::scene::Scene&,
                             ui::scene::SceneAccess,
                             ui::scene::PageLayer&,
                             void* ctx) noexcept {
        auto* prepare = static_cast<PaintPrepare*>(ctx);
        if (!prepare || !prepare->ok) return false;
        prepare->canvas->set_pixel(prepare->x, prepare->y, prepare->color);
        return true;
    }

    [[nodiscard]] ui::scene::PageTransitionSpec transition_spec(
        TransitionScene& env,
        PaintPrepare& prepare) noexcept {
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
            .requested_profile = ui::scene::LayerProfile::Rich,
            .start_ms = 0,
            .hide_source_live_root = true,
            .hide_destination_live_root = true,
            .prepare_destination = prepare_destination,
            .prepare_ctx = &prepare,
        };
    }

    [[nodiscard]] bool page_truth(const TransitionScene& env,
                                  bool source_visible,
                                  bool destination_visible) noexcept {
        return env.source.visible() == source_visible &&
            env.destination.visible() == destination_visible;
    }

    [[nodiscard]] bool request_executed(const SemanticActionRequest& request) noexcept {
        return request.status == SemanticActionRequestStatus::Executed
            && request.reject_reason == SemanticActionRequestRejectReason::None
            && request.emitted_click
            && request.focus_ready;
    }

    [[nodiscard]] bool request_rejected_without_click(const SemanticActionRequest& request) noexcept {
        return request.status == SemanticActionRequestStatus::Rejected
            && request.reject_reason == SemanticActionRequestRejectReason::ActionAdmissionRejected
            && request.admission.status == SemanticActionAdmissionStatus::Disabled
            && !request.emitted_click
            && !request.executed;
    }

    [[nodiscard]] bool run_positive_path(vivid::evidence::RunLog& run_log,
                                         EvidenceVerdict& verdict) {
        TransitionScene env{};
        auto access = env.scene.access();
        const auto focus_setup = env.scene.request_semantic_focus(env.source_root, "nav.library.open");
        if (!vivid::evidence::expect(focus_setup.status == SemanticFocusRequestStatus::Committed
                                     && vivid::evidence::same_handle(access.input_focused(), env.action),
                                     "setup focuses semantic transition action")) {
            return false;
        }

        run_log.case_begin("baseline_page_truth");
        std::printf(" source_visible=%d destination_visible=%d snapshots=%u\n",
                    env.source.visible() ? 1 : 0,
                    env.destination.visible() ? 1 : 0,
                    static_cast<unsigned>(env.scene.layer_stats().snapshot_count));
        if (!vivid::evidence::expect(page_truth(env, true, false)
                                     && env.scene.layer_stats().snapshot_count == 0,
                                     "baseline page truth starts at source page")) {
            return false;
        }

        const auto request =
            env.scene.request_semantic_action(env.source_root, "nav.library.open", SemanticAction::Activate);
        run_log.case_begin("semantic_request_ledger");
        vivid::evidence::print_action_request_ledger(request);
        std::printf("\n");
        if (!vivid::evidence::expect(request_executed(request),
                                     "semantic transition request executes and emits click")) {
            return false;
        }

        const auto click_count =
            vivid::evidence::count_click_events_since(access, env.action, request.events_before);
        run_log.case_begin("request_event_trace");
        std::printf(" emitted_click=%d click=%zu events_before=%zu events_after=%zu focus_ready=%d bridge_ready=%d\n",
                    request.emitted_click ? 1 : 0,
                    click_count,
                    request.events_before,
                    request.events_after,
                    request.focus_ready ? 1 : 0,
                    request_executed(request) ? 1 : 0);
        if (!vivid::evidence::expect(click_count == 1 && request_executed(request),
                                     "semantic request emits one bridgeable click")) {
            return false;
        }

        ui::scene::PageTransitionRunner runner{};
        PaintPrepare prepare{.canvas = &env.canvas, .color = rgba{20, 40, 220, 255}, .x = 4, .y = 2};
        ui::scene::PageTransitionBeginResult begin{};
        if (request_executed(request)) {
            begin = runner.begin(env.scene, access, transition_spec(env, prepare));
        }

        run_log.case_begin("transition_begin");
        std::printf(" bridge_started=%d status=%s admission=%s snapshots=%u source_visible=%d destination_visible=%d\n",
                    request_executed(request) ? 1 : 0,
                    ui::scene::page_transition_begin_status_name(begin.status),
                    ui::scene::layer_admission_name(begin.admission),
                    static_cast<unsigned>(env.scene.layer_stats().snapshot_count),
                    env.source.visible() ? 1 : 0,
                    env.destination.visible() ? 1 : 0);
        if (!vivid::evidence::expect(begin.started()
                                     && begin.admission == ui::scene::LayerAdmission::PixelDouble
                                     && env.scene.layer_stats().snapshot_count == 2
                                     && page_truth(env, false, false),
                                     "semantic bridge starts PixelDouble transition")) {
            return false;
        }

        env.canvas.clear(rgba{0, 0, 0, 255});
        const auto frame = runner.sample(env.scene, 0);
        const auto source_pixel = env.canvas.get_pixel(7, 2);
        const auto destination_pixel = env.canvas.get_pixel(4, 2);
        run_log.case_begin("transition_sample");
        std::printf(" valid=%d source_valid=%d destination_valid=%d source_r=%u destination_b=%u pixels=%u\n",
                    frame.valid ? 1 : 0,
                    frame.source.valid ? 1 : 0,
                    frame.destination.valid ? 1 : 0,
                    static_cast<unsigned>(source_pixel.r),
                    static_cast<unsigned>(destination_pixel.b),
                    static_cast<unsigned>(runner.ledger().total_composite_pixels));
        if (!vivid::evidence::expect(frame.valid
                                     && frame.source.valid
                                     && frame.destination.valid
                                     && source_pixel.r == 230
                                     && destination_pixel.b == 220,
                                     "semantic transition sample composes both pages")) {
            return false;
        }

        const auto done = runner.sample(env.scene, 120);
        runner.commit(env.scene, access);
        const auto trace = runner.trace();
        const auto ledger = runner.ledger();
        run_log.case_begin("transition_commit");
        std::printf(" done=%d commits=%u aborts=%u source_visible=%d destination_visible=%d committed=%d\n",
                    done.transition.state == ui::scene::MotionTransitionState::Finished ? 1 : 0,
                    static_cast<unsigned>(trace.commit_count),
                    static_cast<unsigned>(trace.abort_count),
                    env.source.visible() ? 1 : 0,
                    env.destination.visible() ? 1 : 0,
                    ledger.committed ? 1 : 0);
        if (!vivid::evidence::expect(done.transition.state == ui::scene::MotionTransitionState::Finished
                                     && trace.commit_count == 1
                                     && trace.abort_count == 0
                                     && ledger.committed
                                     && page_truth(env, false, true),
                                     "semantic transition commits destination page truth")) {
            return false;
        }

        run_log.case_begin("snapshot_lifecycle");
        std::printf(" snapshots=%u released=%d source_caps=%u destination_caps=%u peak_bytes=%u pixels=%u\n",
                    static_cast<unsigned>(env.scene.layer_stats().snapshot_count),
                    ledger.snapshots_released ? 1 : 0,
                    static_cast<unsigned>(trace.source_capture_count),
                    static_cast<unsigned>(trace.destination_capture_count),
                    static_cast<unsigned>(ledger.peak_layer_bytes),
                    static_cast<unsigned>(ledger.total_composite_pixels));
        if (!vivid::evidence::expect(env.scene.layer_stats().snapshot_count == 0
                                     && ledger.snapshots_released
                                     && trace.source_capture_count == 1
                                     && trace.destination_capture_count == 1,
                                     "semantic transition releases owned snapshots")) {
            return false;
        }

        verdict.request_ok = request_executed(request);
        verdict.event_ok = click_count == 1 && request.emitted_click;
        verdict.admission_ok = begin.admission == ui::scene::LayerAdmission::PixelDouble;
        verdict.commit_ok = ledger.committed && trace.commit_count == 1 && trace.abort_count == 0;
        verdict.snapshot_lifecycle_ok = env.scene.layer_stats().snapshot_count == 0 && ledger.snapshots_released;
        verdict.page_truth_ok = page_truth(env, false, true);
        return true;
    }

    [[nodiscard]] bool run_rejected_path(vivid::evidence::RunLog& run_log,
                                         EvidenceVerdict& verdict) {
    TransitionScene rejected_env{};
    auto rejected_access = rejected_env.scene.access();
    const auto rejected_focus =
        rejected_env.scene.request_semantic_focus(rejected_env.source_root, "nav.library.open");
    if (!vivid::evidence::expect(rejected_focus.status == SemanticFocusRequestStatus::Committed,
                                 "rejected scenario focuses semantic action")) {
        return false;
    }
    rejected_access.set_enabled(rejected_env.action, false);
    const bool rejected_source_before = rejected_env.source.visible();
    const bool rejected_destination_before = rejected_env.destination.visible();
    const auto rejected_request =
        rejected_env.scene.request_semantic_action(rejected_env.source_root, "nav.library.open", SemanticAction::Activate);
    ui::scene::PageTransitionRunner rejected_runner{};
    const bool rejected_bridge_started = request_executed(rejected_request);
    if (rejected_bridge_started) {
        PaintPrepare rejected_prepare{.canvas = &rejected_env.canvas, .color = rgba{20, 40, 220, 255}, .x = 4, .y = 2};
        (void)rejected_runner.begin(rejected_env.scene, rejected_access, transition_spec(rejected_env, rejected_prepare));
    }
    run_log.case_begin("rejected_request_no_transition");
    vivid::evidence::print_action_request_ledger(rejected_request);
    std::printf(" bridge_started=%d source_before=%d destination_before=%d source_after=%d destination_after=%d snapshots=%u\n",
                rejected_bridge_started ? 1 : 0,
                rejected_source_before ? 1 : 0,
                rejected_destination_before ? 1 : 0,
                rejected_env.source.visible() ? 1 : 0,
                rejected_env.destination.visible() ? 1 : 0,
                static_cast<unsigned>(rejected_env.scene.layer_stats().snapshot_count));
    const bool rejected_no_mutation = request_rejected_without_click(rejected_request)
        && !rejected_bridge_started
        && rejected_env.scene.layer_stats().snapshot_count == 0
        && rejected_env.source.visible() == rejected_source_before
        && rejected_env.destination.visible() == rejected_destination_before;
    if (!vivid::evidence::expect(rejected_no_mutation,
                                 "rejected semantic action does not start transition")) {
        return false;
    }
        verdict.rejected_no_mutation = rejected_no_mutation;
        return true;
    }
}

int main() {
    auto run_log = vivid::evidence::RunLog{"stx", "semantic_transition_demo"};
    run_log.begin();

    EvidenceVerdict verdict{};
    if (!run_positive_path(run_log, verdict)) {
        return 1;
    }
    if (!run_rejected_path(run_log, verdict)) {
        return 1;
    }

    const bool chain_ok = verdict.request_ok
        && verdict.event_ok
        && verdict.admission_ok
        && verdict.commit_ok
        && verdict.snapshot_lifecycle_ok
        && verdict.page_truth_ok
        && verdict.rejected_no_mutation;

    const std::array<vivid::evidence::CausalVerdictField, 7> fields{{
        {"request_ok", verdict.request_ok},
        {"event_ok", verdict.event_ok},
        {"admission_ok", verdict.admission_ok},
        {"commit_ok", verdict.commit_ok},
        {"snapshot_lifecycle_ok", verdict.snapshot_lifecycle_ok},
        {"page_truth_ok", verdict.page_truth_ok},
        {"rejected_no_mutation", verdict.rejected_no_mutation},
    }};
    const vivid::evidence::CausalVerdictEvidence chain{
        .name = "nav.library.open.transaction",
        .fields = fields.data(),
        .field_count = fields.size(),
        .cases_closed = static_cast<unsigned>(kSemanticTransitionCases),
    };

    run_log.case_begin("causal_chain");
    vivid::evidence::print_causal_verdict(chain);
    std::printf("\n");
    if (!vivid::evidence::expect(chain_ok, "semantic-to-transaction causal chain closes")) {
        return 1;
    }

    run_log.end(true);
    std::puts("[semantic_transition_demo] ok");
    return 0;
}
