module;
#include <cstdint>
#include <cstring>
#include <expected>
#include <string_view>
#include <utility>
export module charm.widgets.perf_overlay;

import charm.core.object;
import charm.gfx.color;
import charm.gfx.canvas;
import charm.gfx.render_style;
import charm.gfx.text_box;
import charm.font.typography;
import charm.core.style;
import charm.core.style_sheet;
export import charm.ui.vivid.perf_overlay_runtime;
import out.core;
import out.format;
import out.sink;

using namespace ui::render;

namespace {
    struct perf_trunc_sink {
        char* buf{nullptr};
        std::size_t cap{0};
        std::size_t pos{0};

        out::result<std::size_t> write(out::bytes b) noexcept {
            if (!buf || cap == 0) return std::unexpected(out::errc::buffer_overflow);
            const std::size_t avail = (pos < cap) ? (cap - pos) : 0;
            const std::size_t n = (b.size() < avail) ? b.size() : avail;
            if (n > 0) {
                std::memcpy(buf + pos, b.data(), n);
                pos += n;
            }
            if (n < b.size()) return std::unexpected(out::errc::buffer_overflow);
            return out::ok(b.size());
        }
    };

    template <out::fixed_string Fmt, class... Args>
    inline std::string_view format_to(char* buf, std::size_t size, Args&&... args) noexcept {
        if (!buf || size == 0) return {};
        perf_trunc_sink sink{buf, size - 1u, 0u};
        (void)out::vprint<Fmt>(sink, std::forward<Args>(args)...);
        buf[sink.pos] = '\0';
        return {buf, sink.pos};
    }

}

export
class PerfOverlay : public WidgetBase<PerfOverlay> {
public:
    struct Sample {
        int fps{0};
        int draw_ms{0};
        int dirty_count{0};
        int dirty_area{0};
        int nodes{0};
        int depth_hits{0};
        int cycle_hits{0};
    };

    struct OverlayStats {
        std::uint32_t dispatch_groups{0};
        std::uint32_t batch_flushes{0};
        std::uint32_t failed_cmds{0};
        std::uint32_t batch_shrink{0};
        std::uint32_t batch_shrink_rect{0};
        std::uint32_t batch_shrink_round{0};
        std::uint32_t group_rect{0};
        std::uint32_t group_text{0};
        std::uint32_t group_image{0};
        std::uint32_t group_line{0};
        std::uint32_t group_path{0};
        std::uint32_t group_other{0};
        std::uint32_t cmd_rect{0};
        std::uint32_t cmd_text{0};
        std::uint32_t cmd_image{0};
        std::uint32_t cmd_line{0};
        std::uint32_t cmd_path{0};
        std::uint32_t cmd_other{0};
    };

    PerfOverlay() {
        set_focusable(false);
    }

    void set_sample(const Sample& s) noexcept {
        sample_ = s;
        has_sample_ = true;
    }

    void clear_sample() noexcept {
        sample_ = {};
        has_sample_ = false;
    }

