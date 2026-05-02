#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

import charm.core.geometry;
import charm.core.style;
import charm.core.theme_preset;
import charm.gfx.canvas;
import charm.ui.scene;

namespace {
    constexpr Rect kCardBounds{8, 8, 224, 108};

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

    struct RenderEvidence {
        std::size_t cmd_count{0};
        std::size_t cmd_bytes{0};
        std::size_t exec_cmds{0};
        std::size_t failed_cmds{0};
        std::size_t dirty_count{0};
        std::uint32_t dirty_hash{0};
        std::uint32_t cmd_hash{0};
        std::uint32_t pixel_hash{0};
    };

    unsigned component_card_summary_case_count{0};

    void print_component_card_run_begin() noexcept {
        std::printf("[ccs] run=component_card_state_demo phase=begin\n");
    }

    void print_component_card_run_end(bool ok) noexcept {
        std::printf("[ccs] run=component_card_state_demo phase=end result=%s cases=%u\n",
                    ok ? "ok" : "fail",
                    component_card_summary_case_count);
    }

    void print_component_card_case(const char* name) noexcept {
        ++component_card_summary_case_count;
        std::printf("[ccs] case=%s", name);
    }

    [[nodiscard]] bool expect(bool condition, const char* message) noexcept {
        if (!condition) {
            std::printf("[ERR] %s\n", message);
            return false;
        }
        return true;
    }

    [[nodiscard]] int output_for(bool enabled, int level) noexcept {
        return enabled ? level : 0;
    }

    [[nodiscard]] std::uint32_t hash_mix(std::uint32_t hash, std::uint32_t value) noexcept {
        hash ^= value;
        hash *= 16777619u;
        return hash;
    }

    [[nodiscard]] std::uint32_t hash_bytes(const std::byte* data, std::size_t len) noexcept {
        std::uint32_t hash = 2166136261u;
        if (!data) return hash;
        for (std::size_t i = 0; i < len; ++i) {
            hash ^= static_cast<std::uint8_t>(data[i]);
            hash *= 16777619u;
        }
        return hash;
    }

    [[nodiscard]] std::uint32_t hash_dirty(const DefaultCanvas& canvas) noexcept {
        std::uint32_t hash = 2166136261u;
        const auto& dirty = canvas.dirty_list();
        hash = hash_mix(hash, static_cast<std::uint32_t>(dirty.full() ? 1u : 0u));
        hash = hash_mix(hash, static_cast<std::uint32_t>(dirty.size()));
        for (std::size_t i = 0; i < dirty.size(); ++i) {
            const Rect rect = dirty[i];
            hash = hash_mix(hash, static_cast<std::uint32_t>(rect.x));
            hash = hash_mix(hash, static_cast<std::uint32_t>(rect.y));
            hash = hash_mix(hash, static_cast<std::uint32_t>(rect.w));
            hash = hash_mix(hash, static_cast<std::uint32_t>(rect.h));
        }
        return hash;
    }

    [[nodiscard]] std::uint32_t hash_cmd_stats(const ui::scene::CmdStats& cmd,
                                               const ui::scene::ExecStats& exec) noexcept {
        std::uint32_t hash = 2166136261u;
        hash = hash_mix(hash, static_cast<std::uint32_t>(cmd.cmd_count));
        hash = hash_mix(hash, static_cast<std::uint32_t>(cmd.cmd_bytes));
        hash = hash_mix(hash, static_cast<std::uint32_t>(cmd.text_used));
        hash = hash_mix(hash, static_cast<std::uint32_t>(cmd.blob_used));
        hash = hash_mix(hash, static_cast<std::uint32_t>(exec.cmd_count));
        hash = hash_mix(hash, static_cast<std::uint32_t>(exec.cmd_rect));
        hash = hash_mix(hash, static_cast<std::uint32_t>(exec.cmd_text));
        hash = hash_mix(hash, static_cast<std::uint32_t>(exec.cmd_other));
        hash = hash_mix(hash, static_cast<std::uint32_t>(exec.failed_cmds));
        return hash;
    }

    [[nodiscard]] bool rect_contains(Rect outer, Rect inner) noexcept {
        return inner.x >= outer.x
            && inner.y >= outer.y
            && inner.x + inner.w <= outer.x + outer.w
            && inner.y + inner.h <= outer.y + outer.h;
    }

