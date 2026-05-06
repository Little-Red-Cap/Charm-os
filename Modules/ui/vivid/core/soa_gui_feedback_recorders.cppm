module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <utility>

export module charm.core.soa_gui.feedback_recorders;

import charm.core.geometry;
import charm.core.handle;
import charm.core.soa_kernel;
import charm.core.style;
import charm.core.style_sheet;
import charm.core.soa_gui.style_support;
import charm.gfx.draw_cmd;
import charm.gfx.text_box;
import charm.widgets.perf_overlay;
import out.core;
import out.format;
import util.expected;

export namespace ui::soa_gui_detail {
    constexpr int kWheelLutSize = 72;
    struct WheelQ15Point {
        std::int16_t x{};
        std::int16_t y{};
    };
    constexpr std::array<WheelQ15Point, kWheelLutSize> kWheelLut{{
        {0, -32767},{2856, -32642},{5690, -32269},{8481, -31650},{11207, -30791},{13848, -29697},
        {16383, -28377},{18794, -26841},{21062, -25101},{23170, -23170},{25101, -21062},{26841, -18794},
        {28377, -16384},{29697, -13848},{30791, -11207},{31650, -8481},{32269, -5690},{32642, -2856},
        {32767, 0},{32642, 2856},{32269, 5690},{31650, 8481},{30791, 11207},{29697, 13848},
        {28377, 16383},{26841, 18794},{25101, 21062},{23170, 23170},{21062, 25101},{18794, 26841},
        {16384, 28377},{13848, 29697},{11207, 30791},{8481, 31650},{5690, 32269},{2856, 32642},
        {0, 32767},{-2856, 32642},{-5690, 32269},{-8481, 31650},{-11207, 30791},{-13848, 29697},
        {-16384, 28377},{-18794, 26841},{-21062, 25101},{-23170, 23170},{-25101, 21062},{-26841, 18794},
        {-28377, 16384},{-29697, 13848},{-30791, 11207},{-31650, 8481},{-32269, 5690},{-32642, 2856},
        {-32767, 0},{-32642, -2856},{-32269, -5690},{-31650, -8481},{-30791, -11207},{-29697, -13848},
        {-28377, -16383},{-26841, -18794},{-25101, -21062},{-23170, -23170},{-21062, -25101},{-18794, -26841},
        {-16383, -28377},{-13848, -29697},{-11207, -30791},{-8481, -31650},{-5690, -32269},{-2856, -32642},
    }};

    constexpr int scale_q15(std::int16_t q, int radius) noexcept {
        const int v = static_cast<int>(q) * radius;
        return (v >= 0) ? ((v + (1 << 14)) >> 15) : ((v - (1 << 14)) >> 15);
    }

    struct soa_trunc_sink {
        char* buf{nullptr};
        std::size_t cap{0};
        std::size_t pos{0};

        out::result<std::size_t> write(out::bytes b) noexcept {
            if (!buf || cap == 0) return util::unexpected(out::errc::buffer_overflow);
            const std::size_t avail = (pos < cap) ? (cap - pos) : 0;
            const std::size_t n = (b.size() < avail) ? b.size() : avail;
            if (n > 0) {
                std::memcpy(buf + pos, b.data(), n);
                pos += n;
            }
            if (n < b.size()) return util::unexpected(out::errc::buffer_overflow);
            return out::ok(b.size());
        }
    };

    template <out::fixed_string Fmt, class... Args>
    inline std::string_view format_to(char* buf, std::size_t size, Args&&... args) noexcept {
        if (!buf || size == 0) return {};
        soa_trunc_sink sink{buf, size - 1u, 0u};
        (void)out::vprint<Fmt>(sink, std::forward<Args>(args)...);
        buf[sink.pos] = '\0';
        return {buf, sink.pos};
    }

    void record_progress(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r, const ResolvedColors& colors,
                         const ResolvedMetrics& metrics, const StyleState& state,
                         int value, int min_value, int max_value) {
        (void)state;
        const int pad = metrics.padding;
        const int inner_w = r.w - pad * 2;
        const int inner_h = r.h - pad * 2;
        if (inner_w <= 0 || inner_h <= 0) return;
        const int range = (max_value > min_value) ? (max_value - min_value) : 1;
        const int fill = (inner_w * (value - min_value)) / range;
        out.stroke_rect(Rect{r.x + pad, r.y + pad, inner_w, inner_h}, colors.border);
        out.fill_rect(Rect{r.x + pad, r.y + pad, fill, inner_h}, colors.accent);
    }

