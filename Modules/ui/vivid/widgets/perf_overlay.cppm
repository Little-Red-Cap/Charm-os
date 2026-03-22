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
import out.core;
import out.format;
import out.sink;

using namespace ui::render;

namespace {
    struct trunc_sink {
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
        trunc_sink sink{buf, size - 1u, 0u};
        (void)out::vprint<Fmt>(sink, std::forward<Args>(args)...);
        buf[sink.pos] = '\0';
        return {buf, sink.pos};
    }

}

namespace perf_overlay_detail {
    struct OverlayStatsState {
        std::uint32_t dispatch_groups{0};
        std::uint32_t batch_flushes{0};
        std::uint32_t failed_cmds{0};
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
        bool valid{false};
    };

    inline OverlayStatsState g_overlay_stats{};
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
            return;
        }

        (void)format_to<"fps: {}  draw: {} ms">(buf, sizeof(buf), sample_.fps, sample_.draw_ms);
        draw_text_baseline(cvs, start_x, y + ft.baseline, buf, font, ft);
        y += line_h;

        (void)format_to<"dirty: {}  area: {}">(buf, sizeof(buf), sample_.dirty_count, sample_.dirty_area);
        draw_text_baseline(cvs, start_x, y + ft.baseline, buf, font, ft);
        y += line_h;

        (void)format_to<"nodes: {}  depth: {}  cycle: {}">(buf, sizeof(buf),
                                                         sample_.nodes, sample_.depth_hits, sample_.cycle_hits);
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

        if (perf_overlay_detail::g_overlay_stats.valid) {
            (void)format_to<"dispatch/batch/failed: {}/{}/{}">(buf, sizeof(buf),
                                                              perf_overlay_detail::g_overlay_stats.dispatch_groups,
                                                              perf_overlay_detail::g_overlay_stats.batch_flushes,
                                                              perf_overlay_detail::g_overlay_stats.failed_cmds);
            draw_text_baseline(cvs, start_x, y + ft.baseline, buf, font, ft);
            y += line_h;

            (void)format_to<"grp r/t/i/l/p/o: {}/{}/{}/{}/{}/{}">(buf, sizeof(buf),
                                                                 perf_overlay_detail::g_overlay_stats.group_rect,
                                                                 perf_overlay_detail::g_overlay_stats.group_text,
                                                                 perf_overlay_detail::g_overlay_stats.group_image,
                                                                 perf_overlay_detail::g_overlay_stats.group_line,
                                                                 perf_overlay_detail::g_overlay_stats.group_path,
                                                                 perf_overlay_detail::g_overlay_stats.group_other);
            draw_text_baseline(cvs, start_x, y + ft.baseline, buf, font, ft);
            y += line_h;

            (void)format_to<"cmd r/t/i/l/p/o: {}/{}/{}/{}/{}/{}">(buf, sizeof(buf),
                                                                 perf_overlay_detail::g_overlay_stats.cmd_rect,
                                                                 perf_overlay_detail::g_overlay_stats.cmd_text,
                                                                 perf_overlay_detail::g_overlay_stats.cmd_image,
                                                                 perf_overlay_detail::g_overlay_stats.cmd_line,
                                                                 perf_overlay_detail::g_overlay_stats.cmd_path,
                                                                 perf_overlay_detail::g_overlay_stats.cmd_other);
            draw_text_baseline(cvs, start_x, y + ft.baseline, buf, font, ft);
        }
    }

private:
    Sample sample_{};
    bool has_sample_{false};
};

export inline void set_perf_overlay_stats(const PerfOverlay::OverlayStats& stats) noexcept {
    perf_overlay_detail::g_overlay_stats.dispatch_groups = stats.dispatch_groups;
    perf_overlay_detail::g_overlay_stats.batch_flushes = stats.batch_flushes;
    perf_overlay_detail::g_overlay_stats.failed_cmds = stats.failed_cmds;
    perf_overlay_detail::g_overlay_stats.group_rect = stats.group_rect;
    perf_overlay_detail::g_overlay_stats.group_text = stats.group_text;
    perf_overlay_detail::g_overlay_stats.group_image = stats.group_image;
    perf_overlay_detail::g_overlay_stats.group_line = stats.group_line;
    perf_overlay_detail::g_overlay_stats.group_path = stats.group_path;
    perf_overlay_detail::g_overlay_stats.group_other = stats.group_other;
    perf_overlay_detail::g_overlay_stats.cmd_rect = stats.cmd_rect;
    perf_overlay_detail::g_overlay_stats.cmd_text = stats.cmd_text;
    perf_overlay_detail::g_overlay_stats.cmd_image = stats.cmd_image;
    perf_overlay_detail::g_overlay_stats.cmd_line = stats.cmd_line;
    perf_overlay_detail::g_overlay_stats.cmd_path = stats.cmd_path;
    perf_overlay_detail::g_overlay_stats.cmd_other = stats.cmd_other;
    perf_overlay_detail::g_overlay_stats.valid = true;
}

export inline void clear_perf_overlay_stats() noexcept {
    perf_overlay_detail::g_overlay_stats = perf_overlay_detail::OverlayStatsState{};
}

export inline bool perf_overlay_stats_valid() noexcept {
    return perf_overlay_detail::g_overlay_stats.valid;
}

export inline PerfOverlay::OverlayStats perf_overlay_stats() noexcept {
    PerfOverlay::OverlayStats out{};
    out.dispatch_groups = perf_overlay_detail::g_overlay_stats.dispatch_groups;
    out.batch_flushes = perf_overlay_detail::g_overlay_stats.batch_flushes;
    out.failed_cmds = perf_overlay_detail::g_overlay_stats.failed_cmds;
    out.group_rect = perf_overlay_detail::g_overlay_stats.group_rect;
    out.group_text = perf_overlay_detail::g_overlay_stats.group_text;
    out.group_image = perf_overlay_detail::g_overlay_stats.group_image;
    out.group_line = perf_overlay_detail::g_overlay_stats.group_line;
    out.group_path = perf_overlay_detail::g_overlay_stats.group_path;
    out.group_other = perf_overlay_detail::g_overlay_stats.group_other;
    out.cmd_rect = perf_overlay_detail::g_overlay_stats.cmd_rect;
    out.cmd_text = perf_overlay_detail::g_overlay_stats.cmd_text;
    out.cmd_image = perf_overlay_detail::g_overlay_stats.cmd_image;
    out.cmd_line = perf_overlay_detail::g_overlay_stats.cmd_line;
    out.cmd_path = perf_overlay_detail::g_overlay_stats.cmd_path;
    out.cmd_other = perf_overlay_detail::g_overlay_stats.cmd_other;
    return out;
}




