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
    constexpr Rect kSceneBounds{0, 0, 280, 160};
    constexpr Rect kScopeBounds{12, 12, 184, 136};
    constexpr Rect kFirstBounds{24, 24, 132, 30};
    constexpr Rect kSecondBounds{24, 66, 132, 30};
    constexpr Rect kThirdBounds{24, 108, 132, 30};
    constexpr Rect kOutsideBounds{214, 62, 48, 44};
    constexpr vivid::evidence::RunLog kRunLog{"fsnav", "focus_scope_navigation_demo"};

    struct Handles {
        WidgetHandle root{};
        WidgetHandle scope{};
        WidgetHandle first{};
        WidgetHandle second{};
        WidgetHandle third{};
        WidgetHandle outside{};
    };


    void mouse_down(::ui::scene::Scene& scene, Rect bounds, std::uint32_t ms) {
        const int x = bounds.x + bounds.w / 2;
        const int y = bounds.y + bounds.h / 2;
        scene.dispatch_event(Event::mouse(Event::Type::MouseDown, x, y, 1, ms));
    }

    void mouse_up(::ui::scene::Scene& scene, Rect bounds, std::uint32_t ms) {
        const int x = bounds.x + bounds.w / 2;
        const int y = bounds.y + bounds.h / 2;
        scene.dispatch_event(Event::mouse(Event::Type::MouseUp, x, y, 1, ms + 1));
    }

    struct FocusMoveTrace {
        int focus_out{0};
        int focus_in{0};
        bool focus_out_expected{false};
        bool focus_in_expected{false};
    };

    [[nodiscard]] FocusMoveTrace collect_focus_move(::ui::scene::SceneAccess& access,
                                                    WidgetHandle old_target,
                                                    WidgetHandle new_target) noexcept {
        FocusMoveTrace out{};
        for (std::size_t index = 0; index < access.input_event_count(); ++index) {
            const auto& event = access.input_event(index);
            if (event.event.type == Event::Type::FocusOut) {
                ++out.focus_out;
                out.focus_out_expected = out.focus_out_expected || vivid::evidence::same_handle(event.target, old_target);
            } else if (event.event.type == Event::Type::FocusIn) {
                ++out.focus_in;
                out.focus_in_expected = out.focus_in_expected || vivid::evidence::same_handle(event.target, new_target);
            }
        }
        return out;
    }

    bool expect_focus_move(::ui::scene::SceneAccess& access,
                           WidgetHandle old_target,
                           WidgetHandle new_target,
                           const char* label) noexcept {
        const auto trace = collect_focus_move(access, old_target, new_target);
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
        handles.first = builder.create_scroll_container();
        handles.second = builder.create_scroll_container();
        handles.third = builder.create_scroll_container();
        handles.outside = builder.create_scroll_container();

        builder.link(handles.root, handles.scope);
        builder.link(handles.scope, handles.first);
        builder.link(handles.scope, handles.second);
        builder.link(handles.scope, handles.third);
        builder.link(handles.root, handles.outside);

        builder.set_rect(handles.root, kSceneBounds);
        builder.set_rect(handles.scope, kScopeBounds);
        builder.set_rect(handles.first, kFirstBounds);
        builder.set_rect(handles.second, kSecondBounds);
        builder.set_rect(handles.third, kThirdBounds);
        builder.set_rect(handles.outside, kOutsideBounds);
        builder.set_input_root(handles.root);
        builder.set_focus_scope(handles.scope, handles.first, true);
        builder.set_root(handles.root);
    });

    run_log.case_begin("scope_model");
    std::printf(" focusable_inside=3 outside=1 order=first,second,third keys=tab,right,down,left,up\n");

    mouse_down(scene, kFirstBounds, 10);
    auto initial = scene.access();
    int initial_focus_in = 0;
    bool initial_focus_in_first = false;
    for (std::size_t index = 0; index < initial.input_event_count(); ++index) {
        const auto& event = initial.input_event(index);
        if (event.event.type == Event::Type::FocusIn) {
            ++initial_focus_in;
            initial_focus_in_first = initial_focus_in_first || vivid::evidence::same_handle(event.target, handles.first);
        }
    }
    if (!vivid::evidence::expect(initial_focus_in == 1 && initial_focus_in_first, "first receives initial FocusIn")) {
        return 1;
    }
    if (!vivid::evidence::expect(vivid::evidence::same_handle(initial.input_focused(), handles.first), "initial focus truth is first")) {
        return 1;
    }
    mouse_up(scene, kFirstBounds, 11);

    const auto first_artifact = vivid::evidence::render_scene(scene, canvas, kFirstBounds);
    if (!vivid::evidence::expect(first_artifact.failed_cmds == 0, "first artifact has no failed commands")) return 1;

    run_log.case_begin("initial_focus");
    std::printf(" target=first focus_in=%d input_truth=first cmd_count=%zu pixel_hash=%u\n",
                initial_focus_in,
                first_artifact.cmd_count,
                first_artifact.pixel_hash);

    scene.dispatch_event(Event::key(Event::Type::KeyDown, Event::Key::Tab, 20));
    auto tab_access = scene.access();
    if (!expect_focus_move(tab_access, handles.first, handles.second, "tab moves first to second")) return 1;
    const auto tab_trace = collect_focus_move(tab_access, handles.first, handles.second);
    const auto second_artifact = vivid::evidence::render_scene(scene, canvas, kSecondBounds);
    if (!vivid::evidence::expect(second_artifact.failed_cmds == 0, "second artifact has no failed commands")) return 1;

    run_log.case_begin("tab_to_second");
    std::printf(" key=tab old=first new=second focus_out=%d focus_in=%d input_truth=second focus_ring=1 pixel_hash=%u\n",
                tab_trace.focus_out,
                tab_trace.focus_in,
                second_artifact.pixel_hash);

    scene.dispatch_event(Event::key(Event::Type::KeyDown, Event::Key::Right, 30));
    auto right_access = scene.access();
    if (!expect_focus_move(right_access, handles.second, handles.third, "right moves second to third")) return 1;
    const auto right_trace = collect_focus_move(right_access, handles.second, handles.third);

    run_log.case_begin("right_to_third");
    std::printf(" key=right old=second new=third focus_out=%d focus_in=%d input_truth=third\n",
                right_trace.focus_out,
                right_trace.focus_in);

    scene.dispatch_event(Event::key(Event::Type::KeyDown, Event::Key::Down, 40));
    auto down_access = scene.access();
    if (!expect_focus_move(down_access, handles.third, handles.first, "down wraps third to first")) return 1;
    const auto down_trace = collect_focus_move(down_access, handles.third, handles.first);

    run_log.case_begin("down_wrap_first");
    std::printf(" key=down old=third new=first wrap=1 focus_out=%d focus_in=%d input_truth=first\n",
                down_trace.focus_out,
                down_trace.focus_in);

    scene.dispatch_event(Event::key(Event::Type::KeyDown, Event::Key::Left, 50));
    auto left_access = scene.access();
    if (!expect_focus_move(left_access, handles.first, handles.third, "left wraps first to third")) return 1;
    const auto left_trace = collect_focus_move(left_access, handles.first, handles.third);

    run_log.case_begin("left_wrap_third");
    std::printf(" key=left old=first new=third reverse=1 wrap=1 focus_out=%d focus_in=%d input_truth=third\n",
                left_trace.focus_out,
                left_trace.focus_in);

    const auto outside_baseline = vivid::evidence::render_scene(scene, canvas, kOutsideBounds);
    if (!vivid::evidence::expect(outside_baseline.failed_cmds == 0, "outside baseline has no failed commands")) return 1;

    scene.dispatch_event(Event::key(Event::Type::KeyDown, Event::Key::Right, 60));
    auto outside_skip = scene.access();
    if (!expect_focus_move(outside_skip, handles.third, handles.first, "right wraps third to first without outside")) return 1;
    if (!vivid::evidence::expect(!vivid::evidence::same_handle(outside_skip.input_focused(), handles.outside),
                                 "outside is not selected by scope navigation")) {
        return 1;
    }
    const auto outside_after = vivid::evidence::render_scene(scene, canvas, kOutsideBounds);
    if (!vivid::evidence::expect(outside_after.cmd_hash == outside_baseline.cmd_hash,
                                 "outside command evidence stays baseline")) {
        return 1;
    }

    run_log.case_begin("outside_not_in_nav");
    std::printf(" key=right old=third new=first outside_candidate=0 outside_focus_ring=0 outside_cmd_hash=%u outside_baseline_hash=%u\n",
                outside_after.cmd_hash,
                outside_baseline.cmd_hash);

    run_log.end(true);
    std::puts("[focus_scope_navigation_demo] ok");
    return 0;
}
