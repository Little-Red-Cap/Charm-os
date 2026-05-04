#include <cstddef>
#include <cstdint>
#include <cstdio>

import charm.core.geometry;
import charm.core.style;
import charm.core.theme_preset;
import charm.gfx.canvas;
import charm.ui.scene;

#include "../support/vivid_evidence_support.hpp"

namespace {
    constexpr Rect kSceneBounds{0, 0, 280, 120};
    constexpr Rect kComponentBounds{12, 12, 244, 72};
    constexpr Rect kTitleBounds{24, 20, 132, 20};
    constexpr Rect kToggleBounds{176, 28, 64, 30};
    constexpr vivid::evidence::RunLog kRunLog{"ia", "intent_artifact_demo"};

    struct Handles {
        WidgetHandle root{};
        WidgetHandle row{};
        WidgetHandle title{};
        WidgetHandle toggle{};
    };

    [[nodiscard]] bool same_handle(WidgetHandle lhs, WidgetHandle rhs) noexcept {
        return lhs == rhs;
    }

    void build_scene(::ui::scene::Scene& scene, Handles& handles) {
        scene.build([&](::ui::scene::SceneBuilder& builder) {
            handles.root = builder.create_container();
            handles.row = builder.create_container();
            handles.title = builder.create_label_static("Wi-Fi");
            handles.toggle = builder.create_checkbox("");

            builder.link(handles.root, handles.row);
            builder.link(handles.row, handles.title);
            builder.link(handles.row, handles.toggle);

            builder.set_rect(handles.root, kSceneBounds);
            builder.set_rect(handles.row, kComponentBounds);
            builder.set_rect(handles.title, kTitleBounds);
            builder.set_rect(handles.toggle, kToggleBounds);
            builder.set_semantic(handles.row, SemanticRole::Container, "settings.wifi", "Wi-Fi setting");
            builder.set_semantic_default(handles.toggle, "settings.wifi.toggle", "Wi-Fi toggle");
            builder.set_input_root(handles.root);
            builder.set_focus_scope(handles.row, handles.toggle, true);
            builder.set_root(handles.root);
        });
    }

    void print_action_request_ledger(const SemanticActionRequest& request) noexcept {
        const SemanticActionRequestLedger ledger = semantic_action_request_ledger(request);
        std::printf(" ledger=action_request stage=%s status=%s reason=%s intent=%s action_admission=%s focus=%s admitted=%d focus_ready=%d executed=%d click=%d id=%s\n",
                    semantic_action_request_stage_name(ledger.stage),
                    semantic_action_request_status_name(ledger.status),
                    semantic_action_request_reject_reason_name(ledger.reject_reason),
                    semantic_intent_status_name(ledger.intent_status),
                    semantic_action_admission_status_name(ledger.action_admission_status),
                    semantic_focus_request_status_name(ledger.focus_request_status),
                    ledger.admitted ? 1 : 0,
                    ledger.focus_ready ? 1 : 0,
                    ledger.executed ? 1 : 0,
                    ledger.emitted_click ? 1 : 0,
                    ledger.id);
    }

    void print_render_evidence(const char* prefix,
                               const vivid::evidence::RenderEvidence& evidence) noexcept {
        std::printf(" %s_dirty_count=%zu %s_dirty_hash=%u %s_cmd_count=%zu %s_cmd_bytes=%zu %s_exec_cmds=%zu %s_failed=%zu %s_cmd_hash=%u %s_pixel_hash=%u",
                    prefix,
                    evidence.dirty_count,
                    prefix,
                    evidence.dirty_hash,
                    prefix,
                    evidence.cmd_count,
                    prefix,
                    evidence.cmd_bytes,
                    prefix,
                    evidence.exec_cmds,
                    prefix,
                    evidence.failed_cmds,
                    prefix,
                    evidence.cmd_hash,
                    prefix,
                    evidence.pixel_hash);
    }

    [[nodiscard]] bool artifact_same(const vivid::evidence::RenderEvidence& lhs,
                                     const vivid::evidence::RenderEvidence& rhs) noexcept {
        return lhs.dirty_count == rhs.dirty_count
            && lhs.dirty_hash == rhs.dirty_hash
            && lhs.cmd_hash == rhs.cmd_hash
            && lhs.pixel_hash == rhs.pixel_hash
            && lhs.cmd_count == rhs.cmd_count
            && lhs.cmd_bytes == rhs.cmd_bytes
            && lhs.exec_cmds == rhs.exec_cmds
            && lhs.failed_cmds == rhs.failed_cmds;
    }
}