    void record_progress_bar_simple(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
                                    const ResolvedColors& colors, const ResolvedMetrics& metrics,
                                    int value, int min_value, int max_value) {
        const int pad = metrics.padding;
        const int inner_w = r.w - pad * 2;
        const int inner_h = r.h - pad * 2;
        if (inner_w <= 0 || inner_h <= 0) return;
        const int range = (max_value > min_value) ? (max_value - min_value) : 1;
        int clamped = value;
        if (clamped < min_value) clamped = min_value;
        if (clamped > max_value) clamped = max_value;
        const int fill = (inner_w * (clamped - min_value)) / range;
        const Rect track{r.x + pad, r.y + pad, inner_w, inner_h};
        out.fill_rect(track, colors.border);
        if (fill > 0) {
            out.fill_rect(Rect{track.x, track.y, fill, track.h}, colors.accent);
        }
    }

    void record_progress_bar_round(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
                                   const ResolvedColors& colors, const ResolvedMetrics& metrics,
                                   int value, int min_value, int max_value) {
        const int pad = metrics.padding;
        const int inner_w = r.w - pad * 2;
        const int inner_h = r.h - pad * 2;
        if (inner_w <= 0 || inner_h <= 0) return;
        const int range = (max_value > min_value) ? (max_value - min_value) : 1;
        int clamped = value;
        if (clamped < min_value) clamped = min_value;
        if (clamped > max_value) clamped = max_value;
        const int fill = (inner_w * (clamped - min_value)) / range;
        const Rect track{r.x + pad, r.y + pad, inner_w, inner_h};
        int rad = metrics.corner_radius;
        const int max_rad = inner_h / 2;
        if (rad > max_rad) rad = max_rad;
        if (rad < 0) rad = 0;
        out.fill_round_rect(track, rad, colors.border);
        if (fill > 0) {
            Rect fill_rect{track.x, track.y, fill, track.h};
            int fill_rad = rad;
            const int max_fill_rad = fill_rect.w / 2;
            if (fill_rad > max_fill_rad) fill_rad = max_fill_rad;
            out.fill_round_rect(fill_rect, fill_rad, colors.accent);
        }
    }

    void record_progress_flowing(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
                                 const ResolvedColors& colors, const ResolvedMetrics& metrics,
                                 int value, int min_value, int max_value) {
        const int pad = metrics.padding;
        const int inner_w = r.w - pad * 2;
        const int inner_h = r.h - pad * 2;
        if (inner_w <= 0 || inner_h <= 0) return;
        const Rect track{r.x + pad, r.y + pad, inner_w, inner_h};
        out.fill_rect(track, colors.border);
        if (max_value <= min_value) return;
        int clamped = value;
        if (clamped < min_value) clamped = min_value;
        if (clamped > max_value) clamped = max_value;
        const int range = max_value - min_value;
        if (range <= 0) return;
        const int segment = (inner_w > 0) ? (inner_w / 4) : 0;
        const int seg_w = (segment > 6) ? segment : 6;
        const int value_delta = clamped - min_value;
        const int pos = static_cast<int>((static_cast<std::int64_t>(inner_w + seg_w) * value_delta) / range) - seg_w;
        int seg_x0 = track.x + pos;
        int seg_x1 = seg_x0 + seg_w;
        if (seg_x1 <= track.x || seg_x0 >= track.x + track.w) return;
        if (seg_x0 < track.x) seg_x0 = track.x;
        if (seg_x1 > track.x + track.w) seg_x1 = track.x + track.w;
        if (seg_x1 > seg_x0) {
            out.fill_rect(Rect{seg_x0, track.y, seg_x1 - seg_x0, track.h}, colors.accent);
        }
    }

    void record_progress_wheel(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
                               const ResolvedColors& colors, const ResolvedMetrics& metrics,
                               int value, int min_value, int max_value) {
        const int pad = metrics.padding;
        const int size = (r.w < r.h) ? r.w : r.h;
        int radius = size / 2 - pad;
        if (radius <= 0) return;
        const int cx = r.x + r.w / 2;
        const int cy = r.y + r.h / 2;
        out.stroke_circle(cx, cy, radius, colors.border);

        if (max_value <= min_value) return;
        int clamped = value;
        if (clamped < min_value) clamped = min_value;
        if (clamped > max_value) clamped = max_value;
        const int range = max_value - min_value;
        if (range <= 0) return;
        if (clamped <= min_value) return;
        if (clamped >= max_value) {
            out.stroke_circle(cx, cy, radius, colors.accent);
            return;
        }
        const int value_delta = clamped - min_value;
        int idx = static_cast<int>((static_cast<std::int64_t>(value_delta) * kWheelLutSize) / range);
        if (idx <= 0) return;
        if (idx >= kWheelLutSize) idx = kWheelLutSize - 1;
        const int point_count = idx + 1;
        std::array<Point, kWheelLutSize> points{};
        for (int i = 0; i < point_count; ++i) {
            const auto q = kWheelLut[static_cast<std::size_t>(i)];
            const int px = cx + scale_q15(q.x, radius);
            const int py = cy + scale_q15(q.y, radius);
            points[static_cast<std::size_t>(i)] = Point{px, py};
        }
        out.draw_path(points.data(), point_count, false, colors.accent);
    }

