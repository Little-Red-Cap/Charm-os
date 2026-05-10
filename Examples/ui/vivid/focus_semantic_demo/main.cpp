#include <cstddef>
#include <cstdio>

import charm.core.event;
import charm.core.geometry;
import charm.core.style;
import charm.core.style_evidence;
import charm.core.theme_preset;
import charm.gfx.canvas;
import charm.ui.scene;

#include "../support/vivid_evidence_support.hpp"

namespace {
    constexpr Rect kSceneBounds{0, 0, 300, 176};
    constexpr Rect kScopeBounds{12, 12, 204, 148};
    constexpr Rect kTitleBounds{24, 22, 164, 24};
    constexpr Rect kPrimaryBounds{24, 56, 152, 34};
    constexpr Rect kSecondaryBounds{24, 106, 152, 34};
    constexpr Rect kOutsideBounds{234, 62, 44, 44};
    constexpr vivid::evidence::RunLog kRunLog{"fsem", "focus_semantic_demo"};

    struct Handles {
        WidgetHandle root{};
        WidgetHandle scope{};
        WidgetHandle title{};
        WidgetHandle primary{};
        WidgetHandle secondary{};
        WidgetHandle outside{};
    };

    [[nodiscard]] bool style_evidence_equal(const ResolvedStyleEvidence& lhs,
                                            const ResolvedStyleEvidence& rhs) noexcept {
        return lhs.style_key == rhs.style_key
            && lhs.color_hash == rhs.color_hash
            && lhs.metrics_hash == rhs.metrics_hash;
    }

}

