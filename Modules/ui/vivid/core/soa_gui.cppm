module;
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <utility>

#include "features.hpp"

export module charm.core.soa_gui;

export import charm.core.soa_kernel;
export import charm.core.soa_layout;
export import charm.core.soa_payload;
export import charm.core.geometry;
export import charm.core.style;
export import charm.core.style_sheet;
export import charm.core.event;
export import charm.gfx.canvas;
export import charm.gfx.draw_cmd;
export import charm.gfx.render_style;
export import charm.gfx.text_box;
export import charm.font.typography;
import charm.widgets.perf_overlay;
import out.core;
import out.format;
import util.expected;

namespace {
    StyleState make_state(const SoaKernel& kernel, WidgetHandle h) noexcept {
        const StateCompact state = kernel.state_compact(h);
        return make_style_state(state.enabled(), state.hovered(), state.pressed(), state.focused(), state.variant);
    }

    const Font& font_from_metrics(const ResolvedMetrics& metrics) noexcept {
        return metrics.font ? *metrics.font : get_font(FontId::Normal);
    }

    void apply_style_patch(ResolvedColors& colors,
                           ResolvedMetrics& metrics,
                           ResolvedDecoration& decoration,
                           const StyleState& state,
                           const StylePatch& patch) noexcept {
        if (patch.has_border_width) metrics.border_width = static_cast<std::int16_t>(patch.border_width);
        if (patch.has_corner_radius) metrics.corner_radius = static_cast<std::int16_t>(patch.corner_radius);
        if (patch.has_padding) metrics.padding = static_cast<std::int16_t>(patch.padding);
        if (patch.has_header_padding) metrics.header_padding = static_cast<std::int16_t>(patch.header_padding);
        if (patch.has_content_padding) metrics.content_padding = static_cast<std::int16_t>(patch.content_padding);
        if (patch.has_scrollbar_margin) metrics.scrollbar_margin = static_cast<std::int16_t>(patch.scrollbar_margin);
        if (patch.has_scrollbar_thumb_min) metrics.scrollbar_thumb_min =
            static_cast<std::int16_t>(patch.scrollbar_thumb_min);
        if (patch.has_font_role) {
            metrics.font_role = patch.font_role;
            metrics.font_explicit = false;
            metrics.font = &get_font_weighted(metrics.font_role, metrics.font_weight);
        }
        if (patch.has_font_weight) {
            metrics.font_weight = patch.font_weight;
            if (!metrics.font_explicit) {
                metrics.font = &get_font_weighted(metrics.font_role, metrics.font_weight);
            }
        }
        if (patch.has_font) {
            metrics.font_explicit = patch.font != nullptr;
            metrics.font = metrics.font_explicit
                ? patch.font
                : &get_font_weighted(metrics.font_role, metrics.font_weight);
        }

        if (patch.has_bg_color) colors.bg = patch.bg_color;
        if (patch.has_border_color) colors.border = patch.border_color;
        if (patch.has_font_color) colors.font = patch.font_color;
        if (patch.has_accent_color) colors.accent = patch.accent_color;
        if (patch.has_on_accent) colors.on_accent = patch.on_accent;
        if (patch.has_border_focus) colors.border_focus = patch.border_focus;
        if (patch.has_gradient_enabled) colors.gradient_enabled = patch.gradient_enabled ? 1 : 0;
        if (patch.has_gradient_start) colors.gradient_start = patch.gradient_start;
        if (patch.has_gradient_end) colors.gradient_end = patch.gradient_end;
        if (patch.has_gradient_direction) colors.gradient_direction = patch.gradient_direction;
        if (patch.has_shadow_enabled) decoration.shadow_enabled = patch.shadow_enabled ? 1 : 0;
        if (patch.has_shadow_color) decoration.shadow_color = patch.shadow_color;
        if (patch.has_shadow_offset_x) decoration.shadow_offset_x = static_cast<std::int16_t>(patch.shadow_offset_x);
        if (patch.has_shadow_offset_y) decoration.shadow_offset_y = static_cast<std::int16_t>(patch.shadow_offset_y);
        if (patch.has_shadow_spread) decoration.shadow_spread = static_cast<std::int16_t>(patch.shadow_spread);
        if (patch.has_shadow_radius) decoration.shadow_radius = static_cast<std::int16_t>(patch.shadow_radius);
        if (patch.has_inner_stroke_enabled) decoration.inner_stroke_enabled = patch.inner_stroke_enabled ? 1 : 0;
        if (patch.has_inner_stroke_color) decoration.inner_stroke_color = patch.inner_stroke_color;
        if (patch.has_inner_stroke_width) decoration.inner_stroke_width =
            static_cast<std::int16_t>(patch.inner_stroke_width);
        if (patch.has_outline_enabled) decoration.outline_enabled = patch.outline_enabled ? 1 : 0;
        if (patch.has_outline_color) decoration.outline_color = patch.outline_color;
        if (patch.has_outline_width) decoration.outline_width = static_cast<std::int16_t>(patch.outline_width);

        if (!state.enabled) {
            if (patch.has_bg_disabled) colors.bg = patch.bg_disabled;
            if (patch.has_border_disabled) colors.border = patch.border_disabled;
            if (patch.has_font_color_disabled) colors.font = patch.font_color_disabled;
            if (patch.has_accent_disabled) colors.accent = patch.accent_disabled;
            return;
        }

        if (state.pressed) {
            if (patch.has_bg_pressed) colors.bg = patch.bg_pressed;
            if (patch.has_border_pressed) colors.border = patch.border_pressed;
            if (patch.has_accent_pressed) colors.accent = patch.accent_pressed;
            return;
        }

        if (state.hovered) {
            if (patch.has_bg_hover) colors.bg = patch.bg_hover;
            if (patch.has_border_hover) colors.border = patch.border_hover;
            if (patch.has_accent_hover) colors.accent = patch.accent_hover;
        }
    }

    void apply_style_adjust(ResolvedMetrics& metrics,
                            const StylePatch& patch) noexcept {
        if (patch.has_corner_radius) metrics.corner_radius = static_cast<std::int16_t>(patch.corner_radius);
        if (patch.has_padding) metrics.padding = static_cast<std::int16_t>(patch.padding);
        if (patch.has_font_role) {
            metrics.font_role = patch.font_role;
            metrics.font_explicit = false;
            metrics.font = &get_font_weighted(metrics.font_role, metrics.font_weight);
        }
        if (patch.has_font_weight) {
            metrics.font_weight = patch.font_weight;
            if (!metrics.font_explicit) {
                metrics.font = &get_font_weighted(metrics.font_role, metrics.font_weight);
            }
        }
        if (patch.has_font) {
            metrics.font_explicit = patch.font != nullptr;
            metrics.font = metrics.font_explicit
                ? patch.font
                : &get_font_weighted(metrics.font_role, metrics.font_weight);
        }
    }

    void draw_decoration_shadow(ui::draw_cmd::DefaultDrawCmdBuffer& out,
                                const Rect& r,
                                int radius,
                                const ResolvedDecoration& deco) {
        if (deco.shadow_enabled == 0 || deco.shadow_color.a == 0) return;
        const int spread = deco.shadow_spread;
        Rect sr{
            r.x + deco.shadow_offset_x - spread,
            r.y + deco.shadow_offset_y - spread,
            r.w + spread * 2,
            r.h + spread * 2
        };
        if (sr.w <= 0 || sr.h <= 0) return;
        int rad = (deco.shadow_radius > 0) ? deco.shadow_radius : (radius + spread);
        if (rad < 0) rad = 0;
        out.fill_round_rect(sr, rad, deco.shadow_color);
    }

    void draw_decoration_inner(ui::draw_cmd::DefaultDrawCmdBuffer& out,
                               const Rect& r,
                               int radius,
                               const ResolvedDecoration& deco) {
        if (deco.inner_stroke_enabled == 0 || deco.inner_stroke_width <= 0) return;
        const int width = deco.inner_stroke_width;
        for (int i = 0; i < width; ++i) {
            Rect in{r.x + i, r.y + i, r.w - 2 * i, r.h - 2 * i};
            if (in.w <= 0 || in.h <= 0) break;
            const int rad = std::max(0, radius - i);
            out.stroke_round_rect(in, rad, deco.inner_stroke_color);
        }
    }

    void draw_decoration_outline(ui::draw_cmd::DefaultDrawCmdBuffer& out,
                                 const Rect& r,
                                 int radius,
                                 const ResolvedDecoration& deco) {
        if (deco.outline_enabled == 0 || deco.outline_width <= 0) return;
        const int width = deco.outline_width;
        for (int i = 0; i < width; ++i) {
            Rect out_r{r.x - i, r.y - i, r.w + 2 * i, r.h + 2 * i};
            const int rad = radius + i;
            out.stroke_round_rect(out_r, rad, deco.outline_color);
        }
    }

    void record_decorated_box(ui::draw_cmd::DefaultDrawCmdBuffer& out,
                              const Rect& r,
                              const ResolvedColors& colors,
                              const ResolvedMetrics& metrics,
                              const ResolvedDecoration& deco,
                              bool draw_fill,
                              bool draw_border) {
        const int rad = metrics.corner_radius;
        draw_decoration_shadow(out, r, rad, deco);
        if (draw_fill) {
          if (colors.gradient_enabled) {
              out.fill_linear_gradient_rect(r, colors.gradient_start, colors.gradient_end, rad, colors.gradient_direction == 0);
          } else {
              out.fill_round_rect(r, rad, colors.bg);
          }
      }
        if (draw_border) {
            out.stroke_round_rect(r, rad, colors.border);
        }
        draw_decoration_inner(out, r, rad, deco);
        draw_decoration_outline(out, r, rad, deco);
    }

    constexpr std::size_t kMaxSegments = 8;
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

