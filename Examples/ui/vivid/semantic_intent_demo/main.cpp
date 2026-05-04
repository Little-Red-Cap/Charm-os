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
    constexpr Rect kSceneBounds{0, 0, 360, 196};
    constexpr Rect kScopeBounds{12, 12, 210, 168};
    constexpr Rect kActionBounds{24, 24, 112, 30};
    constexpr Rect kRowBounds{24, 66, 148, 30};
    constexpr Rect kInfoBounds{24, 110, 148, 34};
    constexpr Rect kDuplicateBounds{236, 24, 96, 30};
    constexpr vivid::evidence::RunLog kRunLog{"sint", "semantic_intent_demo"};

    struct Handles {
        WidgetHandle root{};
        WidgetHandle scope{};
        WidgetHandle action{};
        WidgetHandle row{};
        WidgetHandle info{};
        WidgetHandle duplicate{};
    };

    [[nodiscard]] bool resolved(const SemanticIntentResolution& resolution) noexcept {
        return resolution.status == SemanticIntentStatus::Resolved
            && resolution.found
            && resolution.executable;
    }

    [[nodiscard]] bool no_input_side_effect(::ui::scene::Scene& scene,
                                            const Handles& handles,
                                            std::size_t before_events) noexcept {
        return scene.access().input_event_count() == before_events
            && !scene.access().pressed(handles.action)
            && !scene.access().focused(handles.action)
            && !scene.access().pressed(handles.row)
            && !scene.access().focused(handles.row);
    }

    void print_resolution(const SemanticIntentResolution& resolution) noexcept {
        std::printf(" status=%s found=%d executable=%d id=%s actions=%u visited=%zu matches=%zu\n",
                    semantic_intent_status_name(resolution.status),
                    resolution.found ? 1 : 0,
                    resolution.executable ? 1 : 0,
                    resolution.id,
                    resolution.actions,
                    resolution.visited_count,
                    resolution.match_count);
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

    scene.build([&](::ui::scene::SceneBuilder& builder) {
        handles.root = builder.create_container();
        handles.scope = builder.create_container();
        handles.action = builder.create_button_static("Apply");
        handles.row = builder.create_list_item("Track One");
        handles.info = builder.create_container();
        handles.duplicate = builder.create_button_static("Apply Copy");

        builder.link(handles.root, handles.scope);
        builder.link(handles.scope, handles.action);
        builder.link(handles.scope, handles.row);
        builder.link(handles.scope, handles.info);
        builder.link(handles.root, handles.duplicate);

        builder.set_rect(handles.root, kSceneBounds);
        builder.set_rect(handles.scope, kScopeBounds);
        builder.set_rect(handles.action, kActionBounds);
        builder.set_rect(handles.row, kRowBounds);
        builder.set_rect(handles.info, kInfoBounds);
        builder.set_rect(handles.duplicate, kDuplicateBounds);
        builder.set_semantic_default(handles.action, "action.apply");
        builder.set_semantic_default(handles.row, "row.track_one");
        builder.set_semantic(handles.info, SemanticRole::Container, "panel.info", "Info panel");
        builder.set_semantic_default(handles.duplicate, "action.apply", "Duplicate apply");
        builder.set_input_root(handles.root);
        builder.set_focus_scope(handles.scope, handles.action, true);
        builder.set_root(handles.root);
    });

    auto access = scene.access();
    const std::size_t input_events_before = access.input_event_count();

    const auto action_resolution =
        scene.resolve_semantic_intent(handles.scope, "action.apply", SemanticAction::Activate);
    run_log.case_begin("resolve_activate");
    print_resolution(action_resolution);
    if (!vivid::evidence::expect(resolved(action_resolution), "button activate intent resolves")) return 1;
    if (!vivid::evidence::expect(action_resolution.handle == handles.action,
                                 "button activate resolves to scoped handle")) {
        return 1;
    }

    run_log.case_begin("no_execute_side_effect");
    std::printf(" before_events=%zu after_events=%zu pressed=%d focused=%d execute=0\n",
                input_events_before,
                access.input_event_count(),
                access.pressed(handles.action) ? 1 : 0,
                access.focused(handles.action) ? 1 : 0);
    if (!vivid::evidence::expect(no_input_side_effect(scene, handles, input_events_before),
                                 "semantic intent resolution does not execute input")) {
        return 1;
    }

    const auto unsupported =
        scene.resolve_semantic_intent(handles.scope, "panel.info", SemanticAction::Activate);
    run_log.case_begin("unsupported_action");
    print_resolution(unsupported);
    if (!vivid::evidence::expect(unsupported.status == SemanticIntentStatus::UnsupportedAction,
                                 "container semantic id rejects activate")) {
        return 1;
    }

    const auto missing =
        scene.resolve_semantic_intent(handles.scope, "missing.id", SemanticAction::Activate);
    run_log.case_begin("missing_id");
    print_resolution(missing);
    if (!vivid::evidence::expect(missing.status == SemanticIntentStatus::NotFound,
                                 "missing semantic id is not found")) {
        return 1;
    }

    const auto ambiguous =
        scene.resolve_semantic_intent(handles.root, "action.apply", SemanticAction::Activate);
    run_log.case_begin("ambiguous_id");
    print_resolution(ambiguous);
    if (!vivid::evidence::expect(ambiguous.status == SemanticIntentStatus::AmbiguousId,
                                 "duplicate id under root is ambiguous")) {
        return 1;
    }
    if (!vivid::evidence::expect(ambiguous.match_count == 2, "ambiguous intent counts two matches")) return 1;

    access.set_enabled(handles.row, false);
    const auto disabled =
        scene.resolve_semantic_intent(handles.scope, "row.track_one", SemanticAction::Activate);
    run_log.case_begin("disabled_target");
    print_resolution(disabled);
    if (!vivid::evidence::expect(disabled.status == SemanticIntentStatus::Disabled,
                                 "disabled semantic target is not executable")) {
        return 1;
    }

    const auto invalid_root =
        scene.resolve_semantic_intent({}, "action.apply", SemanticAction::Activate);
    const auto missing_request =
        scene.resolve_semantic_intent(handles.scope, "", SemanticAction::Activate);
    run_log.case_begin("invalid_request");
    std::printf(" invalid_root=%s missing_id=%s root_found=%d id_found=%d\n",
                semantic_intent_status_name(invalid_root.status),
                semantic_intent_status_name(missing_request.status),
                invalid_root.found ? 1 : 0,
                missing_request.found ? 1 : 0);
    if (!vivid::evidence::expect(invalid_root.status == SemanticIntentStatus::InvalidRoot,
                                 "invalid root is explicit")) {
        return 1;
    }
    if (!vivid::evidence::expect(missing_request.status == SemanticIntentStatus::MissingId,
                                 "missing request id is explicit")) {
        return 1;
    }

    run_log.end(true);
    std::puts("[semantic_intent_demo] ok");
    return 0;
}
