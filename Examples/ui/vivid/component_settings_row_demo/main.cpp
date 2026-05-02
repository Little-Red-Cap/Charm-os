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
    constexpr Rect kComponentBounds{8, 8, 224, 76};

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

    unsigned component_settings_summary_case_count{0};

    void print_component_settings_run_begin() noexcept {
        std::printf("[csr] run=component_settings_row_demo phase=begin\n");
    }

    void print_component_settings_run_end(bool ok) noexcept {
        std::printf("[csr] run=component_settings_row_demo phase=end result=%s cases=%u\n",
                    ok ? "ok" : "fail",
                    component_settings_summary_case_count);
    }

    void print_component_settings_case(const char* name) noexcept {
        ++component_settings_summary_case_count;
        std::printf("[csr] case=%s", name);
    }

    [[nodiscard]] bool expect(bool condition, const char* message) noexcept {
        if (!condition) {
            std::printf("[ERR] %s\n", message);
            return false;
        }
        return true;
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

    void prepare_style_sheet() noexcept {
        apply_baseline_theme_preset(make_style_from_tokens(Theme::instance().get_tokens()));
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
    print_component_settings_run_begin();
    prepare_style_sheet();

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

    if (!expect(access.value(handles.slider) == 20, "initial slider truth")) return 1;
    if (!expect(access.value(handles.progress) == 20, "initial progress mirror truth")) return 1;
    if (!expect(std::strcmp(scene.text(handles.value_label), "20%") == 0, "initial value label truth")) return 1;

    const auto initial = render_evidence(scene, canvas, Rect{0, 0, 240, 96});
    if (!expect(initial.failed_cmds == 0, "initial render has no failed commands")) return 1;
    if (!expect(initial.cmd_count > 0, "initial render records commands")) return 1;

    print_component_settings_case("initial_artifact");
    std::printf(" value=%d dirty_count=%zu cmd_count=%zu cmd_hash=%u pixel_hash=%u\n",
                state.value,
                initial.dirty_count,
                initial.cmd_count,
                initial.cmd_hash,
                initial.pixel_hash);

    state.old_value = state.value;
    state.value = 64;
    apply_settings_row_state(access, handles, state);

    if (!expect(access.value(handles.slider) == 64, "slider truth changes")) return 1;
    if (!expect(access.value(handles.progress) == 64, "progress mirrors slider truth")) return 1;
    if (!expect(std::strcmp(scene.text(handles.value_label), "64%") == 0, "value label derives from truth")) return 1;

    print_component_settings_case("state_delta");
    std::printf(" source=programmatic key=settings_row.value old=%d new=%d mirror=%d label=%s\n",
                state.old_value,
                state.value,
                access.value(handles.progress),
                scene.text(handles.value_label));

    print_component_settings_case("invalidation_intent");
    std::printf(" kind=paint_only component_x=%d component_y=%d component_w=%d component_h=%d\n",
                kComponentBounds.x,
                kComponentBounds.y,
                kComponentBounds.w,
                kComponentBounds.h);

    const auto updated = render_evidence(scene, canvas, kComponentBounds);
    if (!expect(updated.failed_cmds == 0, "updated render has no failed commands")) return 1;
    if (!expect(updated.cmd_count > 0, "updated render records commands")) return 1;
    if (!expect(updated.pixel_hash != initial.pixel_hash, "state change affects render artifact")) return 1;
    if (!expect(updated.dirty_count == 1, "updated render keeps a single component dirty rect")) return 1;
    if (!expect(dirty_stays_inside(canvas, kComponentBounds), "dirty evidence remains inside component bounds")) return 1;

    print_component_settings_case("render_artifact");
    std::printf(" dirty_count=%zu dirty_hash=%u cmd_count=%zu cmd_bytes=%zu exec_cmds=%zu failed=%zu cmd_hash=%u pixel_hash=%u\n",
                updated.dirty_count,
                updated.dirty_hash,
                updated.cmd_count,
                updated.cmd_bytes,
                updated.exec_cmds,
                updated.failed_cmds,
                updated.cmd_hash,
                updated.pixel_hash);

    print_component_settings_run_end(true);
    std::puts("[component_settings_row_demo] ok");
    return 0;
}
