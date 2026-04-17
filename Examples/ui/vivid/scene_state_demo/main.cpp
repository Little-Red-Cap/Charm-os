#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstring>

import charm.core.event;
import charm.core.handle;
import charm.gfx.canvas;
import charm.ui.scene;

namespace {
    struct Handles {
        WidgetHandle root{};
        WidgetHandle summary{};
        WidgetHandle armed_checkbox{};
        WidgetHandle boost_button{};
        WidgetHandle armed_mirror{};
        WidgetHandle level_progress{};
    };

    struct AppState {
        bool armed{false};
        int level{20};
        int boost_presses{0};
    };

    [[nodiscard]] const char* on_off(bool value) noexcept {
        return value ? "on" : "off";
    }

    [[nodiscard]] bool expect(bool condition, const char* message) noexcept {
        if (!condition) {
            std::printf("[ERR] %s\n", message);
            return false;
        }
        return true;
    }

    [[nodiscard]] int output_value(const AppState& state) noexcept {
        return state.armed ? state.level : 0;
    }

    [[nodiscard]] const char* event_type_name(Event::Type type) noexcept {
        switch (type) {
        case Event::Type::HoverEnter: return "HoverEnter";
        case Event::Type::HoverLeave: return "HoverLeave";
        case Event::Type::MouseDown: return "MouseDown";
        case Event::Type::MouseUp: return "MouseUp";
        case Event::Type::MouseMove: return "MouseMove";
        case Event::Type::MouseWheel: return "MouseWheel";
        case Event::Type::Click: return "Click";
        case Event::Type::DragStart: return "DragStart";
        case Event::Type::DragMove: return "DragMove";
        case Event::Type::DragEnd: return "DragEnd";
        case Event::Type::GestureSwipe: return "GestureSwipe";
        case Event::Type::GesturePinch: return "GesturePinch";
        case Event::Type::FocusIn: return "FocusIn";
        case Event::Type::FocusOut: return "FocusOut";
        case Event::Type::KeyDown: return "KeyDown";
        case Event::Type::KeyUp: return "KeyUp";
        case Event::Type::Cancel: return "Cancel";
        }
        return "Unknown";
    }

    void apply_scene_model(::ui::scene::SceneAccess& access,
                           const Handles& handles,
                           const AppState& state) noexcept {
        char summary[96]{};
        std::snprintf(summary,
                      sizeof(summary),
                      "armed=%s level=%d output=%d boosts=%d",
                      on_off(state.armed),
                      state.level,
                      output_value(state),
                      state.boost_presses);
        access.set_checked(handles.armed_checkbox, state.armed);
        access.set_checked(handles.armed_mirror, state.armed);
        access.set_value(handles.level_progress, output_value(state));
        access.set_text(handles.summary, summary);
    }

    void dump_dispatch(::ui::scene::SceneAccess& access, const char* tag) noexcept {
        std::printf("[%s] events=%zu\n", tag, access.input_event_count());
        for (std::size_t i = 0; i < access.input_event_count(); ++i) {
            const auto& ev = access.input_event(i);
            std::printf("  - %s#%u type=%s\n",
                        widget_kind_name(ev.target.kind),
                        static_cast<unsigned>(ev.target.index),
                        event_type_name(ev.event.type));
        }
    }

    void drive_controller(::ui::scene::SceneAccess& access,
                          const Handles& handles,
                          AppState& state) noexcept {
        for (std::size_t i = 0; i < access.input_event_count(); ++i) {
            const auto& ev = access.input_event(i);
            if (ev.event.type != Event::Type::MouseUp) continue;
            if (ev.target == handles.armed_checkbox) {
                state.armed = access.checked(handles.armed_checkbox);
            } else if (ev.target == handles.boost_button) {
                state.boost_presses += 1;
                state.level = std::min(100, state.level + 15);
            }
        }
        apply_scene_model(access, handles, state);
    }

    void dispatch_click(::ui::scene::Scene& scene,
                        ::ui::scene::SceneAccess& access,
                        WidgetHandle handle) noexcept {
        const Rect rect = access.world_rect(handle);
        const int x = rect.x + rect.w / 2;
        const int y = rect.y + rect.h / 2;
        scene.dispatch_event(Event::mouse(Event::Type::MouseMove, x, y, 0));
        scene.dispatch_event(Event::mouse(Event::Type::MouseDown, x, y, 1));
        scene.dispatch_event(Event::mouse(Event::Type::MouseUp, x, y, 1));
    }

