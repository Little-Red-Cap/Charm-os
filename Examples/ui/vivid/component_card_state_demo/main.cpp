#include <cstdio>
#include <cstring>

import charm.core.geometry;
import charm.core.style;
import charm.core.theme_preset;
import charm.gfx.canvas;
import charm.ui.scene;

#include "../support/vivid_evidence_support.hpp"

namespace {
    constexpr Rect kCardBounds{8, 8, 224, 108};
    constexpr vivid::evidence::RunLog kRunLog{"ccs", "component_card_state_demo"};

    struct Handles {
        WidgetHandle root{};
        WidgetHandle card{};
        WidgetHandle title{};
        WidgetHandle enabled{};
        WidgetHandle level{};
        WidgetHandle output{};
        WidgetHandle summary{};
    };

    struct CardState {
        bool enabled{false};
        bool old_enabled{false};
        int level{30};
        int old_level{30};
        int output{0};
    };

    [[nodiscard]] int output_for(bool enabled, int level) noexcept {
        return enabled ? level : 0;
    }

    void set_summary(::ui::scene::SceneAccess& access,
                     WidgetHandle summary,
                     const CardState& state) noexcept {
        char text[64]{};
        std::snprintf(text,
                      sizeof(text),
                      "enabled=%d level=%d output=%d",
                      state.enabled ? 1 : 0,
                      state.level,
                      state.output);
        access.set_text(summary, text);
    }

    void apply_card_state(::ui::scene::SceneAccess& access,
                          const Handles& handles,
                          CardState& state) noexcept {
        state.output = output_for(state.enabled, state.level);
        access.set_checked(handles.enabled, state.enabled);
        access.set_value(handles.level, state.level);
        access.set_value(handles.output, state.output);
        set_summary(access, handles.summary, state);
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
        handles.card = builder.create_container();
        handles.title = builder.create_label_static("Power Card");
        handles.enabled = builder.create_checkbox("Enabled");
        handles.level = builder.create_slider();
        handles.output = builder.create_progress_bar_simple();
        handles.summary = builder.create_label_static("");

        builder.link(handles.root, handles.card);
        builder.link(handles.card, handles.title);
        builder.link(handles.card, handles.enabled);
        builder.link(handles.card, handles.level);
        builder.link(handles.card, handles.output);
        builder.link(handles.card, handles.summary);

        builder.set_rect(handles.root, {0, 0, 240, 128});
        builder.set_rect(handles.card, kCardBounds);
        builder.set_rect(handles.title, {16, 14, 128, 18});
        builder.set_rect(handles.enabled, {16, 38, 116, 24});
        builder.set_rect(handles.level, {16, 70, 144, 18});
        builder.set_rect(handles.output, {168, 74, 48, 10});
        builder.set_rect(handles.summary, {16, 92, 200, 16});
        builder.set_range(handles.level, 0, 100);
        builder.set_range(handles.output, 0, 100);
        builder.set_root(handles.root);
    });

    auto access = scene.access();
    CardState state{};
    apply_card_state(access, handles, state);

    if (!vivid::evidence::expect(!access.checked(handles.enabled), "card starts disabled")) return 1;
    if (!vivid::evidence::expect(access.value(handles.level) == 30, "card starts with level truth")) return 1;
    if (!vivid::evidence::expect(access.value(handles.output) == 0, "disabled card gates output")) return 1;
    if (!vivid::evidence::expect(std::strcmp(scene.text(handles.summary), "enabled=0 level=30 output=0") == 0,
                                 "initial summary derives card state")) {
        return 1;
    }

    const auto initial = vivid::evidence::render_scene(scene, canvas, Rect{0, 0, 240, 128});
    if (!vivid::evidence::expect(initial.failed_cmds == 0, "initial render has no failed commands")) return 1;
    if (!vivid::evidence::expect(initial.cmd_count > 0, "initial render records commands")) return 1;

    run_log.case_begin("initial_artifact");
    std::printf(" enabled=%d level=%d output=%d dirty_count=%zu cmd_count=%zu cmd_hash=%u pixel_hash=%u\n",
                state.enabled ? 1 : 0,
                state.level,
                state.output,
                initial.dirty_count,
                initial.cmd_count,
                initial.cmd_hash,
                initial.pixel_hash);

    state.old_enabled = state.enabled;
    state.old_level = state.level;
    state.enabled = true;
    state.level = 72;
    apply_card_state(access, handles, state);

    if (!vivid::evidence::expect(access.checked(handles.enabled), "enabled child truth changes")) return 1;
    if (!vivid::evidence::expect(access.value(handles.level) == 72, "slider child truth changes")) return 1;
    if (!vivid::evidence::expect(access.value(handles.output) == 72, "progress child derives output")) return 1;
    if (!vivid::evidence::expect(std::strcmp(scene.text(handles.summary), "enabled=1 level=72 output=72") == 0,
                                 "summary derives combined card state")) {
        return 1;
    }

    run_log.case_begin("state_delta");
    vivid::evidence::print_named_state_delta("enabled", {
        .id = "power_card.enabled",
        .key = "checked",
        .source = "programmatic",
        .old_value = state.old_enabled ? 1 : 0,
        .new_value = state.enabled ? 1 : 0,
    });
    vivid::evidence::print_named_state_delta("level", {
        .id = "power_card.level",
        .key = "value",
        .source = "programmatic",
        .old_value = state.old_level,
        .new_value = state.level,
    });
    std::printf(" output=%d\n", state.output);

    run_log.case_begin("component_derivation");
    std::printf(" children=3 summary=\"%s\" output_mirror=%d\n",
                scene.text(handles.summary),
                access.value(handles.output));

    run_log.case_begin("invalidation_intent");
    vivid::evidence::print_invalidation({
        .kind = "paint_only",
        .dirty_scope = "component",
        .component_bounds = kCardBounds,
        .layout_changed = false,
    });
    std::printf("\n");

    const auto updated = vivid::evidence::render_scene(scene, canvas, kCardBounds);
    if (!vivid::evidence::expect(updated.failed_cmds == 0, "updated render has no failed commands")) return 1;
    if (!vivid::evidence::expect(updated.cmd_count > 0, "updated render records commands")) return 1;
    if (!vivid::evidence::expect(updated.pixel_hash != initial.pixel_hash,
                                 "card state changes render artifact")) {
        return 1;
    }
    if (!vivid::evidence::expect(updated.dirty_count == 1, "updated render keeps a single card dirty rect")) {
        return 1;
    }
    if (!vivid::evidence::expect(vivid::evidence::dirty_stays_inside(canvas, kCardBounds),
                                 "dirty evidence remains inside card bounds")) {
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
    std::puts("[component_card_state_demo] ok");
    return 0;
}
