#include <cstddef>
#include <cstdio>

import charm.core.event;
import charm.core.geometry;
import charm.core.style;
import charm.core.theme_preset;
import charm.gfx.canvas;
import charm.ui.scene;

#include "../support/vivid_evidence_support.hpp"

namespace {
    constexpr Rect kSceneBounds{0, 0, 300, 160};
    constexpr Rect kBaseScopeBounds{12, 12, 188, 136};
    constexpr Rect kBaseABounds{24, 28, 140, 34};
    constexpr Rect kBaseBBounds{24, 84, 140, 34};
    constexpr Rect kModalScopeBounds{204, 18, 84, 124};
    constexpr Rect kModalALocalBounds{10, 14, 60, 34};
    constexpr Rect kModalBLocalBounds{10, 70, 60, 34};
    constexpr Rect kModalAWorldBounds{214, 32, 60, 34};
    constexpr Rect kModalBWorldBounds{214, 88, 60, 34};
    constexpr vivid::evidence::RunLog kRunLog{"fsn", "focus_scope_nested_demo"};

    struct Handles {
        WidgetHandle root{};
        WidgetHandle base_scope{};
        WidgetHandle base_a{};
        WidgetHandle base_b{};
        WidgetHandle modal_scope{};
        WidgetHandle modal_a{};
        WidgetHandle modal_b{};
    };



    struct EventCounts {
        int mouse_down{0};
        int focus_out{0};
        int focus_in{0};
        bool focus_out_expected{false};
        bool focus_in_expected{false};
    };