    [[nodiscard]] bool dirty_stays_inside(const DefaultCanvas& canvas, Rect bounds) noexcept {
        const auto& dirty = canvas.dirty_list();
        if (dirty.full()) return false;
        for (std::size_t i = 0; i < dirty.size(); ++i) {
            if (!rect_contains(bounds, dirty[i])) return false;
        }
        return true;
    }

    void prepare_style_sheet() noexcept {
        apply_baseline_theme_preset(make_style_from_tokens(Theme::instance().get_tokens()));
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

    [[nodiscard]] RenderEvidence render_evidence(::ui::scene::Scene& scene,
                                                 DefaultCanvas& canvas,
                                                 Rect dirty_hint) noexcept {
        canvas.begin_frame();
        canvas.mark_dirty(dirty_hint);
        scene.render();
        const auto cmd = scene.last_cmd_stats();
        const auto exec = scene.last_exec_stats();
        return RenderEvidence{
            .cmd_count = cmd.cmd_count,
            .cmd_bytes = cmd.cmd_bytes,
            .exec_cmds = exec.cmd_count,
            .failed_cmds = exec.failed_cmds,
            .dirty_count = canvas.dirty_list().size(),
            .dirty_hash = hash_dirty(canvas),
            .cmd_hash = hash_cmd_stats(cmd, exec),
            .pixel_hash = hash_bytes(canvas.data(),
                                     static_cast<std::size_t>(canvas.height()) * canvas.stride_bytes()),
        };
    }
}

int main() {
    print_component_card_run_begin();
    prepare_style_sheet();

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

    if (!expect(!access.checked(handles.enabled), "card starts disabled")) return 1;
    if (!expect(access.value(handles.level) == 30, "card starts with level truth")) return 1;
    if (!expect(access.value(handles.output) == 0, "disabled card gates output")) return 1;
    if (!expect(std::strcmp(scene.text(handles.summary), "enabled=0 level=30 output=0") == 0,
                "initial summary derives card state")) {
        return 1;
    }

    const auto initial = render_evidence(scene, canvas, Rect{0, 0, 240, 128});
    if (!expect(initial.failed_cmds == 0, "initial render has no failed commands")) return 1;
    if (!expect(initial.cmd_count > 0, "initial render records commands")) return 1;

    print_component_card_case("initial_artifact");
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

    if (!expect(access.checked(handles.enabled), "enabled child truth changes")) return 1;
    if (!expect(access.value(handles.level) == 72, "slider child truth changes")) return 1;
    if (!expect(access.value(handles.output) == 72, "progress child derives output")) return 1;
    if (!expect(std::strcmp(scene.text(handles.summary), "enabled=1 level=72 output=72") == 0,
                "summary derives combined card state")) {
        return 1;
    }

    print_component_card_case("state_delta");
    std::printf(" source=programmatic enabled_old=%d enabled_new=%d level_old=%d level_new=%d output=%d\n",
                state.old_enabled ? 1 : 0,
                state.enabled ? 1 : 0,
                state.old_level,
                state.level,
                state.output);

    print_component_card_case("component_derivation");
    std::printf(" children=3 summary=\"%s\" output_mirror=%d\n",
                scene.text(handles.summary),
                access.value(handles.output));

    print_component_card_case("invalidation_intent");
    std::printf(" kind=paint_only component_x=%d component_y=%d component_w=%d component_h=%d\n",
                kCardBounds.x,
                kCardBounds.y,
                kCardBounds.w,
                kCardBounds.h);

    const auto updated = render_evidence(scene, canvas, kCardBounds);
    if (!expect(updated.failed_cmds == 0, "updated render has no failed commands")) return 1;
    if (!expect(updated.cmd_count > 0, "updated render records commands")) return 1;
    if (!expect(updated.pixel_hash != initial.pixel_hash, "card state changes render artifact")) return 1;
    if (!expect(updated.dirty_count == 1, "updated render keeps a single card dirty rect")) return 1;
    if (!expect(dirty_stays_inside(canvas, kCardBounds), "dirty evidence remains inside card bounds")) return 1;

    print_component_card_case("render_artifact");
    std::printf(" dirty_count=%zu dirty_hash=%u cmd_count=%zu cmd_bytes=%zu exec_cmds=%zu failed=%zu cmd_hash=%u pixel_hash=%u\n",
                updated.dirty_count,
                updated.dirty_hash,
                updated.cmd_count,
                updated.cmd_bytes,
                updated.exec_cmds,
                updated.failed_cmds,
                updated.cmd_hash,
                updated.pixel_hash);

    print_component_card_run_end(true);
    std::puts("[component_card_state_demo] ok");
    return 0;
}
