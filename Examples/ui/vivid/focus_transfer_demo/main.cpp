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
    constexpr Rect kSceneBounds{0, 0, 220, 112};
    constexpr Rect kSourceBounds{20, 16, 152, 36};
    constexpr Rect kDestinationBounds{20, 62, 152, 36};
    constexpr vivid::evidence::RunLog kRunLog{"ft", "focus_transfer_demo"};

    [[nodiscard]] bool same_handle(WidgetHandle lhs, WidgetHandle rhs) noexcept {
        return lhs.kind == rhs.kind
            && lhs.index == rhs.index
            && lhs.generation == rhs.generation;
    }

    [[nodiscard]] bool style_evidence_equal(const ResolvedStyleEvidence& lhs,
                                            const ResolvedStyleEvidence& rhs) noexcept {
        return lhs.style_key == rhs.style_key
            && lhs.color_hash == rhs.color_hash
            && lhs.metrics_hash == rhs.metrics_hash;
    }

    void click(::ui::scene::Scene& scene, Rect bounds, std::uint32_t ms) {
        const int x = bounds.x + bounds.w / 2;
        const int y = bounds.y + bounds.h / 2;
        scene.dispatch_event(Event::mouse(Event::Type::MouseDown, x, y, 1, ms));
        scene.dispatch_event(Event::mouse(Event::Type::MouseUp, x, y, 1, ms + 1));
    }
}