    [[nodiscard]] EventCounts collect_events(::ui::scene::SceneAccess& access,
                                             WidgetHandle mouse_target,
                                             WidgetHandle focus_out_target,
                                             WidgetHandle focus_in_target) noexcept {
        EventCounts out{};
        for (std::size_t index = 0; index < access.input_event_count(); ++index) {
            const auto& event = access.input_event(index);
            if (event.event.type == Event::Type::MouseDown && vivid::evidence::same_handle(event.target, mouse_target)) {
                ++out.mouse_down;
            } else if (event.event.type == Event::Type::FocusOut) {
                ++out.focus_out;
                out.focus_out_expected = out.focus_out_expected || vivid::evidence::same_handle(event.target, focus_out_target);
            } else if (event.event.type == Event::Type::FocusIn) {
                ++out.focus_in;
                out.focus_in_expected = out.focus_in_expected || vivid::evidence::same_handle(event.target, focus_in_target);
            }
        }
        return out;
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
        handles.base_scope = builder.create_container();
        handles.base_a = builder.create_scroll_container();
        handles.base_b = builder.create_scroll_container();
        handles.modal_scope = builder.create_container();
        handles.modal_a = builder.create_scroll_container();
        handles.modal_b = builder.create_scroll_container();

        builder.link(handles.root, handles.base_scope);
        builder.link(handles.base_scope, handles.base_a);
        builder.link(handles.base_scope, handles.base_b);
        builder.link(handles.root, handles.modal_scope);
        builder.link(handles.modal_scope, handles.modal_a);
        builder.link(handles.modal_scope, handles.modal_b);

        builder.set_rect(handles.root, kSceneBounds);
        builder.set_rect(handles.base_scope, kBaseScopeBounds);
        builder.set_rect(handles.base_a, kBaseABounds);
        builder.set_rect(handles.base_b, kBaseBBounds);
        builder.set_rect(handles.modal_scope, kModalScopeBounds);
        builder.set_rect(handles.modal_a, kModalALocalBounds);
        builder.set_rect(handles.modal_b, kModalBLocalBounds);
        builder.set_input_root(handles.root);
        builder.set_focus_scope(handles.base_scope, handles.base_b, true);
        builder.set_root(handles.root);
    });

    run_log.case_begin("tree_model");
    std::printf(" base_targets=2 modal_targets=2 root_targets=2 nested=1\n");

    auto access = scene.access();
    if (!vivid::evidence::expect(vivid::evidence::same_handle(access.input_focus_scope(), handles.base_scope),
                                 "base focus scope is installed")) {
        return 1;
    }
    if (!vivid::evidence::expect(vivid::evidence::same_handle(access.input_focus_scope_fallback(), handles.base_b),
                                 "base fallback is installed")) {
        return 1;
    }
    if (!vivid::evidence::expect(access.input_focus_scope_stack_size() == 0,
                                 "initial focus scope stack is empty")) {
        return 1;
    }

    run_log.case_begin("base_scope_install");
    std::printf(" active=base_scope fallback=base_b trap=1 stack=%zu\n",
                access.input_focus_scope_stack_size());

    vivid::evidence::mouse_down_center(scene, kBaseABounds, 10);
    auto base_initial = scene.access();
    const auto base_initial_events = collect_events(base_initial, handles.base_a, {}, handles.base_a);
    if (!vivid::evidence::expect(base_initial_events.mouse_down == 1, "base_a receives mouse down")) return 1;
    if (!vivid::evidence::expect(base_initial_events.focus_in == 1 && base_initial_events.focus_in_expected,
                                 "base_a receives FocusIn")) {
        return 1;
    }
    if (!vivid::evidence::expect(vivid::evidence::same_handle(base_initial.input_focused(), handles.base_a),
                                 "input focus commits to base_a")) {
        return 1;
    }
    vivid::evidence::mouse_up_center(scene, kBaseABounds, 11);

    const auto base_a_artifact = vivid::evidence::render_scene(scene, canvas, kBaseABounds);
    if (!vivid::evidence::expect(base_a_artifact.failed_cmds == 0, "base_a render has no failed commands")) return 1;

    run_log.case_begin("base_initial_focus");
    std::printf(" target=base_a mouse_down=%d focus_in=%d input_truth=base_a cmd_count=%zu pixel_hash=%u\n",
                base_initial_events.mouse_down,
                base_initial_events.focus_in,
                base_a_artifact.cmd_count,
                base_a_artifact.pixel_hash);

    auto push_access = scene.access();
    const bool pushed = push_access.push_focus_scope(handles.modal_scope, handles.modal_a, true);
    if (!vivid::evidence::expect(pushed, "modal focus scope push succeeds")) return 1;
    if (!vivid::evidence::expect(vivid::evidence::same_handle(push_access.input_focus_scope(), handles.modal_scope),
                                 "modal focus scope becomes active")) {
        return 1;
    }
    if (!vivid::evidence::expect(push_access.input_focus_scope_stack_size() == 1,
                                 "modal push stores one base frame")) {
        return 1;
    }

    run_log.case_begin("modal_scope_push");
    std::printf(" active=modal_scope fallback=modal_a trap=1 pushed=1 stack=%zu previous=base_scope\n",
                push_access.input_focus_scope_stack_size());

    vivid::evidence::mouse_down_center(scene, kModalBWorldBounds, 20);
    auto modal_inside = scene.access();
    const auto modal_inside_events = collect_events(modal_inside, handles.modal_b, handles.base_a, handles.modal_b);
    if (!vivid::evidence::expect(modal_inside_events.mouse_down == 1, "modal_b receives mouse down")) return 1;
    if (!vivid::evidence::expect(modal_inside_events.focus_out == 1 && modal_inside_events.focus_out_expected,
                                 "modal entry emits FocusOut for base_a")) {
        return 1;
    }
    if (!vivid::evidence::expect(modal_inside_events.focus_in == 1 && modal_inside_events.focus_in_expected,
                                 "modal_b receives FocusIn")) {
        return 1;
    }
    if (!vivid::evidence::expect(vivid::evidence::same_handle(modal_inside.input_focused(), handles.modal_b),
                                 "input focus commits to modal_b")) {
        return 1;
    }
    vivid::evidence::mouse_up_center(scene, kModalBWorldBounds, 21);

    const auto modal_b_artifact = vivid::evidence::render_scene(scene, canvas, kModalBWorldBounds);
    if (!vivid::evidence::expect(modal_b_artifact.failed_cmds == 0, "modal_b render has no failed commands")) return 1;

    run_log.case_begin("modal_inside_focus");
    std::printf(" target=modal_b mouse_down=%d focus_out=%d focus_in=%d input_truth=modal_b stack=%zu focus_ring=1 pixel_hash=%u\n",
                modal_inside_events.mouse_down,
                modal_inside_events.focus_out,
                modal_inside_events.focus_in,
                modal_inside.input_focus_scope_stack_size(),
                modal_b_artifact.pixel_hash);

    const auto base_b_baseline = vivid::evidence::render_scene(scene, canvas, kBaseBBounds);
    if (!vivid::evidence::expect(base_b_baseline.failed_cmds == 0, "base_b baseline render has no failed commands")) {
        return 1;
    }

    vivid::evidence::mouse_down_center(scene, kBaseBBounds, 30);
    auto modal_trap = scene.access();
    const auto modal_trap_events = collect_events(modal_trap, handles.base_b, handles.modal_b, handles.base_b);
    if (!vivid::evidence::expect(modal_trap_events.mouse_down == 1, "base_b still receives pointer event")) return 1;
    if (!vivid::evidence::expect(modal_trap_events.focus_out == 0 && modal_trap_events.focus_in == 0,
                                 "modal trap rejects base focus transfer")) {
        return 1;
    }
    if (!vivid::evidence::expect(vivid::evidence::same_handle(modal_trap.input_focused(), handles.modal_b),
                                 "input focus remains in modal scope")) {
        return 1;
    }
    vivid::evidence::mouse_up_center(scene, kBaseBBounds, 31);

    const auto base_b_rejected = vivid::evidence::render_scene(scene, canvas, kBaseBBounds);
    if (!vivid::evidence::expect(base_b_rejected.cmd_hash == base_b_baseline.cmd_hash,
                                 "base_b rejected artifact keeps command baseline")) {
        return 1;
    }
    if (!vivid::evidence::expect(base_b_rejected.pixel_hash == base_b_baseline.pixel_hash,
                                 "base_b rejected artifact keeps pixel baseline")) {
        return 1;
    }

    run_log.case_begin("modal_trap_dispatch");
    std::printf(" requested=base_b mouse_down=%d focus_out=%d focus_in=%d input_truth=modal_b active=modal_scope stack=%zu leaked=0\n",
                modal_trap_events.mouse_down,
                modal_trap_events.focus_out,
                modal_trap_events.focus_in,
                modal_trap.input_focus_scope_stack_size());

    auto pop_access = scene.access();
    const bool popped = pop_access.pop_focus_scope();
    if (!vivid::evidence::expect(popped, "modal focus scope pop succeeds")) return 1;
    if (!vivid::evidence::expect(vivid::evidence::same_handle(pop_access.input_focus_scope(), handles.base_scope),
                                 "base focus scope is restored")) {
        return 1;
    }
    if (!vivid::evidence::expect(pop_access.input_focus_scope_stack_size() == 0,
                                 "focus scope stack returns to zero")) {
        return 1;
    }

    run_log.case_begin("modal_scope_pop");
    std::printf(" popped=1 active=base_scope fallback=base_b stack=%zu input_truth=modal_b\n",
                pop_access.input_focus_scope_stack_size());

    vivid::evidence::mouse_down_center(scene, kModalAWorldBounds, 40);
    auto restored_trap = scene.access();
    const auto restored_events = collect_events(restored_trap, handles.modal_a, handles.modal_b, handles.modal_a);
    if (!vivid::evidence::expect(restored_events.mouse_down == 1, "modal_a still receives pointer event")) return 1;
    if (!vivid::evidence::expect(restored_events.focus_out == 1 && restored_events.focus_out_expected,
                                 "restored base scope clears previous modal focus")) {
        return 1;
    }
    if (!vivid::evidence::expect(restored_events.focus_in == 1,
                                 "restored base scope focuses fallback")) {
        return 1;
    }
    if (!vivid::evidence::expect(vivid::evidence::same_handle(restored_trap.input_focused(), handles.base_b),
                                 "restored base scope redirects outside request to fallback")) {
        return 1;
    }
    vivid::evidence::mouse_up_center(scene, kModalAWorldBounds, 41);

    const auto modal_a_rejected = vivid::evidence::render_scene(scene, canvas, kModalAWorldBounds);
    if (!vivid::evidence::expect(modal_a_rejected.failed_cmds == 0, "modal_a rejected render has no failed commands")) return 1;

    run_log.case_begin("restored_base_trap");
    std::printf(" requested=modal_a mouse_down=%d focus_out=%d focus_in=%d input_truth=base_b active=base_scope stack=%zu outside_focus_ring=0\n",
                restored_events.mouse_down,
                restored_events.focus_out,
                restored_events.focus_in,
                restored_trap.input_focus_scope_stack_size());

    run_log.end(true);
    std::puts("[focus_scope_nested_demo] ok");
    return 0;
}