    int wrap_index(int idx, int count) noexcept {
        if (count <= 0) return -1;
        idx %= count;
        if (idx < 0) idx += count;
        return idx;
    }

    std::size_t format_int(char* buf, std::size_t cap, int value) noexcept {
        if (!buf || cap == 0) return 0;
        char tmp[16]{};
        std::size_t pos = 0;
        unsigned v = static_cast<unsigned>(value);
        bool neg = false;
        if (value < 0) {
            neg = true;
            v = static_cast<unsigned>(-value);
        }
        do {
            if (pos >= sizeof(tmp)) break;
            tmp[pos++] = static_cast<char>('0' + (v % 10u));
            v /= 10u;
        } while (v != 0u);
        std::size_t out = 0;
        if (neg && out + 1 < cap) {
            buf[out++] = '-';
        }
        while (pos > 0 && out + 1 < cap) {
            buf[out++] = tmp[--pos];
        }
        buf[out] = '\0';
        return out;
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

    bool is_scrollable_kind(WidgetKind kind) noexcept {
        return kind == WidgetKind::ScrollContainer || kind == WidgetKind::List;
    }

    void unsupported_kind(WidgetKind kind) noexcept {
#ifndef NDEBUG
        if (kind == WidgetKind::None) {
            assert(false && "SoaGui unsupported WidgetKind");
        }
#else
        (void)kind;
#endif
    }
}

export
class SoaGui {
public:
    SoaGui(CanvasBase& canvas, SoaKernel& kernel, WidgetHandle root) noexcept;

    void set_root(WidgetHandle root) noexcept;
    WidgetHandle root() const noexcept;

    void render();
    ui::draw_cmd::DrawCmdStats record_commands(ui::draw_cmd::DefaultDrawCmdBuffer& out);
    template <ui::RenderBackend Backend>
    ui::draw_cmd::DrawCmdTileStats render_tiles(Backend& backend,
                                                const FrameBufferView& tile_buffer,
                                                const ui::draw_cmd::DrawCmdTileConfig& config);
    void dispatch_event(const Event& e);
    WidgetHandle hit_test(int x, int y) noexcept;
    ui::draw_cmd::DrawCmdStats last_cmd_stats() const noexcept { return last_cmd_stats_; }
    ui::draw_cmd::DrawCmdExecStats last_exec_stats() const noexcept { return last_exec_stats_; }

private:
    CanvasBase& canvas_;
    SoaKernel& kernel_;
    WidgetHandle root_{};
    SoaLayoutPass layout_;
    std::uint32_t style_version_{0};
    std::uint32_t stylesheet_version_{0};
    ui::draw_cmd::DefaultDrawCmdBuffer cmd_buffer_{};
    ui::draw_cmd::DrawCmdExecutor cmd_exec_{};
    ui::draw_cmd::DrawCmdStats last_cmd_stats_{};
    ui::draw_cmd::DrawCmdExecStats last_exec_stats_{};

    void refresh_styles();
    ResolvedStyleView resolve_style(WidgetKind kind, const StyleState& state) const noexcept;
    void record_tree(ui::draw_cmd::DefaultDrawCmdBuffer& out);
    void record_node(WidgetHandle h, const Rect& world_rect, ui::draw_cmd::DefaultDrawCmdBuffer& out);

    static void record_label(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r, const ResolvedColors& colors,
                             const ResolvedMetrics& metrics, const StyleState& state, const char* text,
                             TextAlignH align_h, TextAlignV align_v);
    static void record_button(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r, const ResolvedColors& colors,
                              const ResolvedMetrics& metrics, const ResolvedDecoration& decoration,
                              const StyleState& state, const char* text,
                              ui::draw_cmd::ImageId icon, std::uint8_t icon_size);
    static void record_image(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
                             ui::draw_cmd::ImageId image, int corner_radius);
    static void record_text_box(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r, const ResolvedColors& colors,
                                const ResolvedMetrics& metrics, const StyleState& state, const char* text,
                                TextAlignV align_v, TextWrap wrap);
    static void record_switch(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r, const ResolvedColors& colors,
                              const ResolvedMetrics& metrics, const StyleState& state, bool checked);
    static void record_checkbox(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r, const ResolvedColors& colors,
                                const ResolvedMetrics& metrics, const StyleState& state,
                                const char* text, bool checked);
    static void record_radio(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r, const ResolvedColors& colors,
                             const ResolvedMetrics& metrics, const StyleState& state,
                             const char* text, bool checked);
    static void record_segmented_control(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
                                         const ResolvedColors& colors, const ResolvedMetrics& metrics,
                                         const StyleState& state, std::uint8_t variant,
                                         const char* const* labels, std::uint8_t count, std::uint8_t selected);
    static void record_stepper(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
                               const ResolvedColors& colors, const ResolvedMetrics& metrics,
                               const StyleState& state,
                               const char* const* labels, std::uint8_t count, std::uint8_t current);
    static void record_text_list(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
                                 const ResolvedColors& colors, const ResolvedMetrics& metrics,
                                 const StyleState& state,
                                 const char* const* items, std::uint16_t count, int selected,
                                 int scroll_y, int row_h);
    static void record_list_view(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
                                 const ResolvedColors& colors, const ResolvedMetrics& metrics,
                                 const StyleState& state, const SoaKernel& kernel, WidgetHandle h);
    static void record_table_view(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
                                  const ResolvedColors& colors, const ResolvedMetrics& metrics,
                                  const StyleState& state, const SoaKernel& kernel, WidgetHandle h);
    static void record_tree_view(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
                                 const ResolvedColors& colors, const ResolvedMetrics& metrics,
                                 const StyleState& state, const SoaKernel& kernel, WidgetHandle h);
    static void record_list(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r, const ResolvedColors& colors,
                            const ResolvedMetrics& metrics, const ResolvedDecoration& decoration,
                            const StyleState& state,
                            int scroll_y, int max_scroll);
    static void record_list_item(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r, const ResolvedColors& colors,
                                 const ResolvedMetrics& metrics, const StyleState& state,
                                 const char* text, bool selected);
    static void record_scroll_container(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r, const ResolvedColors& colors,
                                        const ResolvedMetrics& metrics, const StyleState& state,
                                        int scroll_y, int max_scroll);
    static void record_slider(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r, const ResolvedColors& colors,
                              const ResolvedMetrics& metrics, const StyleState& state,
                              int value, int min_value, int max_value);
    static void record_progress(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r, const ResolvedColors& colors,
                                const ResolvedMetrics& metrics, const StyleState& state,
                                int value, int min_value, int max_value);
    static void record_progress_bar_simple(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
                                           const ResolvedColors& colors, const ResolvedMetrics& metrics,
                                           int value, int min_value, int max_value);
    static void record_progress_bar_round(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
                                          const ResolvedColors& colors, const ResolvedMetrics& metrics,
                                          int value, int min_value, int max_value);
    static void record_progress_flowing(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
                                        const ResolvedColors& colors, const ResolvedMetrics& metrics,
                                        int value, int min_value, int max_value);
    static void record_progress_wheel(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
                                      const ResolvedColors& colors, const ResolvedMetrics& metrics,
                                      int value, int min_value, int max_value);
    static void record_number_list(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
                                   const ResolvedColors& colors, const ResolvedMetrics& metrics,
                                   const StyleState& state, const SoaKernel& kernel, WidgetHandle h);
    static void record_roller(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
                              const ResolvedColors& colors, const ResolvedMetrics& metrics,
                              const StyleState& state, const SoaKernel& kernel, WidgetHandle h);
    static void record_spinner(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
                               const ResolvedColors& colors, std::uint8_t phase);
    static void record_perf_overlay(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
                                    const ResolvedColors& colors, const ResolvedMetrics& metrics,
                                    const StyleState& state);
    static void record_scrollbar(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
                                 const ResolvedColors& colors, const ResolvedMetrics& metrics,
                                 ScrollBarOrientation orient, int scroll_y, int max_scroll, int page_size);
};

SoaGui::SoaGui(CanvasBase& canvas, SoaKernel& kernel, WidgetHandle root) noexcept
    : canvas_(canvas), kernel_(kernel), root_(root), layout_(kernel) {
    kernel_.set_input_root(root_);
    refresh_styles();
}

void SoaGui::set_root(WidgetHandle root) noexcept {
    root_ = root;
    kernel_.set_input_root(root);
}

WidgetHandle SoaGui::root() const noexcept {
    return root_;
}

    void SoaGui::render() {
        refresh_styles();
        layout_.run_if_needed(root_);
        cmd_buffer_.clear();
        ui::draw_cmd::ImageRegistryLockGuard guard{};
        ui::draw_cmd::ImageRegistryPhaseGuard phase_record{ui::draw_cmd::ImageRegisterReason::FrameRecord};
        record_tree(cmd_buffer_);
        ui::draw_cmd::ImageRegistryPhaseGuard phase_compact{ui::draw_cmd::ImageRegisterReason::FrameCompact};
        (void)cmd_buffer_.compact();
        last_cmd_stats_ = cmd_buffer_.stats();
        text_profile_reset();
        canvas_.begin_frame();
        ui::draw_cmd::ImageRegistryPhaseGuard phase_execute{ui::draw_cmd::ImageRegisterReason::FrameExecute};
        last_exec_stats_ = cmd_exec_.execute(canvas_, cmd_buffer_);
        canvas_.end_frame();
    }

