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
    constexpr Rect kSceneBounds{0, 0, 320, 220};
    constexpr Rect kScopeBounds{10, 10, 230, 190};
    constexpr Rect kOriginBounds{72, 82, 56, 36};
    constexpr Rect kRightBounds{160, 82, 56, 36};
    constexpr Rect kDownBounds{160, 142, 56, 36};
    constexpr Rect kTopBounds{72, 28, 56, 36};
    constexpr Rect kOutsideBounds{262, 86, 42, 42};
    constexpr vivid::evidence::RunLog kRunLog{"fss", "focus_spatial_navigation_demo"};

    struct Handles {
        WidgetHandle root{};
        WidgetHandle scope{};
        WidgetHandle origin{};
        WidgetHandle right{};
        WidgetHandle down{};
        WidgetHandle top{};
        WidgetHandle outside{};
    };



    bool expect_focus_move(::ui::scene::SceneAccess& access,
                           WidgetHandle old_target,
                           WidgetHandle new_target,
                           const char* label) noexcept {
        const auto trace = vivid::evidence::collect_focus_move(access, old_target, new_target);
        if (!vivid::evidence::expect(trace.focus_out == 1 && trace.focus_out_expected, label)) return false;
        if (!vivid::evidence::expect(trace.focus_in == 1 && trace.focus_in_expected, label)) return false;
        if (!vivid::evidence::expect(vivid::evidence::same_handle(access.input_focused(), new_target), label)) return false;
        return true;
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
        handles.origin = builder.create_scroll_container();
        handles.right = builder.create_scroll_container();
        handles.down = builder.create_scroll_container();
        handles.top = builder.create_scroll_container();
        handles.outside = builder.create_scroll_container();

        builder.link(handles.root, handles.scope);
        builder.link(handles.scope, handles.origin);
        builder.link(handles.scope, handles.right);
        builder.link(handles.scope, handles.down);
        builder.link(handles.scope, handles.top);
        builder.link(handles.root, handles.outside);

        builder.set_rect(handles.root, kSceneBounds);
        builder.set_rect(handles.scope, kScopeBounds);
        builder.set_rect(handles.origin, kOriginBounds);
        builder.set_rect(handles.right, kRightBounds);
        builder.set_rect(handles.down, kDownBounds);
        builder.set_rect(handles.top, kTopBounds);
        builder.set_rect(handles.outside, kOutsideBounds);
        builder.set_input_root(handles.root);
        builder.set_focus_scope(handles.scope, handles.origin, true);
        builder.set_root(handles.root);
    });

    run_log.case_begin("scope_model");
    std::printf(" focusable_inside=4 outside=1 policy=spatial_then_preorder keys=right,down,left,up,tab\n");

    vivid::evidence::mouse_down_center(scene, kOriginBounds, 10);
    auto initial = scene.access();
    const auto initial_trace =
        vivid::evidence::collect_pointer_focus_trace(initial, handles.origin, {}, handles.origin);
    if (!vivid::evidence::expect(initial_trace.focus_in == 1 && initial_trace.focus_in_expected,
                                 "origin receives initial FocusIn")) return 1;
    if (!vivid::evidence::expect(vivid::evidence::same_handle(initial.input_focused(), handles.origin),
                                 "initial focus truth is origin")) return 1;
    vivid::evidence::mouse_up_center(scene, kOriginBounds, 11);

    const auto origin_artifact = vivid::evidence::render_scene(scene, canvas, kOriginBounds);
    if (!vivid::evidence::expect(origin_artifact.failed_cmds == 0,
                                 "origin artifact has no failed commands")) return 1;

    run_log.case_begin("initial_focus");
    std::printf(" target=origin focus_in=%d input_truth=origin cmd_count=%zu pixel_hash=%u\n",
                initial_trace.focus_in,
                origin_artifact.cmd_count,
                origin_artifact.pixel_hash);

    scene.dispatch_event(Event::key(Event::Type::KeyDown, Event::Key::Right, 20));
    auto right_access = scene.access();
    if (!expect_focus_move(right_access, handles.origin, handles.right, "right chooses spatial right")) return 1;
    const auto right_trace = vivid::evidence::collect_focus_move(right_access, handles.origin, handles.right);
    run_log.case_begin("right_to_right");
    std::printf(" key=right old=origin new=right mode=spatial focus_out=%d focus_in=%d input_truth=right\n",
                right_trace.focus_out,
                right_trace.focus_in);

    scene.dispatch_event(Event::key(Event::Type::KeyDown, Event::Key::Down, 30));
    auto down_access = scene.access();
    if (!expect_focus_move(down_access, handles.right, handles.down, "down chooses spatial down")) return 1;
    const auto down_trace = vivid::evidence::collect_focus_move(down_access, handles.right, handles.down);
    run_log.case_begin("down_to_down");
    std::printf(" key=down old=right new=down mode=spatial focus_out=%d focus_in=%d input_truth=down\n",
                down_trace.focus_out,
                down_trace.focus_in);

    scene.dispatch_event(Event::key(Event::Type::KeyDown, Event::Key::Left, 40));
    auto left_access = scene.access();
    if (!expect_focus_move(left_access, handles.down, handles.origin, "left chooses spatial origin")) return 1;
    const auto left_trace = vivid::evidence::collect_focus_move(left_access, handles.down, handles.origin);
    run_log.case_begin("left_to_origin");
    std::printf(" key=left old=down new=origin mode=spatial focus_out=%d focus_in=%d input_truth=origin\n",
                left_trace.focus_out,
                left_trace.focus_in);

    scene.dispatch_event(Event::key(Event::Type::KeyDown, Event::Key::Up, 50));
    auto up_access = scene.access();
    if (!expect_focus_move(up_access, handles.origin, handles.top, "up chooses spatial top")) return 1;
    const auto up_trace = vivid::evidence::collect_focus_move(up_access, handles.origin, handles.top);
    run_log.case_begin("up_to_top");
    std::printf(" key=up old=origin new=top mode=spatial focus_out=%d focus_in=%d input_truth=top\n",
                up_trace.focus_out,
                up_trace.focus_in);

    scene.dispatch_event(Event::key(Event::Type::KeyDown, Event::Key::Tab, 60));
    auto tab_access = scene.access();
    if (!expect_focus_move(tab_access, handles.top, handles.origin, "tab keeps preorder wrap")) return 1;
    const auto tab_trace = vivid::evidence::collect_focus_move(tab_access, handles.top, handles.origin);
    run_log.case_begin("tab_preorder");
    std::printf(" key=tab old=top new=origin mode=preorder wrap=1 focus_out=%d focus_in=%d input_truth=origin\n",
                tab_trace.focus_out,
                tab_trace.focus_in);

    scene.dispatch_event(Event::key(Event::Type::KeyDown, Event::Key::Left, 70));
    auto wrap_access = scene.access();
    if (!expect_focus_move(wrap_access, handles.origin, handles.top, "left falls back to reverse preorder wrap")) return 1;
    const auto wrap_trace = vivid::evidence::collect_focus_move(wrap_access, handles.origin, handles.top);
    run_log.case_begin("no_candidate_wrap");
    std::printf(" key=left old=origin new=top mode=preorder fallback=1 wrap=1 focus_out=%d focus_in=%d input_truth=top\n",
                wrap_trace.focus_out,
                wrap_trace.focus_in);

    const auto outside_baseline = vivid::evidence::render_scene(scene, canvas, kOutsideBounds);
    if (!vivid::evidence::expect(outside_baseline.failed_cmds == 0,
                                 "outside baseline has no failed commands")) return 1;

    scene.dispatch_event(Event::key(Event::Type::KeyDown, Event::Key::Right, 80));
    auto outside_skip = scene.access();
    if (!expect_focus_move(outside_skip, handles.top, handles.right, "right chooses inside scope instead of outside")) return 1;
    if (!vivid::evidence::expect(!vivid::evidence::same_handle(outside_skip.input_focused(), handles.outside),
                                 "outside is not selected by spatial navigation")) return 1;
    const auto outside_after = vivid::evidence::render_scene(scene, canvas, kOutsideBounds);
    if (!vivid::evidence::expect(outside_after.cmd_hash == outside_baseline.cmd_hash,
                                 "outside command evidence stays baseline")) return 1;

    run_log.case_begin("outside_not_candidate");
    std::printf(" key=right old=top new=right outside_candidate=0 outside_focus_ring=0 outside_cmd_hash=%u outside_baseline_hash=%u\n",
                outside_after.cmd_hash,
                outside_baseline.cmd_hash);

    run_log.end(true);
    std::puts("[focus_spatial_navigation_demo] ok");
    return 0;
}