    [[nodiscard]] bool saw_release_for(::ui::scene::SceneAccess& access, WidgetHandle handle) noexcept {
        for (std::size_t i = 0; i < access.input_event_count(); ++i) {
            const auto& ev = access.input_event(i);
            if (ev.target == handle && ev.event.type == Event::Type::MouseUp) {
                return true;
            }
        }
        return false;
    }
}

int main() {
    static DefaultFrameBuffer fb{};
    static DefaultCanvas canvas{fb};
    static ::ui::scene::Scene scene{canvas};
    Handles handles{};

    scene.build([&](::ui::scene::SceneBuilder& builder) {
        handles.root = builder.create_container();
        builder.set_rect(handles.root, {0, 0, 240, 160});

        handles.summary = builder.create_label_static("");
        handles.armed_checkbox = builder.create_checkbox("Armed");
        handles.boost_button = builder.create_button_static("Boost +15");
        handles.armed_mirror = builder.create_switch();
        handles.level_progress = builder.create_progress_bar_simple();

        builder.link(handles.root, handles.summary);
        builder.link(handles.root, handles.armed_checkbox);
        builder.link(handles.root, handles.boost_button);
        builder.link(handles.root, handles.armed_mirror);
        builder.link(handles.root, handles.level_progress);

        builder.set_rect(handles.summary, {12, 12, 216, 18});
        builder.set_rect(handles.armed_checkbox, {12, 44, 108, 28});
        builder.set_rect(handles.boost_button, {128, 40, 96, 32});
        builder.set_rect(handles.armed_mirror, {12, 84, 72, 28});
        builder.set_rect(handles.level_progress, {12, 124, 216, 16});

        builder.set_hit_testable(handles.armed_mirror, false);
        builder.set_range(handles.level_progress, 0, 100);
        builder.set_input_root(handles.root);
        builder.set_root(handles.root);
    });

    auto access = scene.access();
    AppState state{};
    apply_scene_model(access, handles, state);

    if (!expect(!access.checked(handles.armed_checkbox), "checkbox starts unchecked")) return 1;
    if (!expect(!access.checked(handles.armed_mirror), "mirror switch starts unchecked")) return 1;
    if (!expect(access.value(handles.level_progress) == 0, "progress starts gated by app-state")) return 1;
    if (!expect(std::strcmp(scene.text(handles.summary), "armed=off level=20 output=0 boosts=0") == 0,
                "summary starts from controller-applied truth")) {
        return 1;
    }

    dispatch_click(scene, access, handles.armed_checkbox);
    dump_dispatch(access, "click-armed");
    if (!expect(saw_release_for(access, handles.armed_checkbox), "checkbox release appears in current dispatch")) return 1;
    drive_controller(access, handles, state);
    if (!expect(state.armed, "controller syncs armed truth from checkbox")) return 1;
    if (!expect(access.checked(handles.armed_mirror), "controller mirrors armed truth into switch")) return 1;
    if (!expect(access.value(handles.level_progress) == 20, "controller exposes current level when armed")) return 1;

    dispatch_click(scene, access, handles.boost_button);
    dump_dispatch(access, "click-boost-1");
    if (!expect(saw_release_for(access, handles.boost_button), "boost button release appears in current dispatch")) return 1;
    if (!expect(!saw_release_for(access, handles.armed_checkbox), "previous dispatch events do not leak forward")) return 1;
    drive_controller(access, handles, state);
    if (!expect(state.boost_presses == 1, "boost click updates app-state")) return 1;
    if (!expect(state.level == 35, "boost click advances logical level")) return 1;
    if (!expect(access.value(handles.level_progress) == 35, "controller writes derived output through SceneAccess")) return 1;

    dispatch_click(scene, access, handles.armed_checkbox);
    dump_dispatch(access, "click-disarm");
    if (!expect(saw_release_for(access, handles.armed_checkbox), "disarm release appears in current dispatch")) return 1;
    drive_controller(access, handles, state);
    if (!expect(!state.armed, "controller syncs disarm truth from checkbox")) return 1;
    if (!expect(!access.checked(handles.armed_mirror), "mirror switch tracks disarmed state")) return 1;
    if (!expect(access.value(handles.level_progress) == 0, "derived output is gated off after disarm")) return 1;
    if (!expect(std::strcmp(scene.text(handles.summary), "armed=off level=35 output=0 boosts=1") == 0,
                "summary keeps app-state and derived output explicit")) {
        return 1;
    }

    std::printf("[summary] %s\n", scene.text(handles.summary));
    std::printf("[progress] value=%d\n", access.value(handles.level_progress));
    std::printf("[scene_state_demo] ok\n");
    return 0;
}