    ui::draw_cmd::DrawCmdStats SoaGui::record_commands(ui::draw_cmd::DefaultDrawCmdBuffer& out) {
        refresh_styles();
        layout_.run_if_needed(root_);
        out.clear();
        ui::draw_cmd::ImageRegistryLockGuard guard{};
        ui::draw_cmd::ImageRegistryPhaseGuard phase_record{ui::draw_cmd::ImageRegisterReason::FrameRecord};
        record_tree(out);
        ui::draw_cmd::ImageRegistryPhaseGuard phase_compact{ui::draw_cmd::ImageRegisterReason::FrameCompact};
        (void)out.compact();
        last_cmd_stats_ = out.stats();
        return last_cmd_stats_;
    }

template <ui::RenderBackend Backend>
    ui::draw_cmd::DrawCmdTileStats SoaGui::render_tiles(Backend& backend,
                                                        const FrameBufferView& tile_buffer,
                                                        const ui::draw_cmd::DrawCmdTileConfig& config) {
        refresh_styles();
        layout_.run_if_needed(root_);
        cmd_buffer_.clear();
        ui::draw_cmd::ImageRegistryLockGuard guard{};
        ui::draw_cmd::ImageRegistryPhaseGuard phase_record{ui::draw_cmd::ImageRegisterReason::FrameRecord};
        record_tree(cmd_buffer_);
        ui::draw_cmd::ImageRegistryPhaseGuard phase_compact{ui::draw_cmd::ImageRegisterReason::FrameCompact};
        (void)cmd_buffer_.compact();
        last_cmd_stats_ = cmd_buffer_.stats();
        text_profile_reset();
        ui::draw_cmd::ImageRegistryPhaseGuard phase_execute{ui::draw_cmd::ImageRegisterReason::FrameExecute};
        return cmd_exec_.execute_tiles(backend, tile_buffer, cmd_buffer_, config);
    }

void SoaGui::dispatch_event(const Event& e) {
    if (!root_) return;
    layout_.run_if_needed(root_);
    kernel_.input_dispatch(e);
}

WidgetHandle SoaGui::hit_test(int x, int y) noexcept {
    if (!root_) return {};
    layout_.run_if_needed(root_);
    return kernel_.input_hit_test(x, y);
}

void SoaGui::refresh_styles() {
    const auto token_version = Theme::instance().get_tokens().version;
    const auto sheet_version = StyleSheet::instance().stylesheet_version();
    if (token_version == style_version_ && sheet_version == stylesheet_version_) return;
    style_version_ = token_version;
    stylesheet_version_ = sheet_version;
    StyleSheet::instance().rebuild_if_needed();
}

ResolvedStyleView SoaGui::resolve_style(WidgetKind kind, const StyleState& state) const noexcept {
    return StyleSheet::instance().lookup(kind, state);
}

void SoaGui::record_tree(ui::draw_cmd::DefaultDrawCmdBuffer& out) {
    if (!root_) return;
    struct Frame {
        WidgetHandle h{};
        WidgetHandle child{};
        bool entered{false};
        Rect clip_rect{};
        bool clip_enabled{false};
        bool clip_pushed{false};
        int offset_x{0};
        int offset_y{0};
        int child_offset_x{0};
        int child_offset_y{0};
        Rect world_rect{};
    };
    std::array<Frame, 256> stack{};
    std::size_t sp = 0;
    const auto base_clip = canvas_.save_clip();
    stack[sp++] = Frame{root_, {}, false, base_clip.rect, base_clip.enabled, false, 0, 0, 0, 0, Rect{}};

    while (sp > 0) {
        auto& frame = stack[sp - 1];
        if (!frame.entered) {
            frame.entered = true;
            if (!kernel_.valid(frame.h) || !kernel_.visible(frame.h)) {
                --sp;
                continue;
            }
            const Rect local_rect = kernel_.rect(frame.h);
            frame.world_rect = Rect{
                local_rect.x + frame.offset_x,
                local_rect.y + frame.offset_y,
                local_rect.w,
                local_rect.h
            };
            Rect paint = kernel_.paint_bounds(frame.h);
            if (!rect_valid(paint)) {
                paint = local_rect;
            }
            paint = Rect{
                paint.x + frame.offset_x,
                paint.y + frame.offset_y,
                paint.w,
                paint.h
            };
            if (frame.clip_enabled) {
                Rect out_clip{};
                if (!rect_intersect(paint, frame.clip_rect, out_clip)) {
                    --sp;
                    continue;
                }
            }
            record_node(frame.h, frame.world_rect, out);
            frame.child = kernel_.first_child(frame.h);
            frame.child_offset_x = frame.offset_x + local_rect.x;
            frame.child_offset_y = frame.offset_y + local_rect.y;
            if (is_scrollable_kind(kernel_.kind(frame.h))) {
                frame.child_offset_y -= kernel_.scroll_y(frame.h);
            }
            if (kernel_.clip_children(frame.h)) {
                Rect clip_rect = frame.world_rect;
                Rect out_clip{};
                bool ok = rect_valid(clip_rect);
                if (ok && frame.clip_enabled) {
                    ok = rect_intersect(clip_rect, frame.clip_rect, out_clip);
                    clip_rect = out_clip;
                }
                if (!ok) {
                    frame.child = {};
                } else {
                    out.push_clip(clip_rect);
                    frame.clip_pushed = true;
                    frame.clip_rect = clip_rect;
                    frame.clip_enabled = true;
                }
            }
            continue;
        }

        if (!frame.child) {
            if (frame.clip_pushed) {
                out.pop_clip();
            }
            --sp;
            continue;
        }

        WidgetHandle child = frame.child;
        frame.child = kernel_.next_sibling(child);
        if (sp >= stack.size()) continue;
        stack[sp++] = Frame{
            child,
            {},
            false,
            frame.clip_rect,
            frame.clip_enabled,
            false,
            frame.child_offset_x,
            frame.child_offset_y,
            0,
            0,
            Rect{}
        };
    }
}

void SoaGui::record_node(WidgetHandle h, const Rect& world_rect, ui::draw_cmd::DefaultDrawCmdBuffer& out) {
    const WidgetKind kind = kernel_.kind(h);
    const StyleState state = make_state(kernel_, h);
    const ResolvedStyleView style = resolve_style(kind, state);
    const ResolvedColors* colors = style.colors;
    const ResolvedMetrics* metrics = style.metrics;
    const ResolvedDecoration* decoration = style.decoration;
    ResolvedColors patched_colors{};
    ResolvedMetrics patched_metrics{};
    ResolvedDecoration patched_decoration{};
    const auto class_id = kernel_.style_class(h);
    const StylePatch* class_patch = (class_id != kStyleClassInvalid)
        ? Theme::instance().style_class(class_id)
        : nullptr;
    const auto patch_kind = kernel_.style_patch_kind(h);
    const StylePatch* local_patch = kernel_.style_patch(h);
    const StylePatch* override_patch = (patch_kind == StylePatchKind::Override) ? local_patch : nullptr;
    const StylePatch* adjust_patch = (patch_kind == StylePatchKind::Adjust) ? local_patch : nullptr;
      if (class_patch || override_patch || adjust_patch) {
          patched_colors = *colors;
          patched_metrics = *metrics;
          patched_decoration = *decoration;
        if (class_patch) {
            apply_style_patch(patched_colors, patched_metrics, patched_decoration, state, *class_patch);
        }
        if (override_patch) {
            apply_style_patch(patched_colors, patched_metrics, patched_decoration, state, *override_patch);
        }
        if (adjust_patch) {
            apply_style_adjust(patched_metrics, *adjust_patch);
        }
        colors = &patched_colors;
        metrics = &patched_metrics;
        decoration = &patched_decoration;
    }
    switch (kind) {
    case WidgetKind::None:
        unsupported_kind(kind);
        break;
    case WidgetKind::Container:
        if (class_patch || override_patch) {
            const auto wants_surface = [](const StylePatch* patch) noexcept {
                return patch && (patch->has_bg_color || patch->has_border_color ||
                    patch->has_border_width || patch->has_corner_radius ||
                    patch->has_shadow_enabled || patch->has_inner_stroke_enabled || patch->has_outline_enabled ||
                    patch->has_gradient_enabled || patch->has_gradient_start ||
                    patch->has_gradient_end || patch->has_gradient_direction);
            };
            const bool draw_surface = wants_surface(class_patch) || wants_surface(override_patch);
            if (draw_surface) {
                record_decorated_box(out, world_rect, *colors, *metrics, *decoration, true, true);
            }
        }
        break;
    case WidgetKind::ScrollContainer:
        record_scroll_container(out, world_rect, *colors, *metrics, state,
                                kernel_.scroll_y(h), kernel_.max_scroll(h));
        break;
    case WidgetKind::Dial:
        unsupported_kind(kind);
        break;
    case WidgetKind::Arc:
        unsupported_kind(kind);
        break;
    case WidgetKind::Image:
        record_image(out, world_rect, kernel_.image(h), metrics->corner_radius);
        break;
      case WidgetKind::Label:
          record_label(out, world_rect, *colors, *metrics, state, kernel_.text(h),
                       kernel_.text_align_h(h), kernel_.text_align_v(h));
          break;
        case WidgetKind::Button:
        case WidgetKind::IconButton:
            record_button(out, world_rect, *colors, *metrics, *decoration, state, kernel_.text(h),
                          kernel_.button_icon(h), kernel_.button_icon_size(h));
            break;
    case WidgetKind::Checkbox:
        record_checkbox(out, world_rect, *colors, *metrics, state, kernel_.text(h), kernel_.checked(h));
        break;
    case WidgetKind::Led:
        unsupported_kind(kind);
        break;
    case WidgetKind::Slider:
        record_slider(out, world_rect, *colors, *metrics, state,
                      kernel_.value(h), kernel_.min_value(h), kernel_.max_value(h));
        break;
    case WidgetKind::Switch:
        record_switch(out, world_rect, *colors, *metrics, state, kernel_.checked(h));
        break;
    case WidgetKind::Progress:
        record_progress(out, world_rect, *colors, *metrics, state,
                        kernel_.value(h), kernel_.min_value(h), kernel_.max_value(h));
        break;
    case WidgetKind::List:
        record_list(out, world_rect, *colors, *metrics, *decoration, state,
                    kernel_.scroll_y(h), kernel_.max_scroll(h));
        break;
    case WidgetKind::ListItem:
        record_list_item(out, world_rect, *colors, *metrics, state, kernel_.text(h), kernel_.checked(h));
        break;
    case WidgetKind::ListView:
        record_list_view(out, world_rect, *colors, *metrics, state, kernel_, h);
        break;
    case WidgetKind::IconList:
        record_list_view(out, world_rect, *colors, *metrics, state, kernel_, h);
        break;
    case WidgetKind::TextTrackingList:
        unsupported_kind(kind);
        break;
    case WidgetKind::TextList:
        {
            const std::uint16_t count = kernel_.text_list_count(h);
            constexpr std::size_t kMaxTextListItems = soa_detail::kMaxTextListItems;
            std::array<const char*, kMaxTextListItems> items{};
            for (std::uint16_t i = 0; i < count && i < items.size(); ++i) {
                items[i] = kernel_.text_list_item(h, i);
            }
            record_text_list(out, world_rect, *colors, *metrics, state,
                             items.data(), count, kernel_.text_list_selected(h),
                             kernel_.scroll_y(h), kernel_.list_row_height(h));
        }
        break;
    case WidgetKind::ConsoleBox:
        {
            const std::uint16_t count = kernel_.text_list_count(h);
            constexpr std::size_t kMaxTextListItems = soa_detail::kMaxTextListItems;
            std::array<const char*, kMaxTextListItems> items{};
            for (std::uint16_t i = 0; i < count && i < items.size(); ++i) {
                items[i] = kernel_.text_list_item(h, i);
            }
            record_text_list(out, world_rect, *colors, *metrics, state,
                             items.data(), count, -1,
                             kernel_.scroll_y(h), kernel_.list_row_height(h));
        }
        break;
    case WidgetKind::ModalDialog:
        unsupported_kind(kind);
        break;
    case WidgetKind::ProgressBarSimple:
        record_progress_bar_simple(out, world_rect, *colors, *metrics,
                                   kernel_.value(h), kernel_.min_value(h), kernel_.max_value(h));
        break;
    case WidgetKind::ProgressBarRound:
        record_progress_bar_round(out, world_rect, *colors, *metrics,
                                  kernel_.value(h), kernel_.min_value(h), kernel_.max_value(h));
        break;
    case WidgetKind::DynamicNebula:
        unsupported_kind(kind);
        break;
    case WidgetKind::CrtScreen:
        unsupported_kind(kind);
        break;
    case WidgetKind::ScrollBar:
        {
            const int min_value = kernel_.min_value(h);
            const int max_value = kernel_.max_value(h);
            const ScrollBarOrientation orient = kernel_.scrollbar_orientation(h);
            int scroll_y = kernel_.value(h) - min_value;
            int max_scroll = max_value - min_value;
            int page_size = kernel_.scrollbar_page_size(h);
            WidgetHandle target = kernel_.scrollbar_target(h);
            if (target) {
                scroll_y = kernel_.scroll_y(target);
                max_scroll = kernel_.max_scroll(target);
                if (page_size <= 0) {
                    const Rect tr = kernel_.rect(target);
                    page_size = (orient == ScrollBarOrientation::Vertical) ? tr.h : tr.w;
                }
            }
            if (max_scroll < 0) max_scroll = 0;
            record_scrollbar(out, world_rect, *colors, *metrics, orient, scroll_y, max_scroll, page_size);
            if (state.focused) {
                out.focus_ring(world_rect, colors->border_focus, metrics->corner_radius, 0, -1);
            }
        }
        break;
    case WidgetKind::SegmentedControl:
    case WidgetKind::TabView:
        {
            const std::uint8_t count = kernel_.segmented_count(h);
            std::array<const char*, kMaxSegments> labels{};
            for (std::uint8_t i = 0; i < count && i < labels.size(); ++i) {
                labels[i] = kernel_.segmented_label(h, i);
            }
            record_segmented_control(out, world_rect, *colors, *metrics, state, state.variant,
                                     labels.data(), count, kernel_.segmented_selected(h));
        }
        break;
    case WidgetKind::TextArea:
        record_text_box(out, world_rect, *colors, *metrics, state, kernel_.text(h),
                        TextAlignV::Top, TextWrap::Word);
        break;
    case WidgetKind::TextInput:
        record_text_box(out, world_rect, *colors, *metrics, state, kernel_.text(h),
                        TextAlignV::Center, TextWrap::None);
        break;
    case WidgetKind::NumberInput:
        record_text_box(out, world_rect, *colors, *metrics, state, kernel_.text(h),
                        TextAlignV::Center, TextWrap::None);
        break;
    case WidgetKind::TextBox:
        record_text_box(out, world_rect, *colors, *metrics, state, kernel_.text(h),
                        TextAlignV::Top, TextWrap::Word);
        break;
    case WidgetKind::ToggleGroup:
        break;
        break;
    case WidgetKind::TableView:
        record_table_view(out, world_rect, *colors, *metrics, state, kernel_, h);
        break;
    case WidgetKind::TreeView:
        record_tree_view(out, world_rect, *colors, *metrics, state, kernel_, h);
        break;
    case WidgetKind::Dropdown:
        unsupported_kind(kind);
        break;
    case WidgetKind::Roller:
        record_roller(out, world_rect, *colors, *metrics, state, kernel_, h);
        break;
    case WidgetKind::Spinner:
        record_spinner(out, world_rect, *colors, kernel_.spinner_phase(h));
        break;
    case WidgetKind::Bar:
        unsupported_kind(kind);
        break;
    case WidgetKind::PopupLayer:
        unsupported_kind(kind);
        break;
    case WidgetKind::MessageBox:
        unsupported_kind(kind);
        break;
    case WidgetKind::Menu:
        record_list(out, world_rect, *colors, *metrics, *decoration, state, 0, 0);
        if (state.focused) {
            out.focus_ring(world_rect, colors->border_focus, metrics->corner_radius, 0, -1);
        }
        break;
    case WidgetKind::MenuItem:
        record_list_item(out, world_rect, *colors, *metrics, state,
                         kernel_.text(h), kernel_.checked(h));
        break;
    case WidgetKind::Radio:
        record_radio(out, world_rect, *colors, *metrics, state, kernel_.text(h), kernel_.checked(h));
        break;
    case WidgetKind::RadioGroup:
        unsupported_kind(kind);
        break;
    case WidgetKind::Chart:
        unsupported_kind(kind);
        break;
    case WidgetKind::Waveform:
        unsupported_kind(kind);
        break;
    case WidgetKind::Gauge:
        unsupported_kind(kind);
        break;
    case WidgetKind::PrimitivesCanvas:
        unsupported_kind(kind);
        break;
    case WidgetKind::PerfOverlay:
        record_perf_overlay(out, world_rect, *colors, *metrics, state);
        break;
    case WidgetKind::Stepper:
        {
            const std::uint8_t count = kernel_.stepper_count(h);
            std::array<const char*, soa_detail::kMaxStepperSteps> labels{};
            for (std::uint8_t i = 0; i < count && i < labels.size(); ++i) {
                labels[i] = kernel_.stepper_label(h, i);
            }
            record_stepper(out, world_rect, *colors, *metrics, state,
                           labels.data(), count, kernel_.stepper_current(h));
        }
        break;
    case WidgetKind::Timeline:
        unsupported_kind(kind);
        break;
    case WidgetKind::RichText:
        unsupported_kind(kind);
        break;
    case WidgetKind::CodeBlock:
        unsupported_kind(kind);
        break;
    case WidgetKind::ProgressWheel:
        record_progress_wheel(out, world_rect, *colors, *metrics,
                              kernel_.value(h), kernel_.min_value(h), kernel_.max_value(h));
        break;
    case WidgetKind::WaveformView:
        unsupported_kind(kind);
        break;
    case WidgetKind::BatteryGauge:
        unsupported_kind(kind);
        break;
    case WidgetKind::HistogramView:
        unsupported_kind(kind);
        break;
    case WidgetKind::RingIndication:
        unsupported_kind(kind);
        break;
    case WidgetKind::FoldablePanel:
        unsupported_kind(kind);
        break;
    case WidgetKind::ProgressFlowing:
        record_progress_flowing(out, world_rect, *colors, *metrics,
                                kernel_.value(h), kernel_.min_value(h), kernel_.max_value(h));
        break;
    case WidgetKind::CloudyGlass:
        unsupported_kind(kind);
        break;
    case WidgetKind::NumberList:
        record_number_list(out, world_rect, *colors, *metrics, state, kernel_, h);
        break;
    case WidgetKind::SpinZoomWidget:
        unsupported_kind(kind);
        break;
    case WidgetKind::SpinningWheel:
        unsupported_kind(kind);
        break;
    case WidgetKind::ImageBox:
        unsupported_kind(kind);
        break;
    case WidgetKind::MeterPointer:
        unsupported_kind(kind);
        break;
    case WidgetKind::ProgressBarDrill:
        unsupported_kind(kind);
        break;
    case WidgetKind::SpectrumView:
        unsupported_kind(kind);
        break;
    case WidgetKind::BusyWheel:
        unsupported_kind(kind);
        break;
    case WidgetKind::BatteryGasGauge:
        unsupported_kind(kind);
        break;
    case WidgetKind::Histogram:
        unsupported_kind(kind);
        break;
    }
}