    void record_spinner(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
                        const ResolvedColors& colors, std::uint8_t phase) {
        const int size = (r.w < r.h) ? r.w : r.h;
        const int radius = size / 2;
        if (radius <= 0) return;
        const int cx = r.x + r.w / 2;
        const int cy = r.y + r.h / 2;
        static constexpr std::array<Point, 8> kDirs{
            Point{0, -1}, Point{1, -1}, Point{1, 0}, Point{1, 1},
            Point{0, 1}, Point{-1, 1}, Point{-1, 0}, Point{-1, -1}
        };
        const int len = radius - 1;
        const std::uint8_t base = static_cast<std::uint8_t>(phase % kDirs.size());
        for (std::size_t i = 0; i < kDirs.size(); ++i) {
            const std::size_t idx = (base + i) % kDirs.size();
            const rgba color = (i == 0) ? colors.accent : colors.border;
            const int x1 = cx + kDirs[idx].x * len;
            const int y1 = cy + kDirs[idx].y * len;
            out.draw_line(cx, cy, x1, y1, color);
        }
    }

    void record_perf_overlay(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
                             const ResolvedColors& colors, const ResolvedMetrics& metrics,
                             const StyleState& state) {
        (void)state;
        const int radius = metrics.corner_radius;
        out.fill_round_rect(r, radius, colors.bg);
        for (int i = 0; i < metrics.border_width; ++i) {
            out.stroke_round_rect(Rect{r.x + i, r.y + i, r.w - 2 * i, r.h - 2 * i}, radius, colors.border);
        }

        const Font& font = font_from_metrics(metrics);
        const int line_h = (font.line_height > 0) ? font.line_height : 12;
        const int pad = metrics.padding;
        Rect line_rect{r.x + pad, r.y + pad, r.w - pad * 2, line_h};

        char buf[96]{};
        if (!perf_overlay_stats_valid()) {
            (void)format_to<"perf: n/a">(buf, sizeof(buf));
            out.draw_text_box(line_rect, buf, colors.font, font,
                              TextAlignH::Left, TextAlignV::Center, TextWrap::None, TextEllipsis::None);
            return;
        }

        const auto stats = perf_overlay_stats();
        (void)format_to<"dispatch/batch/failed: {}/{}/{}">(buf, sizeof(buf),
                                                          static_cast<unsigned>(stats.dispatch_groups),
                                                          static_cast<unsigned>(stats.batch_flushes),
                                                          static_cast<unsigned>(stats.failed_cmds));
        out.draw_text_box(line_rect, buf, colors.font, font,
                          TextAlignH::Left, TextAlignV::Center, TextWrap::None, TextEllipsis::None);
        line_rect.y += line_h;

        (void)format_to<"grp r/t/i/l/p/o: {}/{}/{}/{}/{}/{}">(buf, sizeof(buf),
                                                             static_cast<unsigned>(stats.group_rect),
                                                             static_cast<unsigned>(stats.group_text),
                                                             static_cast<unsigned>(stats.group_image),
                                                             static_cast<unsigned>(stats.group_line),
                                                             static_cast<unsigned>(stats.group_path),
                                                             static_cast<unsigned>(stats.group_other));
        out.draw_text_box(line_rect, buf, colors.font, font,
                          TextAlignH::Left, TextAlignV::Center, TextWrap::None, TextEllipsis::None);
        line_rect.y += line_h;

        (void)format_to<"cmd r/t/i/l/p/o: {}/{}/{}/{}/{}/{}">(buf, sizeof(buf),
                                                             static_cast<unsigned>(stats.cmd_rect),
                                                             static_cast<unsigned>(stats.cmd_text),
                                                             static_cast<unsigned>(stats.cmd_image),
                                                             static_cast<unsigned>(stats.cmd_line),
                                                             static_cast<unsigned>(stats.cmd_path),
                                                             static_cast<unsigned>(stats.cmd_other));
        out.draw_text_box(line_rect, buf, colors.font, font,
                          TextAlignH::Left, TextAlignV::Center, TextWrap::None, TextEllipsis::None);
    }
}
