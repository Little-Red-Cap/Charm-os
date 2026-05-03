#include <cstdio>
#include <cstring>

import charm.core.geometry;
import charm.core.style;
import charm.core.theme_preset;
import charm.gfx.canvas;
import charm.ui.scene;

#include "../support/vivid_evidence_support.hpp"

namespace {
    constexpr Rect kComponentBounds{8, 8, 224, 76};
    constexpr vivid::evidence::RunLog kRunLog{"csr", "component_settings_row_demo"};

    struct Handles {
        WidgetHandle root{};
        WidgetHandle row{};
        WidgetHandle title{};
        WidgetHandle slider{};
        WidgetHandle progress{};
        WidgetHandle value_label{};
    };

    struct SettingsRowState {
        int old_value{20};
        int value{20};
    };

    void set_value_label(::ui::scene::SceneAccess& access, WidgetHandle label, int value) noexcept {
        char text[16]{};
        std::snprintf(text, sizeof(text), "%d%%", value);
        access.set_text(label, text);
    }

    void apply_settings_row_state(::ui::scene::SceneAccess& access,
                                  const Handles& handles,
                                  const SettingsRowState& state) noexcept {
        access.set_value(handles.slider, state.value);
        access.set_value(handles.progress, state.value);
        set_value_label(access, handles.value_label, state.value);
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
        handles.row = builder.create_container();
        handles.title = builder.create_label_static("Brightness");
        handles.slider = builder.create_slider();
        handles.progress = builder.create_progress_bar_simple();
        handles.value_label = builder.create_label_static("20%");

        builder.link(handles.root, handles.row);
        builder.link(handles.row, handles.title);
        builder.link(handles.row, handles.slider);
        builder.link(handles.row, handles.progress);
        builder.link(handles.row, handles.value_label);

        builder.set_rect(handles.root, {0, 0, 240, 96});
        builder.set_rect(handles.row, kComponentBounds);
        builder.set_rect(handles.title, {16, 14, 120, 18});
        builder.set_rect(handles.value_label, {176, 14, 48, 18});
        builder.set_rect(handles.slider, {16, 42, 144, 18});
        builder.set_rect(handles.progress, {168, 46, 48, 10});
        builder.set_range(handles.slider, 0, 100);
        builder.set_range(handles.progress, 0, 100);
        builder.set_root(handles.root);
    });

    auto access = scene.access();
    SettingsRowState state{};
    apply_settings_row_state(access, handles, state);

    if (!vivid::evidence::expect(access.value(handles.slider) == 20, "initial slider truth")) return 1;
    if (!vivid::evidence::expect(access.value(handles.progress) == 20, "initial progress mirror truth")) return 1;
    if (!vivid::evidence::expect(std::strcmp(scene.text(handles.value_label), "20%") == 0,
                                 "initial value label truth")) {
        return 1;
    }

    const auto initial = vivid::evidence::render_scene(scene, canvas, Rect{0, 0, 240, 96});
    if (!vivid::evidence::expect(initial.failed_cmds == 0, "initial render has no failed commands")) return 1;
    if (!vivid::evidence::expect(initial.cmd_count > 0, "initial render records commands")) return 1;

    run_log.case_begin("initial_artifact");
    std::printf(" value=%d dirty_count=%zu cmd_count=%zu cmd_hash=%u pixel_hash=%u\n",
                state.value,
                initial.dirty_count,
                initial.cmd_count,
                initial.cmd_hash,
                initial.pixel_hash);

    state.old_value = state.value;
    state.value = 64;
    apply_settings_row_state(access, handles, state);

    if (!vivid::evidence::expect(access.value(handles.slider) == 64, "slider truth changes")) return 1;
    if (!vivid::evidence::expect(access.value(handles.progress) == 64, "progress mirrors slider truth")) return 1;
    if (!vivid::evidence::expect(std::strcmp(scene.text(handles.value_label), "64%") == 0,
                                 "value label derives from truth")) {
        return 1;
    }

    run_log.case_begin("state_delta");
    std::printf(" source=programmatic key=settings_row.value old=%d new=%d mirror=%d label=%s\n",
                state.old_value,
                state.value,
                access.value(handles.progress),
                scene.text(handles.value_label));

    run_log.case_begin("invalidation_intent");
    std::printf(" kind=paint_only component_x=%d component_y=%d component_w=%d component_h=%d\n",
                kComponentBounds.x,
                kComponentBounds.y,
                kComponentBounds.w,
                kComponentBounds.h);

    const auto updated = vivid::evidence::render_scene(scene, canvas, kComponentBounds);
    if (!vivid::evidence::expect(updated.failed_cmds == 0, "updated render has no failed commands")) return 1;
    if (!vivid::evidence::expect(updated.cmd_count > 0, "updated render records commands")) return 1;
    if (!vivid::evidence::expect(updated.pixel_hash != initial.pixel_hash,
                                 "state change affects render artifact")) {
        return 1;
    }
    if (!vivid::evidence::expect(updated.dirty_count == 1, "updated render keeps a single component dirty rect")) {
        return 1;
    }
    if (!vivid::evidence::expect(vivid::evidence::dirty_stays_inside(canvas, kComponentBounds),
                                 "dirty evidence remains inside component bounds")) {
        return 1;
    }

    run_log.case_begin("render_artifact");
    std::printf(" dirty_count=%zu dirty_hash=%u cmd_count=%zu cmd_bytes=%zu exec_cmds=%zu failed=%zu cmd_hash=%u pixel_hash=%u\n",
                updated.dirty_count,
                updated.dirty_hash,
                updated.cmd_count,
                updated.cmd_bytes,
                updated.exec_cmds,
                updated.failed_cmds,
                updated.cmd_hash,
                updated.pixel_hash);

    run_log.end(true);
    std::puts("[component_settings_row_demo] ok");
    return 0;
}