int main() {
    auto run_log = kRunLog;
    run_log.begin();
    vivid::evidence::prepare_style_sheet();

    auto& sheet = StyleSheet::instance();
    const auto token_version = Theme::instance().get_tokens().version;
    const auto stylesheet_version = sheet.stylesheet_version();

    static DefaultFrameBuffer fb{};
    static DefaultCanvas canvas{fb};
    static ::ui::scene::Scene scene{canvas};
    WidgetHandle source{};
    WidgetHandle destination{};

    scene.build([&](::ui::scene::SceneBuilder& builder) {
        const auto root = builder.create_container();
        source = builder.create_scroll_container();
        destination = builder.create_scroll_container();

        builder.link(root, source);
        builder.link(root, destination);
        builder.set_rect(root, kSceneBounds);
        builder.set_rect(source, kSourceBounds);
        builder.set_rect(destination, kDestinationBounds);
        builder.set_input_root(root);
        builder.set_root(root);
    });

    const StyleStateEvidence state_evidence = make_style_state_evidence(WidgetKind::ScrollContainer);
    if (!vivid::evidence::expect(!state_evidence.includes_focused,
                                 "scroll container style state keeps focus outside style mask")) {
        return 1;
    }

    run_log.case_begin("style_mask_boundary");
    vivid::evidence::print_style_state_mask("scroll_container",
                                            "focus_outside_style_mask",
                                            state_evidence);
    std::printf("\n");

    const StyleState normal_state = make_style_state(true, false, false, false);
    const StyleState focused_state = make_style_state(true, false, false, true);
    const auto normal_style = sheet.lookup(WidgetKind::ScrollContainer, normal_state);
    const auto focused_style = sheet.lookup(WidgetKind::ScrollContainer, focused_state);
    if (!vivid::evidence::expect(normal_style.colors != nullptr, "normal style has colors")) return 1;
    if (!vivid::evidence::expect(normal_style.metrics != nullptr, "normal style has metrics")) return 1;
    if (!vivid::evidence::expect(focused_style.colors != nullptr, "focused style has colors")) return 1;
    if (!vivid::evidence::expect(focused_style.metrics != nullptr, "focused style has metrics")) return 1;

    const ResolvedStyleEvidence style_before = make_resolved_style_evidence(normal_style);
    const ResolvedStyleEvidence style_focus_lookup = make_resolved_style_evidence(focused_style);
    if (!vivid::evidence::expect(style_evidence_equal(style_before, style_focus_lookup),
                                 "focused lookup keeps scroll container style evidence stable")) {
        return 1;
    }

    auto access = scene.access();
    click(scene, kSourceBounds, 10);
    auto access_after_source_click = scene.access();
    if (!vivid::evidence::expect(same_handle(access_after_source_click.input_focused(), source),
                                 "source receives initial focus")) {
        return 1;
    }

    const auto source_artifact = vivid::evidence::render_scene(scene, canvas, kSourceBounds);
    if (!vivid::evidence::expect(source_artifact.failed_cmds == 0, "source focused render has no failed commands")) {
        return 1;
    }
    if (!vivid::evidence::expect(source_artifact.cmd_count > 0, "source focused render records commands")) return 1;

    run_log.case_begin("initial_focus_artifact");
    std::printf(" target=source focus=1 dirty_count=%zu cmd_count=%zu cmd_hash=%u pixel_hash=%u token_version=%u stylesheet_version=%u\n",
                source_artifact.dirty_count,
                source_artifact.cmd_count,
                source_artifact.cmd_hash,
                source_artifact.pixel_hash,
                token_version,
                stylesheet_version);

    scene.dispatch_event(Event::mouse(Event::Type::MouseDown,
                                      kDestinationBounds.x + kDestinationBounds.w / 2,
                                      kDestinationBounds.y + kDestinationBounds.h / 2,
                                      1,
                                      20));
    auto transfer_access = scene.access();
    int focus_out = 0;
    int focus_in = 0;
    int mouse_down = 0;
    bool focus_out_source = false;
    bool focus_in_destination = false;
    for (std::size_t index = 0; index < transfer_access.input_event_count(); ++index) {
        const auto& event = transfer_access.input_event(index);
        if (event.event.type == Event::Type::MouseDown) {
            ++mouse_down;
        } else if (event.event.type == Event::Type::FocusOut) {
            ++focus_out;
            focus_out_source = focus_out_source || same_handle(event.target, source);
        } else if (event.event.type == Event::Type::FocusIn) {
            ++focus_in;
            focus_in_destination = focus_in_destination || same_handle(event.target, destination);
        }
    }
    if (!vivid::evidence::expect(mouse_down == 1, "transfer emits destination mouse down")) return 1;
    if (!vivid::evidence::expect(focus_out == 1 && focus_out_source, "transfer emits source FocusOut")) return 1;
    if (!vivid::evidence::expect(focus_in == 1 && focus_in_destination, "transfer emits destination FocusIn")) return 1;
    if (!vivid::evidence::expect(same_handle(transfer_access.input_focused(), destination),
                                 "destination becomes focused during transfer")) {
        return 1;
    }

    run_log.case_begin("transfer_event_trace");
    std::printf(" source=mouse_down old=source new=destination mouse_down=%d focus_out=%d focus_in=%d focus_out_source=%d focus_in_destination=%d\n",
                mouse_down,
                focus_out,
                focus_in,
                focus_out_source ? 1 : 0,
                focus_in_destination ? 1 : 0);

    scene.dispatch_event(Event::mouse(Event::Type::MouseUp,
                                      kDestinationBounds.x + kDestinationBounds.w / 2,
                                      kDestinationBounds.y + kDestinationBounds.h / 2,
                                      1,
                                      21));
    auto after_transfer = scene.access();
    if (!vivid::evidence::expect(same_handle(after_transfer.input_focused(), destination),
                                 "destination remains focused after release")) {
        return 1;
    }

    run_log.case_begin("focus_truth_after_transfer");
    std::printf(" old=source new=destination focused_kind=%s focused_index=%u transfer_committed=1\n",
                widget_kind_name(after_transfer.input_focused().kind),
                static_cast<unsigned>(after_transfer.input_focused().index));

    const auto focused_after_transfer = sheet.lookup(WidgetKind::ScrollContainer, focused_state);
    if (!vivid::evidence::expect(focused_after_transfer.colors != nullptr, "focused after transfer has colors")) {
        return 1;
    }
    if (!vivid::evidence::expect(focused_after_transfer.metrics != nullptr, "focused after transfer has metrics")) {
        return 1;
    }
    const ResolvedStyleEvidence style_after = make_resolved_style_evidence(focused_after_transfer);
    if (!vivid::evidence::expect(style_evidence_equal(style_before, style_after),
                                 "focus transfer keeps style evidence stable")) {
        return 1;
    }

    run_log.case_begin("style_evidence_after_transfer");
    vivid::evidence::print_focus_style_evidence("scroll_container",
                                                true,
                                                style_after,
                                                true,
                                                state_evidence.includes_focused);
    std::printf("\n");

    const auto destination_artifact = vivid::evidence::render_scene(scene, canvas, kDestinationBounds);
    if (!vivid::evidence::expect(destination_artifact.failed_cmds == 0,
                                 "destination focused render has no failed commands")) {
        return 1;
    }
    if (!vivid::evidence::expect(destination_artifact.pixel_hash != source_artifact.pixel_hash,
                                 "focus transfer changes render artifact")) {
        return 1;
    }
    if (!vivid::evidence::expect(destination_artifact.dirty_count == 1,
                                 "destination focus repaint uses single dirty rect")) {
        return 1;
    }
    if (!vivid::evidence::expect(vivid::evidence::dirty_stays_inside(canvas, kDestinationBounds),
                                 "destination dirty evidence remains inside bounds")) {
        return 1;
    }

    run_log.case_begin("render_artifact_after_transfer");
    std::printf(" target=destination dirty_count=%zu dirty_hash=%u cmd_hash_old=%u cmd_hash_new=%u pixel_hash_old=%u pixel_hash_new=%u artifact_changed=1 focus_ring=1\n",
                destination_artifact.dirty_count,
                destination_artifact.dirty_hash,
                source_artifact.cmd_hash,
                destination_artifact.cmd_hash,
                source_artifact.pixel_hash,
                destination_artifact.pixel_hash);

    after_transfer.set_focused(destination, false);
    const auto cleared_artifact = vivid::evidence::render_scene(scene, canvas, kDestinationBounds);
    if (!vivid::evidence::expect(cleared_artifact.failed_cmds == 0, "cleared destination render has no failed commands")) {
        return 1;
    }
    if (!vivid::evidence::expect(cleared_artifact.cmd_count + 1 == destination_artifact.cmd_count,
                                 "clearing destination focus removes one focus command")) {
        return 1;
    }
    if (!vivid::evidence::expect(cleared_artifact.pixel_hash != destination_artifact.pixel_hash,
                                 "clearing destination focus changes artifact")) {
        return 1;
    }

    run_log.case_begin("clear_destination_focus");
    std::printf(" target=destination focused_old=1 focused_new=0 cmd_count=%zu cmd_hash=%u pixel_hash=%u focus_ring=0\n",
                cleared_artifact.cmd_count,
                cleared_artifact.cmd_hash,
                cleared_artifact.pixel_hash);

    run_log.end(true);
    std::puts("[focus_transfer_demo] ok");
    return 0;
}