int main() {
    auto run_log = kRunLog;
    run_log.begin();
    vivid::evidence::prepare_style_sheet();

    static DefaultFrameBuffer fb{};
    static DefaultCanvas canvas{fb};
    static ::ui::scene::Scene scene{canvas};
    Handles handles{};
    build_scene(scene, handles);

    auto access = scene.access();
    access.set_focusable(handles.toggle, true);
    const auto focus_setup = scene.request_semantic_focus(handles.row, "settings.wifi.toggle");
    if (!vivid::evidence::expect(focus_setup.status == SemanticFocusRequestStatus::Committed
                                 && same_handle(access.input_focused(), handles.toggle),
                                 "setup focuses toggle through semantic focus request")) {
        return 1;
    }

    const auto baseline = vivid::evidence::render_scene(scene, canvas, kComponentBounds);
    if (!vivid::evidence::expect(baseline.failed_cmds == 0, "baseline render has no failed commands")) return 1;
    if (!vivid::evidence::expect(!access.checked(handles.toggle), "baseline toggle is unchecked")) return 1;

    run_log.case_begin("baseline_artifact");
    std::printf(" checked=%d focus=toggle", access.checked(handles.toggle) ? 1 : 0);
    print_render_evidence("base", baseline);
    std::printf("\n");

    const bool before_checked = access.checked(handles.toggle);
    const auto positive_request =
        scene.request_semantic_action(handles.row, "settings.wifi.toggle", SemanticAction::Activate);
    const bool after_checked = access.checked(handles.toggle);

    run_log.case_begin("request_ledger");
    print_action_request_ledger(positive_request);
    if (!vivid::evidence::expect(positive_request.status == SemanticActionRequestStatus::Executed
                                 && positive_request.reject_reason == SemanticActionRequestRejectReason::None
                                 && positive_request.emitted_click
                                 && positive_request.focus_ready,
                                 "semantic action request commits through normal action path")) {
        return 1;
    }

    run_log.case_begin("state_delta");
    vivid::evidence::print_state_delta({
        .id = "settings.wifi.toggle",
        .key = "checked",
        .source = "semantic_action_request",
        .old_value = before_checked ? 1 : 0,
        .new_value = after_checked ? 1 : 0,
    });
    std::printf("\n");
    if (!vivid::evidence::expect(!before_checked && after_checked,
                                 "semantic activate toggles checked truth")) {
        return 1;
    }

    run_log.case_begin("invalidation");
    vivid::evidence::print_invalidation({
        .kind = "paint_only",
        .dirty_scope = "component",
        .component_bounds = kComponentBounds,
        .layout_changed = false,
    });
    std::printf("\n");

    const auto changed = vivid::evidence::render_scene(scene, canvas, kComponentBounds);
    if (!vivid::evidence::expect(changed.failed_cmds == 0, "changed render has no failed commands")) return 1;
    if (!vivid::evidence::expect(changed.pixel_hash != baseline.pixel_hash,
                                 "state delta changes render artifact")) {
        return 1;
    }
    if (!vivid::evidence::expect(changed.dirty_count == 1, "changed render keeps one dirty rect")) return 1;
    if (!vivid::evidence::expect(vivid::evidence::dirty_stays_inside(canvas, kComponentBounds),
                                 "changed dirty evidence stays inside component")) {
        return 1;
    }

    run_log.case_begin("render_artifact");
    std::printf(" changed=1 dirty_within_component=1");
    print_render_evidence("after", changed);
    std::printf("\n");

    access.set_enabled(handles.toggle, false);
    const auto disabled_baseline = vivid::evidence::render_scene(scene, canvas, kComponentBounds);
    if (!vivid::evidence::expect(disabled_baseline.failed_cmds == 0,
                                 "disabled baseline render has no failed commands")) {
        return 1;
    }
    const bool disabled_before_checked = access.checked(handles.toggle);
    const auto rejected =
        scene.request_semantic_action(handles.row, "settings.wifi.toggle", SemanticAction::Activate);
    const bool disabled_after_checked = access.checked(handles.toggle);

    run_log.case_begin("rejected_request_ledger");
    print_action_request_ledger(rejected);
    if (!vivid::evidence::expect(rejected.status == SemanticActionRequestStatus::Rejected
                                 && rejected.reject_reason == SemanticActionRequestRejectReason::ActionAdmissionRejected
                                 && rejected.admission.status == SemanticActionAdmissionStatus::Disabled
                                 && !rejected.emitted_click,
                                 "disabled semantic target is rejected before execution")) {
        return 1;
    }

    run_log.case_begin("rejected_no_state_delta");
    vivid::evidence::print_state_delta({
        .id = "settings.wifi.toggle",
        .key = "checked",
        .source = "semantic_action_request",
        .old_value = disabled_before_checked ? 1 : 0,
        .new_value = disabled_after_checked ? 1 : 0,
        .reason = "admission_rejected",
    });
    std::printf("\n");
    if (!vivid::evidence::expect(disabled_before_checked == disabled_after_checked,
                                 "rejected request preserves checked truth")) {
        return 1;
    }

    const auto rejected_artifact = vivid::evidence::render_scene(scene, canvas, kComponentBounds);
    if (!vivid::evidence::expect(rejected_artifact.failed_cmds == 0,
                                 "rejected render has no failed commands")) {
        return 1;
    }
    if (!vivid::evidence::expect(artifact_same(disabled_baseline, rejected_artifact),
                                 "rejected request preserves render artifact")) {
        return 1;
    }

    run_log.case_begin("rejected_artifact");
    std::printf(" changed=0 dirty_within_component=1");
    print_render_evidence("before", disabled_baseline);
    print_render_evidence("after", rejected_artifact);
    std::printf("\n");

    run_log.end(true);
    std::puts("[intent_artifact_demo] ok");
    return 0;
}