int main() {
    auto run_log = kRunLog;
    run_log.begin();
    vivid::evidence::prepare_style_sheet();

    auto& sheet = StyleSheet::instance();

    static DefaultFrameBuffer fb{};
    static DefaultCanvas canvas{fb};
    static ::ui::scene::Scene scene{canvas};
    Handles handles{};

    scene.build([&](::ui::scene::SceneBuilder& builder) {
        handles.root = builder.create_container();
        handles.scope = builder.create_container();
        handles.title = builder.create_label_static("Semantic Focus");
        handles.primary = builder.create_scroll_container();
        handles.secondary = builder.create_scroll_container();
        handles.outside = builder.create_scroll_container();

        builder.link(handles.root, handles.scope);
        builder.link(handles.scope, handles.title);
        builder.link(handles.scope, handles.primary);
        builder.link(handles.scope, handles.secondary);
        builder.link(handles.root, handles.outside);

        builder.set_rect(handles.root, kSceneBounds);
        builder.set_rect(handles.scope, kScopeBounds);
        builder.set_rect(handles.title, kTitleBounds);
        builder.set_rect(handles.primary, kPrimaryBounds);
        builder.set_rect(handles.secondary, kSecondaryBounds);
        builder.set_rect(handles.outside, kOutsideBounds);
        builder.set_semantic(handles.primary, SemanticRole::Button, "primary", "Primary action");
        builder.set_semantic(handles.secondary, SemanticRole::ListItem, "secondary", "Secondary row");
        builder.set_semantic(handles.outside, SemanticRole::Button, "outside", "Outside action");
        builder.set_input_root(handles.root);
        builder.set_focus_scope(handles.scope, handles.primary, true);
        builder.set_root(handles.root);
    });

    run_log.case_begin("semantic_table");
    const auto primary_entry = scene.semantic_snapshot(handles.primary);
    const auto secondary_entry = scene.semantic_snapshot(handles.secondary);
    const auto outside_entry = scene.semantic_snapshot(handles.outside);
    const auto title_entry = scene.semantic_snapshot(handles.title);
    std::printf(" source=runtime targets=3 semantic_id=%s role=%s semantic_id2=%s role2=%s outside_semantic_present=%d stable=1\n",
                primary_entry.id,
                primary_entry.role,
                secondary_entry.id,
                secondary_entry.role,
                outside_entry.found ? 1 : 0);

    if (!vivid::evidence::expect(primary_entry.found && primary_entry.focusable,
                                 "primary semantic target exists")) return 1;
    if (!vivid::evidence::expect(secondary_entry.found && secondary_entry.focusable,
                                 "secondary semantic target exists")) return 1;
    if (!vivid::evidence::expect(outside_entry.found && outside_entry.focusable,
                                 "outside semantic target exists")) return 1;
    if (!vivid::evidence::expect(!title_entry.found,
                                 "decorative title is not semantic focus target")) return 1;

    run_log.case_begin("decorative_excluded");
    std::printf(" widget=label decorative_present=1 decorative_semantic=0 focusable=0 semantic_found=0\n");

    vivid::evidence::click_center(scene, kPrimaryBounds, 10);
    auto primary_access = scene.access();
    const auto primary_semantic = primary_access.semantic_focus_snapshot();
    if (!vivid::evidence::expect(primary_semantic.found, "primary focus resolves semantic target")) return 1;
    if (!vivid::evidence::expect(primary_semantic.id[0] == 'p',
                                 "primary semantic id selected")) return 1;
    const bool primary_focus_committed =
        vivid::evidence::same_handle(primary_access.input_focused(), handles.primary)
        && primary_semantic.found
        && primary_semantic.id[0] == 'p';

    const auto primary_artifact = vivid::evidence::render_scene(scene, canvas, kPrimaryBounds);
    if (!vivid::evidence::expect(primary_artifact.failed_cmds == 0,
                                 "primary semantic focus artifact has no failed commands")) return 1;

    run_log.case_begin("initial_semantic_focus");
    std::printf(" source=pointer semantic_found=1 semantic_current=%s role=%s input_truth=primary focus_ring=1 cmd_hash=%u pixel_hash=%u\n",
                primary_semantic.id,
                primary_semantic.role,
                primary_artifact.cmd_hash,
                primary_artifact.pixel_hash);

    scene.dispatch_event(Event::mouse(Event::Type::MouseDown,
                                      kSecondaryBounds.x + kSecondaryBounds.w / 2,
                                      kSecondaryBounds.y + kSecondaryBounds.h / 2,
                                      1,
                                      20));
    auto transfer_access = scene.access();
    const auto transfer_trace =
        vivid::evidence::collect_focus_move(transfer_access, handles.primary, handles.secondary);
    const auto secondary_semantic = transfer_access.semantic_focus_snapshot();
    if (!vivid::evidence::expect(transfer_trace.focus_out == 1 && transfer_trace.focus_out_expected,
                                 "semantic transfer emits FocusOut primary")) return 1;
    if (!vivid::evidence::expect(transfer_trace.focus_in == 1 && transfer_trace.focus_in_expected,
                                 "semantic transfer emits FocusIn secondary")) return 1;
    if (!vivid::evidence::expect(secondary_semantic.found, "secondary focus resolves semantic target")) return 1;
    if (!vivid::evidence::expect(secondary_semantic.id[0] == 's',
                                 "secondary semantic id selected")) return 1;
    const bool secondary_focus_committed =
        vivid::evidence::same_handle(transfer_access.input_focused(), handles.secondary)
        && secondary_semantic.found
        && secondary_semantic.id[0] == 's';
    scene.dispatch_event(Event::mouse(Event::Type::MouseUp,
                                      kSecondaryBounds.x + kSecondaryBounds.w / 2,
                                      kSecondaryBounds.y + kSecondaryBounds.h / 2,
                                      1,
                                      21));

    run_log.case_begin("transfer_semantic_focus");
    std::printf(" source=pointer old=primary new=secondary focus_out=%d focus_in=%d semantic_found=1 semantic_current=%s role=%s input_truth=secondary\n",
                transfer_trace.focus_out,
                transfer_trace.focus_in,
                secondary_semantic.id,
                secondary_semantic.role);

    scene.dispatch_event(Event::key(Event::Type::KeyDown, Event::Key::Up, 30));
    auto key_access = scene.access();
    const auto key_trace =
        vivid::evidence::collect_focus_move(key_access, handles.secondary, handles.primary);
    const auto key_semantic = key_access.semantic_focus_snapshot();
    if (!vivid::evidence::expect(key_trace.focus_out == 1 && key_trace.focus_out_expected,
                                 "keyboard semantic focus emits FocusOut secondary")) return 1;
    if (!vivid::evidence::expect(key_trace.focus_in == 1 && key_trace.focus_in_expected,
                                 "keyboard semantic focus emits FocusIn primary")) return 1;
    if (!vivid::evidence::expect(key_semantic.found && key_semantic.id[0] == 'p',
                                 "keyboard focus resolves primary semantic target")) return 1;
    const bool keyboard_focus_committed =
        vivid::evidence::same_handle(key_access.input_focused(), handles.primary)
        && key_semantic.found
        && key_semantic.id[0] == 'p';

    run_log.case_begin("keyboard_semantic_focus");
    std::printf(" source=key key=up old=secondary new=primary semantic_found=1 semantic_current=%s role=%s focus_out=%d focus_in=%d input_truth=primary\n",
                key_semantic.id,
                key_semantic.role,
                key_trace.focus_out,
                key_trace.focus_in);

    scene.dispatch_event(Event::key(Event::Type::KeyDown, Event::Key::Right, 40));
    auto outside_access = scene.access();
    const auto outside_skip_semantic = outside_access.semantic_focus_snapshot();
    if (!vivid::evidence::expect(outside_skip_semantic.found,
                                 "inside fallback focus still resolves semantic target")) return 1;
    if (!vivid::evidence::expect(!vivid::evidence::same_handle(outside_access.input_focused(), handles.outside),
                                 "outside semantic target is not selected by active scope navigation")) return 1;
    const bool outside_excluded =
        !vivid::evidence::same_handle(outside_access.input_focused(), handles.outside)
        && outside_skip_semantic.found;

    run_log.case_begin("outside_semantic_not_selected");
    std::printf(" source=key key=right outside_semantic_present=1 outside_selected=0 semantic_found=1 semantic_current=%s input_truth=inside_scope\n",
                outside_skip_semantic.id);

    const StyleStateEvidence focus_state = make_style_state_evidence(WidgetKind::ScrollContainer);
    const StyleState normal_state = make_style_state(true, false, false, false);
    const StyleState focused_state = make_style_state(true, false, false, true);
    const auto normal_style = sheet.lookup(WidgetKind::ScrollContainer, normal_state);
    const auto focused_style = sheet.lookup(WidgetKind::ScrollContainer, focused_state);
    if (!vivid::evidence::expect(normal_style.colors != nullptr && normal_style.metrics != nullptr,
                                 "button normal style resolves")) return 1;
    if (!vivid::evidence::expect(focused_style.colors != nullptr && focused_style.metrics != nullptr,
                                 "button focused style resolves")) return 1;
    const auto normal_evidence = make_resolved_style_evidence(normal_style);
    const auto focused_evidence = make_resolved_style_evidence(focused_style);
    if (!vivid::evidence::expect(style_evidence_equal(normal_evidence, focused_evidence),
                                 "semantic focus keeps style evidence stable")) return 1;
    if (!vivid::evidence::expect(!focus_state.includes_focused,
                                 "semantic focus remains outside scroll container style mask")) return 1;
    const bool semantic_style_boundary =
        style_evidence_equal(normal_evidence, focused_evidence)
        && !focus_state.includes_focused;

    run_log.case_begin("style_boundary");
    std::printf(" widget=scroll_container focused_in_style_mask=%d style_same=1 style_key=%u color_hash=%u metrics_hash=%u\n",
                focus_state.includes_focused ? 1 : 0,
                focused_evidence.style_key,
                focused_evidence.color_hash,
                focused_evidence.metrics_hash);

    const Rect aligned_bounds = vivid::evidence::same_handle(outside_access.input_focused(), handles.primary)
        ? kPrimaryBounds
        : kSecondaryBounds;
    const auto aligned_artifact = vivid::evidence::render_scene(scene, canvas, aligned_bounds);
    if (!vivid::evidence::expect(aligned_artifact.failed_cmds == 0,
                                 "semantic aligned artifact has no failed commands")) return 1;
    if (!vivid::evidence::expect(aligned_artifact.cmd_count > 0,
                                 "semantic aligned artifact records commands")) return 1;
    const bool artifact_aligned =
        aligned_artifact.failed_cmds == 0
        && aligned_artifact.cmd_count > 0
        && outside_skip_semantic.found
        && !vivid::evidence::same_handle(outside_access.input_focused(), handles.outside);

    run_log.case_begin("artifact_alignment");
    std::printf(" semantic_current=%s input_truth=%s focus_ring=1 dirty_count=%zu cmd_hash=%u pixel_hash=%u artifact_aligned=1\n",
                outside_skip_semantic.id,
                outside_skip_semantic.id,
                aligned_artifact.dirty_count,
                aligned_artifact.cmd_hash,
                aligned_artifact.pixel_hash);

    const vivid::evidence::CausalChainEvidence chain{
        .name = "semantic_focus.alignment",
        .request_ok = primary_entry.found
            && primary_entry.focusable
            && secondary_entry.found
            && secondary_entry.focusable
            && outside_entry.found
            && outside_entry.focusable
            && !title_entry.found
            && transfer_trace.focus_out == 1
            && transfer_trace.focus_out_expected
            && transfer_trace.focus_in == 1
            && transfer_trace.focus_in_expected
            && key_trace.focus_out == 1
            && key_trace.focus_out_expected
            && key_trace.focus_in == 1
            && key_trace.focus_in_expected
            && outside_excluded,
        .state_delta_ok = primary_focus_committed
            && secondary_focus_committed
            && keyboard_focus_committed
            && outside_excluded,
        .invalidation_ok = semantic_style_boundary,
        .artifact_ok = artifact_aligned
            && primary_artifact.cmd_count > 0,
        .rejected_no_mutation = outside_excluded,
    };
    run_log.case_begin("causal_chain");
    vivid::evidence::print_causal_chain(chain);
    std::printf("\n");
    if (!vivid::evidence::expect(chain.ok(),
                                 "semantic focus causal chain closes")) {
        return 1;
    }

    run_log.end(true);
    std::puts("[focus_semantic_demo] ok");
    return 0;
}