    void draw(CanvasBase& cvs) {
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<PerfOverlay>();
        Style st_scratch;
        const Style& st = resolve_style(WidgetKind::PerfOverlay, state, base, st_scratch);
        const auto r = get_rect();
        std::uint32_t missing_glyphs = 0;
        std::uint32_t missing_fallbacks = 0;
        std::uint32_t utf8_replaces = 0;
#if defined(VIVID_SOA_TRACE_INPUT)
        missing_glyphs = missing_glyph_count();
        missing_fallbacks = missing_glyph_fallback_count();
        utf8_replaces = utf8_replacement_count();
#endif
        const auto text_profile = text_profile_sample();
        const auto& overlay_stats = ui::perf_overlay_runtime::get();

        rgba bg{};
        rgba border{};
        rgba font{};

        resolve_colors(st, state, bg, border, font);

        draw_round_rect(cvs, r.x, r.y, r.w, r.h, st.metrics.corner_radius, bg, true);
        for (int i = 0; i < st.metrics.border_width; ++i) {
            draw_round_rect(cvs, r.x + i, r.y + i, r.w - 2 * i, r.h - 2 * i, st.metrics.corner_radius, border, false);
        }

        const Font& ft = resolve_font(st);
        const int line_h = (ft.line_height > 0) ? ft.line_height : 12;
        const int start_x = r.x + st.metrics.padding;
        int y = r.y + st.metrics.padding;

        char buf[96]{};
        auto draw_debug_lines = [&](int& y_pos) {
            for (std::size_t i = 0; i < ui::perf_overlay_runtime::debug_line_count; ++i) {
                const std::string_view line = ui::perf_overlay_runtime::debug_line(i);
                if (line.empty()) continue;
                draw_text_baseline(cvs, start_x, y_pos + ft.baseline, line.data(), font, ft);
                y_pos += line_h;
            }
        };

        if (!has_sample_) {
            (void)format_to<"perf: n/a">(buf, sizeof(buf));
            draw_text_baseline(cvs, start_x, y + ft.baseline, buf, font, ft);
            y += line_h;
            (void)format_to<"glyph: {}/{}/{}">(buf, sizeof(buf),
                                              static_cast<unsigned>(missing_glyphs),
                                              static_cast<unsigned>(missing_fallbacks),
                                              static_cast<unsigned>(utf8_replaces));
            draw_text_baseline(cvs, start_x, y + ft.baseline, buf, font, ft);
            y += line_h;

            (void)format_to<"text: {}/{}/{}">(buf, sizeof(buf),
                                             static_cast<unsigned long long>(text_profile.draw_calls),
                                             static_cast<unsigned long long>(text_profile.glyphs),
                                             static_cast<unsigned long long>(text_profile.pixels));
            draw_text_baseline(cvs, start_x, y + ft.baseline, buf, font, ft);
            y += line_h;
            draw_debug_lines(y);
            return;
        }

        (void)format_to<"fps: {}  draw: {} ms">(buf, sizeof(buf), sample_.fps, sample_.draw_ms);
        draw_text_baseline(cvs, start_x, y + ft.baseline, buf, font, ft);
        y += line_h;

        (void)format_to<"dirty: {}  area: {}">(buf, sizeof(buf), sample_.dirty_count, sample_.dirty_area);
        draw_text_baseline(cvs, start_x, y + ft.baseline, buf, font, ft);
        y += line_h;

        (void)format_to<"nodes: {}  depth: {}  cycle: {}  shrink: {}/{}/{}">(buf, sizeof(buf),
                                                                            sample_.nodes,
                                                                            sample_.depth_hits,
                                                                            sample_.cycle_hits,
                                                                            overlay_stats.batch_shrink,
                                                                            overlay_stats.batch_shrink_rect,
                                                                            overlay_stats.batch_shrink_round);
        draw_text_baseline(cvs, start_x, y + ft.baseline, buf, font, ft);
        y += line_h;

        (void)format_to<"glyph: {}/{}/{}">(buf, sizeof(buf),
                                          static_cast<unsigned>(missing_glyphs),
                                          static_cast<unsigned>(missing_fallbacks),
                                          static_cast<unsigned>(utf8_replaces));
        draw_text_baseline(cvs, start_x, y + ft.baseline, buf, font, ft);
        y += line_h;

        (void)format_to<"text: {}/{}/{}">(buf, sizeof(buf),
                                         static_cast<unsigned long long>(text_profile.draw_calls),
                                         static_cast<unsigned long long>(text_profile.glyphs),
                                         static_cast<unsigned long long>(text_profile.pixels));
        draw_text_baseline(cvs, start_x, y + ft.baseline, buf, font, ft);
        y += line_h;

        if (ui::perf_overlay_runtime::valid()) {
            (void)format_to<"dispatch/batch/failed: {}/{}/{}">(buf, sizeof(buf),
                                                              overlay_stats.dispatch_groups,
                                                              overlay_stats.batch_flushes,
                                                              overlay_stats.failed_cmds);
            draw_text_baseline(cvs, start_x, y + ft.baseline, buf, font, ft);
            y += line_h;

            (void)format_to<"grp r/t/i/l/p/o: {}/{}/{}/{}/{}/{}">(buf, sizeof(buf),
                                                                 overlay_stats.group_rect,
                                                                 overlay_stats.group_text,
                                                                 overlay_stats.group_image,
                                                                 overlay_stats.group_line,
                                                                 overlay_stats.group_path,
                                                                 overlay_stats.group_other);
            draw_text_baseline(cvs, start_x, y + ft.baseline, buf, font, ft);
            y += line_h;

            (void)format_to<"cmd r/t/i/l/p/o: {}/{}/{}/{}/{}/{}">(buf, sizeof(buf),
                                                                 overlay_stats.cmd_rect,
                                                                 overlay_stats.cmd_text,
                                                                 overlay_stats.cmd_image,
                                                                 overlay_stats.cmd_line,
                                                                 overlay_stats.cmd_path,
                                                                 overlay_stats.cmd_other);
            draw_text_baseline(cvs, start_x, y + ft.baseline, buf, font, ft);
            y += line_h;
        }

        draw_debug_lines(y);
    }

private:
    Sample sample_{};
    bool has_sample_{false};
};

export inline void set_perf_overlay_stats(const PerfOverlay::OverlayStats& stats) noexcept {
    ui::perf_overlay_runtime::set({
        .dispatch_groups = stats.dispatch_groups,
        .batch_flushes = stats.batch_flushes,
        .failed_cmds = stats.failed_cmds,
        .batch_shrink = stats.batch_shrink,
        .batch_shrink_rect = stats.batch_shrink_rect,
        .batch_shrink_round = stats.batch_shrink_round,
        .group_rect = stats.group_rect,
        .group_text = stats.group_text,
        .group_image = stats.group_image,
        .group_line = stats.group_line,
        .group_path = stats.group_path,
        .group_other = stats.group_other,
        .cmd_rect = stats.cmd_rect,
        .cmd_text = stats.cmd_text,
        .cmd_image = stats.cmd_image,
        .cmd_line = stats.cmd_line,
        .cmd_path = stats.cmd_path,
        .cmd_other = stats.cmd_other,
    });
}

export inline void clear_perf_overlay_stats() noexcept {
    ui::perf_overlay_runtime::clear();
}

export inline bool perf_overlay_stats_valid() noexcept {
    return ui::perf_overlay_runtime::valid();
}

export inline PerfOverlay::OverlayStats perf_overlay_stats() noexcept {
    const auto& stats = ui::perf_overlay_runtime::get();
    PerfOverlay::OverlayStats out{};
    out.dispatch_groups = stats.dispatch_groups;
    out.batch_flushes = stats.batch_flushes;
    out.failed_cmds = stats.failed_cmds;
    out.batch_shrink = stats.batch_shrink;
    out.batch_shrink_rect = stats.batch_shrink_rect;
    out.batch_shrink_round = stats.batch_shrink_round;
    out.group_rect = stats.group_rect;
    out.group_text = stats.group_text;
    out.group_image = stats.group_image;
    out.group_line = stats.group_line;
    out.group_path = stats.group_path;
    out.group_other = stats.group_other;
    out.cmd_rect = stats.cmd_rect;
    out.cmd_text = stats.cmd_text;
    out.cmd_image = stats.cmd_image;
    out.cmd_line = stats.cmd_line;
    out.cmd_path = stats.cmd_path;
    out.cmd_other = stats.cmd_other;
    return out;
}