    void SoaGui::record_label(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r, const ResolvedColors& colors,
                              const ResolvedMetrics& metrics, const StyleState& state, const char* text,
                              TextAlignH align_h, TextAlignV align_v) {
        (void)state;
        const Font& font = font_from_metrics(metrics);
        if (text && (std::strcmp(text, "Your") == 0 || std::strcmp(text, "Mix") == 0
                     || std::strcmp(text, "Today's Mix for you") == 0)) {
            std::printf("[font-draw] text=%s line=%d base=%d rect=%d,%d,%d,%d\n",
                        text,
                        font.line_height,
                        font.baseline,
                        r.x,
                        r.y,
                        r.w,
                        r.h);
        }
        out.draw_text_box(r, text ? text : "", colors.font, font,
                          align_h, align_v, TextWrap::None, TextEllipsis::End);
    }

void SoaGui::record_button(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r, const ResolvedColors& colors,
                           const ResolvedMetrics& metrics, const ResolvedDecoration& decoration,
                           const StyleState& state, const char* text,
                           ui::draw_cmd::ImageId icon, std::uint8_t icon_size) {
    record_decorated_box(out, r, colors, metrics, decoration, true, true);
    Rect text_rect = r;
    TextAlignH align = TextAlignH::Center;
    if (ui::draw_cmd::image_id_valid(icon)) {
        int icon_px = static_cast<int>(icon_size);
        if (icon_px <= 0) {
            icon_px = r.h - metrics.padding * 2;
        }
        if (icon_px > r.h) icon_px = r.h;
        if (icon_px < 4) icon_px = r.h;
        const bool has_text = text && text[0] != '\0';
        const int icon_x = has_text ? (r.x + metrics.padding)
                                    : (r.x + (r.w - icon_px) / 2);
        const int icon_y = r.y + (r.h - icon_px) / 2;
        out.draw_icon(Rect{icon_x, icon_y, icon_px, icon_px}, icon);
        if (has_text) {
            text_rect.x = icon_x + icon_px + metrics.padding;
            text_rect.w = r.x + r.w - metrics.padding - text_rect.x;
            if (text_rect.w < 0) text_rect.w = 0;
            align = TextAlignH::Left;
        }
    }
    out.draw_text_box(text_rect, text ? text : "", colors.font, font_from_metrics(metrics),
                      align, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
    if (state.focused) {
        const int rad = metrics.corner_radius;
        out.focus_ring(r, colors.border_focus, metrics.corner_radius, 0, rad);
    }
}

void SoaGui::record_image(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
                          ui::draw_cmd::ImageId image, int corner_radius) {
    if (!ui::draw_cmd::image_id_valid(image)) return;
    if (corner_radius > 0) {
        out.draw_image_round_rect(r, image, corner_radius);
        return;
    }
    out.draw_image(r, image);
}

void SoaGui::record_text_box(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r, const ResolvedColors& colors,
                             const ResolvedMetrics& metrics, const StyleState& state, const char* text,
                             TextAlignV align_v, TextWrap wrap) {
    const int rad = metrics.corner_radius;
    out.fill_round_rect(r, rad, colors.bg);
    out.stroke_round_rect(r, rad, colors.border);
    Rect text_r{
        r.x + metrics.padding,
        r.y + metrics.padding,
        r.w - metrics.padding * 2,
        r.h - metrics.padding * 2
    };
    if (text_r.w < 0) text_r.w = 0;
    if (text_r.h < 0) text_r.h = 0;
    out.draw_text_box(text_r, text ? text : "", colors.font, font_from_metrics(metrics),
                      TextAlignH::Left, align_v, wrap, TextEllipsis::End);
    if (state.focused) {
        out.focus_ring(r, colors.border_focus, metrics.corner_radius, 0, rad);
    }
}

void SoaGui::record_switch(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r, const ResolvedColors& colors,
                           const ResolvedMetrics& metrics, const StyleState& state, bool checked) {
    (void)metrics;
    (void)state;
    const int rad = r.h / 2;
    const rgba track = checked ? colors.accent : colors.bg;
    out.fill_round_rect(r, rad, track);
    out.stroke_round_rect(r, rad, colors.border);
    const int knob = r.h - 4;
    const int knob_x = checked ? (r.x + r.w - knob - 2) : (r.x + 2);
    out.fill_round_rect(Rect{knob_x, r.y + 2, knob, knob}, knob / 2, colors.on_accent);
}

void SoaGui::record_checkbox(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r, const ResolvedColors& colors,
                             const ResolvedMetrics& metrics, const StyleState& state,
                             const char* text, bool checked) {
    int box = r.h;
    if (box > r.w) box = r.w;
    const int box_x = r.x;
    const int box_y = r.y + (r.h - box) / 2;
    out.stroke_rect(Rect{box_x, box_y, box, box}, colors.border);
    if (checked && box > 4) {
        out.fill_rect(Rect{box_x + 2, box_y + 2, box - 4, box - 4}, colors.accent);
    }
    Rect text_r{
        r.x + box + metrics.padding,
        r.y,
        r.w - box - metrics.padding,
        r.h
    };
    if (text_r.w < 0) text_r.w = 0;
    out.draw_text_box(text_r, text ? text : "", colors.font, font_from_metrics(metrics),
                      TextAlignH::Left, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
    if (state.focused) {
        out.focus_ring(r, colors.border_focus, metrics.corner_radius, 0, -1);
    }
}

void SoaGui::record_radio(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r, const ResolvedColors& colors,
                          const ResolvedMetrics& metrics, const StyleState& state,
                          const char* text, bool checked) {
    const int pad = metrics.padding;
    int radius = r.h / 2;
    if (radius < 2) radius = 2;
    const int cx = r.x + pad + radius;
    const int cy = r.y + r.h / 2;
    out.stroke_circle(cx, cy, radius, colors.border);
    if (checked && radius > 2) {
        out.fill_circle(cx, cy, radius - 2, colors.accent);
    }
    Rect text_r{
        cx + radius + pad,
        r.y,
        r.w - (radius * 2 + pad * 2),
        r.h
    };
    if (text_r.w < 0) text_r.w = 0;
    out.draw_text_box(text_r, text ? text : "", colors.font, font_from_metrics(metrics),
                      TextAlignH::Left, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
    if (state.focused) {
        out.focus_ring(r, colors.border_focus, metrics.corner_radius, 0, -1);
    }
}

void SoaGui::record_list(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r, const ResolvedColors& colors,
                         const ResolvedMetrics& metrics, const ResolvedDecoration& decoration,
                         const StyleState& state,
                         int scroll_y, int max_scroll) {
    (void)state;
    record_decorated_box(out, r, colors, metrics, decoration, true, true);
    record_scrollbar(out, r, colors, metrics, ScrollBarOrientation::Vertical, scroll_y, max_scroll, r.h);
}

void SoaGui::record_list_item(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r, const ResolvedColors& colors,
                              const ResolvedMetrics& metrics, const StyleState& state,
                              const char* text, bool selected) {
    rgba bg = colors.bg;
    rgba font = colors.font;
    if (selected) {
        bg = colors.accent;
        font = colors.on_accent;
    }
    out.fill_rect(r, bg);
    out.stroke_rect(r, colors.border);
    Rect text_r{
        r.x + metrics.padding,
        r.y,
        r.w - metrics.padding * 2,
        r.h
    };
    if (text_r.w < 0) text_r.w = 0;
    out.draw_text_box(text_r, text ? text : "", font, font_from_metrics(metrics),
                      TextAlignH::Left, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
    if (state.focused) {
        out.focus_ring(r, colors.border_focus, metrics.corner_radius, 0, -1);
    }
}

void SoaGui::record_scroll_container(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r, const ResolvedColors& colors,
                                     const ResolvedMetrics& metrics, const StyleState& state,
                                     int scroll_y, int max_scroll) {
    out.fill_rect(r, colors.bg);
    out.stroke_rect(r, colors.border);
    record_scrollbar(out, r, colors, metrics, ScrollBarOrientation::Vertical, scroll_y, max_scroll, r.h);
    if (state.focused) {
        out.focus_ring(r, colors.border_focus, metrics.corner_radius, 0, -1);
    }
}

void SoaGui::record_segmented_control(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
                                      const ResolvedColors& colors, const ResolvedMetrics& metrics,
                                      const StyleState& state, std::uint8_t variant,
                                      const char* const* labels, std::uint8_t count, std::uint8_t selected) {
    const int rad = metrics.corner_radius;
    const bool underline_mode = (variant != 0);
    if (underline_mode) {
        out.fill_rect(r, colors.bg);
        out.stroke_rect(r, colors.border);
    } else {
        out.fill_round_rect(r, rad, colors.bg);
        out.stroke_round_rect(r, rad, colors.border);
    }
    if (count == 0 || r.w <= 0) {
        if (state.focused) {
            out.focus_ring(r, colors.border_focus, metrics.corner_radius, 0, underline_mode ? -1 : rad);
        }
        return;
    }
    const int seg_w = (count > 0) ? (r.w / count) : 0;
    if (seg_w <= 0) {
        if (state.focused) {
            out.focus_ring(r, colors.border_focus, metrics.corner_radius, 0, underline_mode ? -1 : rad);
        }
        return;
    }
    for (std::uint8_t i = 0; i < count; ++i) {
        const int x = r.x + static_cast<int>(i) * seg_w;
        const int w = (i + 1u == count) ? (r.w - static_cast<int>(i) * seg_w) : seg_w;
        Rect seg{x, r.y, w, r.h};
        if (i == selected) {
            if (underline_mode) {
                Rect underline{
                    seg.x + metrics.padding,
                    seg.y + seg.h - 3,
                    seg.w - metrics.padding * 2,
                    2
                };
                if (underline.w < 0) underline.w = 0;
                out.fill_rect(underline, colors.accent);
            } else {
                out.fill_rect(seg, colors.accent);
            }
        }
        if (!underline_mode && i > 0) {
            out.fill_rect(Rect{x, r.y + 2, 1, r.h - 4}, colors.border);
        }
        Rect text_r{
            seg.x + metrics.padding,
            seg.y,
            seg.w - metrics.padding * 2,
            seg.h
        };
        if (text_r.w < 0) text_r.w = 0;
        const rgba text_color = (i == selected)
            ? (underline_mode ? colors.accent : colors.on_accent)
            : colors.font;
        const char* label = labels ? labels[i] : "";
        out.draw_text_box(text_r, label ? label : "", text_color, font_from_metrics(metrics),
                          TextAlignH::Center, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
    }
    if (state.focused) {
        out.focus_ring(r, colors.border_focus, metrics.corner_radius, 0, underline_mode ? -1 : rad);
    }
}

void SoaGui::record_stepper(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
                            const ResolvedColors& colors, const ResolvedMetrics& metrics,
                            const StyleState& state,
                            const char* const* labels, std::uint8_t count, std::uint8_t current) {
    out.fill_rect(r, colors.bg);
    out.stroke_rect(r, colors.border);
    if (count == 0) {
        if (state.focused) {
            out.focus_ring(r, colors.border_focus, metrics.corner_radius, 0, -1);
        }
        return;
    }
    const int pad = metrics.padding;
    const int left = r.x + pad;
    const int right = r.x + r.w - pad;
    const int center_y = r.y + r.h / 2;
    int span = right - left;
    if (span < 0) span = 0;
    int radius = r.h / 2 - pad;
    if (radius < 2) radius = 2;
    if (count > 1 && span > 0) {
        out.draw_line(left, center_y, right, center_y, colors.border);
    }
    int slot_w = (count > 0) ? (r.w / count) : r.w;
    if (slot_w < radius * 2) slot_w = radius * 2;
    const int label_h = font_from_metrics(metrics).line_height;
    for (std::uint8_t i = 0; i < count; ++i) {
        const int cx = (count == 1)
            ? (left + right) / 2
            : left + (span * static_cast<int>(i)) / (count - 1);
        const bool done = i < current;
        const bool active = i == current;
        const rgba fill = active ? colors.accent : (done ? colors.border : colors.bg);
        out.fill_circle(cx, center_y, radius, fill);
        out.stroke_circle(cx, center_y, radius, active ? colors.accent : colors.border);
        const char* label = (labels && labels[i]) ? labels[i] : "";
        if (label[0] != '\0') {
            const int label_x = cx - slot_w / 2;
            Rect label_rect{label_x, center_y + radius + 2, slot_w, label_h + 2};
            out.draw_text_box(label_rect, label, colors.font, font_from_metrics(metrics),
                              TextAlignH::Center, TextAlignV::Top, TextWrap::None, TextEllipsis::End);
        }
    }
    if (state.focused) {
        out.focus_ring(r, colors.border_focus, metrics.corner_radius, 0, -1);
    }
}

void SoaGui::record_text_list(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
                              const ResolvedColors& colors, const ResolvedMetrics& metrics,
                              const StyleState& state,
                              const char* const* items, std::uint16_t count, int selected,
                              int scroll_y, int row_h) {
    out.fill_rect(r, colors.bg);
    out.stroke_rect(r, colors.border);
    const int pad = metrics.padding;
    Rect clip_rect{r.x + pad, r.y + pad, r.w - pad * 2, r.h - pad * 2};
    if (clip_rect.w < 0) clip_rect.w = 0;
    if (clip_rect.h < 0) clip_rect.h = 0;
    out.push_clip(clip_rect);

    if (row_h <= 0) row_h = 1;
    const int visible = (row_h > 0) ? (clip_rect.h / row_h + 1) : 0;
    int start = 0;
    if (row_h > 0) {
        start = scroll_y / row_h;
        if (start < 0) start = 0;
    }
    int end = start + visible;
    if (end > static_cast<int>(count)) end = static_cast<int>(count);
    int y = clip_rect.y - (scroll_y % row_h);
    for (int i = start; i < end; ++i) {
        Rect row{clip_rect.x, y, clip_rect.w, row_h};
        if (i == selected) {
            out.fill_rect(row, colors.accent);
        }
        const rgba font = (i == selected) ? colors.on_accent : colors.font;
        out.draw_text_box(row, items && items[i] ? items[i] : "", font, font_from_metrics(metrics),
                          TextAlignH::Left, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
        y += row_h;
    }
    out.pop_clip();
    if (state.focused) {
        out.focus_ring(r, colors.border_focus, metrics.corner_radius, 0, -1);
    }
}

void SoaGui::record_number_list(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
                                const ResolvedColors& colors, const ResolvedMetrics& metrics,
                                const StyleState& state, const SoaKernel& kernel, WidgetHandle h) {
    out.fill_rect(r, colors.bg);
    out.stroke_rect(r, colors.border);
    const int pad = metrics.padding;
    Rect clip_rect{r.x + pad, r.y + pad, r.w - pad * 2, r.h - pad * 2};
    if (clip_rect.w < 0) clip_rect.w = 0;
    if (clip_rect.h < 0) clip_rect.h = 0;
    out.push_clip(clip_rect);

    const int row_h = kernel.number_list_row_height(h);
    const int count = kernel.number_list_count(h);
    const int selected = kernel.number_list_selected(h);
    const int scroll = kernel.scroll_y(h);
    if (row_h <= 0 || count <= 0) {
        out.pop_clip();
        if (state.focused) {
            out.focus_ring(r, colors.border_focus, metrics.corner_radius, 0, -1);
        }
        return;
    }
    const int center_y = r.y + r.h / 2;
    const int base_index = scroll / row_h;
    const int offset = scroll - base_index * row_h;
    const int visible = clip_rect.h / row_h + 3;
    int start = base_index - visible / 2;
    int y = center_y - row_h / 2 - offset - (base_index - start) * row_h;
    for (int i = 0; i < visible; ++i) {
        const int idx = start + i;
        if (idx >= 0 && idx < count) {
            Rect row{clip_rect.x, y, clip_rect.w, row_h};
            if (idx == selected) {
                out.fill_rect(row, colors.accent);
            }
            const rgba font = (idx == selected) ? colors.on_accent : colors.font;
            const int value = kernel.number_list_value(h, idx);
            char buf[16]{};
            (void)format_int(buf, sizeof(buf), value);
            out.draw_text_box(row, buf, font, font_from_metrics(metrics),
                              TextAlignH::Center, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
        }
        y += row_h;
    }
    out.pop_clip();
    if (state.focused) {
        out.focus_ring(r, colors.border_focus, metrics.corner_radius, 0, -1);
    }
}

void SoaGui::record_roller(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
                           const ResolvedColors& colors, const ResolvedMetrics& metrics,
                           const StyleState& state, const SoaKernel& kernel, WidgetHandle h) {
    out.fill_rect(r, colors.bg);
    out.stroke_rect(r, colors.border);
    const int pad = metrics.padding;
    Rect clip_rect{r.x + pad, r.y + pad, r.w - pad * 2, r.h - pad * 2};
    if (clip_rect.w < 0) clip_rect.w = 0;
    if (clip_rect.h < 0) clip_rect.h = 0;
    out.push_clip(clip_rect);

    const int row_h = kernel.roller_row_height(h);
    const int count = kernel.roller_count(h);
    const int selected = kernel.roller_selected(h);
    const int scroll = kernel.scroll_y(h);
    if (row_h <= 0 || count <= 0) {
        out.pop_clip();
        if (state.focused) {
            out.focus_ring(r, colors.border_focus, metrics.corner_radius, 0, -1);
        }
        return;
    }
    const int center_y = r.y + r.h / 2;
    const int base_index = scroll / row_h;
    const int offset = scroll - base_index * row_h;
    const int visible = clip_rect.h / row_h + 3;
    int start = base_index - visible / 2;
    int y = center_y - row_h / 2 - offset - (base_index - start) * row_h;
    for (int i = 0; i < visible; ++i) {
        const int raw_idx = start + i;
        const int idx = wrap_index(raw_idx, count);
        if (idx >= 0) {
            Rect row{clip_rect.x, y, clip_rect.w, row_h};
            if (idx == selected) {
                out.fill_rect(row, colors.accent);
            }
            const rgba font = (idx == selected) ? colors.on_accent : colors.font;
            const char* text = kernel.roller_item_text(h, static_cast<std::uint16_t>(idx));
            out.draw_text_box(row, text ? text : "", font, font_from_metrics(metrics),
                              TextAlignH::Center, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
        }
        y += row_h;
    }
    out.pop_clip();
    if (state.focused) {
        out.focus_ring(r, colors.border_focus, metrics.corner_radius, 0, -1);
    }
}

void SoaGui::record_list_view(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
                              const ResolvedColors& colors, const ResolvedMetrics& metrics,
                              const StyleState& state, const SoaKernel& kernel, WidgetHandle h) {
    out.fill_rect(r, colors.bg);
    out.stroke_rect(r, colors.border);
    const int pad = metrics.padding;
    Rect clip_rect{r.x + pad, r.y + pad, r.w - pad * 2, r.h - pad * 2};
    if (clip_rect.w < 0) clip_rect.w = 0;
    if (clip_rect.h < 0) clip_rect.h = 0;
    out.push_clip(clip_rect);

    const std::uint16_t count = kernel.list_view_count(h);
    int row_h = kernel.list_row_height(h);
    if (row_h <= 0) row_h = 1;
    const int scroll_y = kernel.scroll_y(h);
    const std::uint8_t overscan = kernel.list_view_overscan(h);
    const int base_start = (row_h > 0) ? (scroll_y / row_h) : 0;
    int start = base_start - static_cast<int>(overscan);
    if (start < 0) start = 0;
    int y = clip_rect.y - (scroll_y % row_h) - (base_start - start) * row_h;
    const int visible = (row_h > 0) ? (clip_rect.h / row_h + 1 + overscan * 2) : 0;
    int end = start + visible;
    if (end > static_cast<int>(count)) end = static_cast<int>(count);
    const int selected = kernel.list_view_selected(h);
    const int active = kernel.list_view_active(h);

    const int icon_size_raw = static_cast<int>(kernel.list_view_icon_size(h));
    for (int i = start; i < end; ++i) {
        Rect row{clip_rect.x, y, clip_rect.w, row_h};
        if (i == selected) {
            out.fill_rect(row, colors.accent);
        } else if (i == active) {
            out.stroke_rect(row, colors.accent);
        }
        const bool row_selected = (i == selected);
        const bool row_active = (i == active);
        const rgba font = row_selected ? colors.on_accent
                                       : (row_active ? colors.accent : colors.font);
        const auto icon = kernel.list_view_item_icon(h, static_cast<std::uint16_t>(i));
        int text_x = row.x + pad;
        int text_w = row.w - pad * 2;
        if (ui::draw_cmd::image_id_valid(icon)) {
            int icon_size = icon_size_raw;
            if (icon_size <= 0) {
                icon_size = row_h - pad * 2;
            }
            if (icon_size > row_h) icon_size = row_h;
            if (icon_size < 4) icon_size = row_h;
            const int icon_x = row.x + pad;
            const int icon_y = row.y + (row_h - icon_size) / 2;
            out.draw_icon(Rect{icon_x, icon_y, icon_size, icon_size}, icon);
            text_x = icon_x + icon_size + pad;
            text_w = row.x + row.w - pad - text_x;
        }
        if (text_w < 0) text_w = 0;
        const Rect text_rect{text_x, row.y, text_w, row_h};
        const char* title = kernel.list_view_item_text(h, static_cast<std::uint16_t>(i));
        const char* subtitle = kernel.list_view_item_subtitle(h, static_cast<std::uint16_t>(i));
        if (subtitle && subtitle[0] != '\0' && row_h >= 44) {
            const Font& title_font = font_from_metrics(metrics);
            const Font& subtitle_font = get_font(FontId::Small);
            const int title_h = title_font.line_height;
            const int subtitle_h = subtitle_font.line_height;
            const int line_gap = row_h >= 68 ? 3 : 2;
            const int total_h = title_h + line_gap + subtitle_h;
            int top = row.y + (row_h - total_h) / 2;
            if (top < row.y) top = row.y;
            const Rect title_rect{text_x, top, text_w, title_h};
            const Rect subtitle_rect{text_x, top + title_h + line_gap, text_w, subtitle_h};
            const auto subtitle_color = row_selected ? colors.on_accent
                                                     : (row_active ? colors.accent : colors.border);
            out.draw_text_box(title_rect, title ? title : "", font, title_font,
                              TextAlignH::Left, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
            out.draw_text_box(subtitle_rect, subtitle, subtitle_color, subtitle_font,
                              TextAlignH::Left, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
        } else {
            out.draw_text_box(text_rect, title ? title : "", font, font_from_metrics(metrics),
                              TextAlignH::Left, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
        }
        y += row_h;
    }

    out.pop_clip();
    if (state.focused) {
        out.focus_ring(r, colors.border_focus, metrics.corner_radius, 0, -1);
    }
}

void SoaGui::record_table_view(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
                               const ResolvedColors& colors, const ResolvedMetrics& metrics,
                               const StyleState& state, const SoaKernel& kernel, WidgetHandle h) {
    out.fill_rect(r, colors.bg);
    out.stroke_rect(r, colors.border);
    const int pad = metrics.padding;
    Rect clip_rect{r.x + pad, r.y + pad, r.w - pad * 2, r.h - pad * 2};
    if (clip_rect.w < 0) clip_rect.w = 0;
    if (clip_rect.h < 0) clip_rect.h = 0;

    const std::uint16_t rows = kernel.table_view_row_count(h);
    const std::uint8_t cols = kernel.table_view_col_count(h);
    int row_h = kernel.list_row_height(h);
    if (row_h <= 0) row_h = 1;
    const int scroll_y = kernel.scroll_y(h);
    const int scroll_x = kernel.table_view_scroll_x(h);
    const std::uint8_t overscan = kernel.table_view_overscan(h);
    int col_w = kernel.table_view_col_width(h);
    const bool has_col_fn = kernel.table_view_has_col_width_fn(h);
    const bool fixed_cols = (!has_col_fn && col_w > 0);
    const bool equal_cols = (!has_col_fn && col_w <= 0);
    if (equal_cols) {
        col_w = (cols > 0) ? (clip_rect.w / cols) : clip_rect.w;
    }
    if (col_w <= 0) col_w = 1;
    const bool has_header = kernel.table_view_has_header(h);
    int header_h = kernel.table_view_header_height(h);
    if (header_h < 0) header_h = 0;
    if (header_h > clip_rect.h) header_h = clip_rect.h;
    const TableViewHeaderStyle header_style = kernel.table_view_header_style(h);
    const bool header_divider = kernel.table_view_header_divider(h);
    const TableViewColDividerStyle col_divider_style = kernel.table_view_col_divider_style(h);
    Rect body_rect{clip_rect.x, clip_rect.y + header_h, clip_rect.w, clip_rect.h - header_h};
    if (body_rect.h < 0) body_rect.h = 0;

    int col_start = 0;
    int col_end = static_cast<int>(cols);
    int x_start = clip_rect.x;
    if (cols > 0 && clip_rect.w > 0) {
        if (fixed_cols) {
            if (scroll_x > 0 && col_w > 0) {
                col_start = scroll_x / col_w;
                const int offset = scroll_x - col_start * col_w;
                x_start = clip_rect.x - offset;
            }
            const int visible_cols = (clip_rect.w + col_w - 1) / col_w;
            col_end = col_start + visible_cols + 1;
            if (col_end > static_cast<int>(cols)) col_end = static_cast<int>(cols);
        } else if (equal_cols) {
            col_start = 0;
            col_end = static_cast<int>(cols);
            x_start = clip_rect.x;
        } else if (has_col_fn) {
            if (scroll_x > 0) {
                int x_accum = 0;
                while (col_start < static_cast<int>(cols)) {
                    int w = kernel.table_view_col_width_at(h, static_cast<std::uint8_t>(col_start));
                    if (w <= 0) w = 1;
                    if (x_accum + w > scroll_x) break;
                    x_accum += w;
                    ++col_start;
                }
                x_start = clip_rect.x - (scroll_x - x_accum);
            }
            int x_cursor = x_start;
            col_end = col_start;
            while (col_end < static_cast<int>(cols) && x_cursor < clip_rect.x + clip_rect.w) {
                int w = kernel.table_view_col_width_at(h, static_cast<std::uint8_t>(col_end));
                if (w <= 0) w = 1;
                x_cursor += w;
                ++col_end;
            }
            if (col_end < static_cast<int>(cols)) {
                ++col_end;
            }
        }
    }

    int header_pad = kernel.table_view_header_padding(h);
    if (header_pad <= 0) {
        header_pad = (metrics.header_padding > 0) ? metrics.header_padding : pad;
    }
    if (has_header && header_h > 0 && cols > 0) {
        const Rect header_rect{clip_rect.x, clip_rect.y, clip_rect.w, header_h};
        rgba header_bg = colors.bg;
        rgba header_font = colors.font;
        bool header_inset = false;
        if (header_style == TableViewHeaderStyle::Accent) {
            header_bg = colors.accent;
            header_font = colors.on_accent;
        } else if (header_style == TableViewHeaderStyle::Muted) {
            header_bg = colors.border;
            header_inset = true;
        }
        out.fill_rect(header_rect, header_bg);
        if (header_inset && header_rect.w > 2 && header_rect.h > 2) {
            const Rect inset{header_rect.x + 1, header_rect.y + 1,
                             header_rect.w - 2, header_rect.h - 2};
            out.fill_rect(inset, colors.bg);
        }
        if (header_divider) {
            out.fill_rect(Rect{header_rect.x, header_rect.y + header_rect.h - 1, header_rect.w, 1}, colors.border);
        }
        int x = x_start;
        for (int col = col_start; col < col_end; ++col) {
            int w = has_col_fn ? kernel.table_view_col_width_at(h, static_cast<std::uint8_t>(col)) : col_w;
            if (!has_col_fn && equal_cols && cols > 0) {
                if (static_cast<std::uint8_t>(col + 1) == cols) {
                    w = clip_rect.x + clip_rect.w - x;
                }
            }
            if (w <= 0) {
                x += has_col_fn ? 1 : col_w;
                continue;
            }
            Rect cell{x, header_rect.y, w, header_h};
            const char* text = kernel.table_view_header_text(h, static_cast<std::uint8_t>(col));
            Rect text_rect{cell.x + header_pad, cell.y, cell.w - header_pad * 2, cell.h};
            if (text_rect.w < 0) text_rect.w = 0;
            out.draw_text_box(text_rect, text ? text : "", header_font, font_from_metrics(metrics),
                              TextAlignH::Left, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
            x += w;
        }
    }

    out.push_clip(body_rect);

    const int base_start = (row_h > 0) ? (scroll_y / row_h) : 0;
    int start = base_start - static_cast<int>(overscan);
    if (start < 0) start = 0;
    int y = body_rect.y - (scroll_y % row_h) - (base_start - start) * row_h;
    int visible = 0;
    if (row_h > 0 && body_rect.h > 0) {
        visible = body_rect.h / row_h + 1 + overscan * 2;
    }
    int end = start + visible;
    if (end > static_cast<int>(rows)) end = static_cast<int>(rows);

    for (int row = start; row < end; ++row) {
        int x = x_start;
        for (int col = col_start; col < col_end; ++col) {
            int w = has_col_fn ? kernel.table_view_col_width_at(h, static_cast<std::uint8_t>(col)) : col_w;
            if (!has_col_fn && equal_cols && cols > 0) {
                if (static_cast<std::uint8_t>(col + 1) == cols) {
                    w = clip_rect.x + clip_rect.w - x;
                }
            }
            if (w <= 0) {
                x += has_col_fn ? 1 : col_w;
                continue;
            }
            Rect cell{x, y, w, row_h};
            const char* text = kernel.table_view_cell_text(h, static_cast<std::uint16_t>(row),
                                                           static_cast<std::uint8_t>(col));
            out.draw_text_box(cell, text ? text : "", colors.font, font_from_metrics(metrics),
                              TextAlignH::Left, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
            x += w;
        }
        out.fill_rect(Rect{body_rect.x, y + row_h - 1, body_rect.w, 1}, colors.border);
        y += row_h;
    }

    out.pop_clip();

    if (col_divider_style != TableViewColDividerStyle::None
        && (end > start || header_h > 0) && cols > 0) {
        const int line_h_body = (end > start) ? (end - start) * row_h : 0;
        const int line_y_body = body_rect.y - (scroll_y % row_h) - (base_start - start) * row_h;
        int line_y = line_y_body;
        int line_h = line_h_body;
        if (col_divider_style == TableViewColDividerStyle::HeaderOnly) {
            if (header_h > 0) {
                line_y = clip_rect.y;
                line_h = header_h;
            } else {
                line_h = 0;
            }
        } else if (col_divider_style == TableViewColDividerStyle::BodyOnly) {
            line_y = line_y_body;
            line_h = line_h_body;
        } else {
            line_y = header_h > 0 ? clip_rect.y : line_y_body;
            line_h = line_h_body + ((header_h > 0) ? header_h : 0);
        }
        if (line_h > 0) {
            int x = x_start;
            for (int col = col_start; col < col_end; ++col) {
                int w = has_col_fn ? kernel.table_view_col_width_at(h, static_cast<std::uint8_t>(col)) : col_w;
                if (!has_col_fn && equal_cols && cols > 0) {
                    if (static_cast<std::uint8_t>(col + 1) == cols) {
                        w = clip_rect.x + clip_rect.w - x;
                    }
                }
                if (col > col_start) {
                    out.fill_rect(Rect{x, line_y, 1, line_h}, colors.border);
                }
                if (w <= 0) {
                    w = has_col_fn ? 1 : col_w;
                }
                x += w;
            }
        }
    }
    if (state.focused) {
        out.focus_ring(r, colors.border_focus, metrics.corner_radius, 0, -1);
    }
}

void SoaGui::record_tree_view(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
                              const ResolvedColors& colors, const ResolvedMetrics& metrics,
                              const StyleState& state, const SoaKernel& kernel, WidgetHandle h) {
    out.fill_rect(r, colors.bg);
    out.stroke_rect(r, colors.border);
    const int pad = metrics.padding;
    Rect clip_rect{r.x + pad, r.y + pad, r.w - pad * 2, r.h - pad * 2};
    if (clip_rect.w < 0) clip_rect.w = 0;
    if (clip_rect.h < 0) clip_rect.h = 0;
    out.push_clip(clip_rect);

    const std::uint16_t count = kernel.tree_view_count(h);
    int row_h = kernel.list_row_height(h);
    if (row_h <= 0) row_h = 1;
    const int scroll_y = kernel.scroll_y(h);
    const std::uint8_t overscan = kernel.tree_view_overscan(h);
    const int indent_px = static_cast<int>(kernel.tree_view_indent_px(h));
    const int max_indent_px = kernel.tree_view_max_indent_px(h);
    const int min_text_avail_px = kernel.tree_view_min_text_avail_px(h);

    const int base_start = (row_h > 0) ? (scroll_y / row_h) : 0;
    int start = base_start - static_cast<int>(overscan);
    if (start < 0) start = 0;
    int y = clip_rect.y - (scroll_y % row_h) - (base_start - start) * row_h;
    const int visible = (row_h > 0) ? (clip_rect.h / row_h + 1 + overscan * 2) : 0;
    int end = start + visible;
    if (end > static_cast<int>(count)) end = static_cast<int>(count);

    for (int row = start; row < end; ++row) {
        const std::uint8_t indent = kernel.tree_view_item_indent(h, static_cast<std::uint16_t>(row));
        int indent_x = indent_px * static_cast<int>(indent);
        if (max_indent_px > 0 && indent_x > max_indent_px) {
            indent_x = max_indent_px;
        }
        Rect text_r{
            clip_rect.x + pad + indent_x,
            y,
            clip_rect.w - pad * 2 - indent_x,
            row_h
        };
        if (text_r.w < 0) text_r.w = 0;
        const char* text = kernel.tree_view_item_text(h, static_cast<std::uint16_t>(row));
        const bool too_narrow = (min_text_avail_px > 0 && text_r.w < min_text_avail_px);
        const char* draw_text = text ? text : "";
        TextEllipsis ellipsis = TextEllipsis::End;
        if (too_narrow) {
            draw_text = "...";
            ellipsis = TextEllipsis::None;
        }
        out.draw_text_box(text_r, draw_text, colors.font, font_from_metrics(metrics),
                          TextAlignH::Left, TextAlignV::Center, TextWrap::None, ellipsis);
        out.fill_rect(Rect{clip_rect.x, y + row_h - 1, clip_rect.w, 1}, colors.border);
        y += row_h;
    }

    out.pop_clip();
    if (state.focused) {
        out.focus_ring(r, colors.border_focus, metrics.corner_radius, 0, -1);
    }
}

void SoaGui::record_slider(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r, const ResolvedColors& colors,
                           const ResolvedMetrics& metrics, const StyleState& state,
                           int value, int min_value, int max_value) {
    (void)state;
    const int pad = metrics.padding;
    const int track_h = 4;
    const int inner_w = r.w - pad * 2;
    if (inner_w <= 0) return;
    const int range = (max_value > min_value) ? (max_value - min_value) : 1;
    const int fill = (inner_w * (value - min_value)) / range;
    const int track_y = r.y + (r.h - track_h) / 2;
    out.fill_rect(Rect{r.x + pad, track_y, inner_w, track_h}, colors.border);
    out.fill_rect(Rect{r.x + pad, track_y, fill, track_h}, colors.accent);
    const int knob = r.h - pad * 2;
    const int knob_x = r.x + pad + fill - knob / 2;
    out.fill_round_rect(Rect{knob_x, r.y + pad, knob, knob}, knob / 2, colors.accent);
}

void SoaGui::record_progress(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r, const ResolvedColors& colors,
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

void SoaGui::record_progress_bar_simple(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
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

void SoaGui::record_progress_bar_round(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
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

void SoaGui::record_progress_flowing(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
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

void SoaGui::record_progress_wheel(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
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

void SoaGui::record_spinner(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
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

void SoaGui::record_perf_overlay(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
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

void SoaGui::record_scrollbar(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
                              const ResolvedColors& colors, const ResolvedMetrics& metrics,
                              ScrollBarOrientation orient, int scroll_y, int max_scroll, int page_size) {
    if (max_scroll <= 0) return;
    if (r.w <= 0 || r.h <= 0) return;
    int margin = metrics.scrollbar_margin;
    if (margin < 0) margin = 0;
    int track_len = (orient == ScrollBarOrientation::Vertical)
        ? (r.h - margin * 2)
        : (r.w - margin * 2);
    if (track_len <= 0) return;
    int bar_w = (metrics.border_width > 0) ? (metrics.border_width * 2 + 2) : 4;
    if (bar_w < 2) bar_w = 2;
    int track_x = r.x + margin;
    int track_y = r.y + margin;
    if (orient == ScrollBarOrientation::Vertical) {
        track_x = r.x + r.w - margin - bar_w;
        if (track_x < r.x) track_x = r.x;
    } else {
        track_y = r.y + r.h - margin - bar_w;
        if (track_y < r.y) track_y = r.y;
    }
    int page = page_size;
    if (page <= 0) {
        page = (orient == ScrollBarOrientation::Vertical) ? r.h : r.w;
    }
    const int content_h = page + max_scroll;
    int thumb_min = metrics.scrollbar_thumb_min;
    if (thumb_min <= 0) thumb_min = 12;
    int thumb_h = (content_h > 0) ? (track_len * page) / content_h : track_len;
    if (thumb_h < thumb_min) thumb_h = thumb_min;
    if (thumb_h > track_len) thumb_h = track_len;
    const int max_thumb_y = track_len - thumb_h;
    int clamped = scroll_y;
    if (clamped < 0) clamped = 0;
    if (clamped > max_scroll) clamped = max_scroll;
    const int thumb_y = (orient == ScrollBarOrientation::Vertical)
        ? (track_y + ((max_scroll > 0) ? (max_thumb_y * clamped) / max_scroll : 0))
        : (track_x + ((max_scroll > 0) ? (max_thumb_y * clamped) / max_scroll : 0));
    rgba track = colors.border;
    if (track.a > 32) track.a = static_cast<std::uint8_t>(track.a / 2);
    if (orient == ScrollBarOrientation::Vertical) {
        out.fill_round_rect(Rect{track_x, track_y, bar_w, track_len}, bar_w / 2, track);
        out.fill_round_rect(Rect{track_x, thumb_y, bar_w, thumb_h}, bar_w / 2, colors.accent);
    } else {
        out.fill_round_rect(Rect{track_x, track_y, track_len, bar_w}, bar_w / 2, track);
        out.fill_round_rect(Rect{thumb_y, track_y, thumb_h, bar_w}, bar_w / 2, colors.accent);
    }
}
