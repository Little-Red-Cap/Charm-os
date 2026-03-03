module;
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>

#include "features.hpp"

export module charm.core.soa_gui;

export import charm.core.soa_kernel;
export import charm.core.soa_layout;
export import charm.core.geometry;
export import charm.core.style;
export import charm.core.style_sheet;
export import charm.core.event;
export import charm.gfx.canvas;
export import charm.gfx.draw_cmd;
export import charm.gfx.render;
export import charm.widgets.text;
export import charm.font.typography;

namespace {
    StyleState make_state(const SoaKernel& kernel, WidgetHandle h) noexcept {
        const StateCompact state = kernel.state_compact(h);
        return make_style_state(state.enabled(), state.hovered(), state.pressed(), state.focused(), state.variant);
    }

    const Font& font_from_metrics(const ResolvedMetrics& metrics) noexcept {
        return metrics.font ? *metrics.font : get_font(FontId::Normal);
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

    void refresh_styles();
    ResolvedStyleView resolve_style(WidgetKind kind, const StyleState& state) const noexcept;
    void record_tree(ui::draw_cmd::DefaultDrawCmdBuffer& out);
    void record_node(WidgetHandle h, const Rect& world_rect, ui::draw_cmd::DefaultDrawCmdBuffer& out);

    static void record_label(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r, const ResolvedColors& colors,
                             const ResolvedMetrics& metrics, const StyleState& state, const char* text);
    static void record_button(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r, const ResolvedColors& colors,
                              const ResolvedMetrics& metrics, const StyleState& state, const char* text);
    static void record_switch(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r, const ResolvedColors& colors,
                              const ResolvedMetrics& metrics, const StyleState& state, bool checked);
    static void record_checkbox(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r, const ResolvedColors& colors,
                                const ResolvedMetrics& metrics, const StyleState& state,
                                const char* text, bool checked);
    static void record_radio(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r, const ResolvedColors& colors,
                             const ResolvedMetrics& metrics, const StyleState& state,
                             const char* text, bool checked);
    static void record_list(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r, const ResolvedColors& colors,
                            const ResolvedMetrics& metrics, const StyleState& state,
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
    record_tree(cmd_buffer_);
    last_cmd_stats_ = cmd_buffer_.stats();
    canvas_.begin_frame();
    cmd_exec_.execute(canvas_, cmd_buffer_);
    canvas_.end_frame();
}

ui::draw_cmd::DrawCmdStats SoaGui::record_commands(ui::draw_cmd::DefaultDrawCmdBuffer& out) {
    refresh_styles();
    layout_.run_if_needed(root_);
    out.clear();
    record_tree(out);
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
    record_tree(cmd_buffer_);
    last_cmd_stats_ = cmd_buffer_.stats();
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
    const ResolvedColors& colors = *style.colors;
    const ResolvedMetrics& metrics = *style.metrics;
    switch (kind) {
    case WidgetKind::None:
        unsupported_kind(kind);
        break;
    case WidgetKind::Container:
        break;
    case WidgetKind::ScrollContainer:
        record_scroll_container(out, world_rect, colors, metrics, state,
                                kernel_.scroll_y(h), kernel_.max_scroll(h));
        break;
    case WidgetKind::Dial:
        unsupported_kind(kind);
        break;
    case WidgetKind::Arc:
        unsupported_kind(kind);
        break;
    case WidgetKind::Image:
        unsupported_kind(kind);
        break;
    case WidgetKind::Label:
        record_label(out, world_rect, colors, metrics, state, kernel_.text(h));
        break;
    case WidgetKind::Button:
        record_button(out, world_rect, colors, metrics, state, kernel_.text(h));
        break;
    case WidgetKind::Checkbox:
        record_checkbox(out, world_rect, colors, metrics, state, kernel_.text(h), kernel_.checked(h));
        break;
    case WidgetKind::Led:
        unsupported_kind(kind);
        break;
    case WidgetKind::Slider:
        record_slider(out, world_rect, colors, metrics, state,
                      kernel_.value(h), kernel_.min_value(h), kernel_.max_value(h));
        break;
    case WidgetKind::Switch:
        record_switch(out, world_rect, colors, metrics, state, kernel_.checked(h));
        break;
    case WidgetKind::Progress:
        record_progress(out, world_rect, colors, metrics, state,
                        kernel_.value(h), kernel_.min_value(h), kernel_.max_value(h));
        break;
    case WidgetKind::List:
        record_list(out, world_rect, colors, metrics, state,
                    kernel_.scroll_y(h), kernel_.max_scroll(h));
        break;
    case WidgetKind::ListItem:
        record_list_item(out, world_rect, colors, metrics, state, kernel_.text(h), kernel_.checked(h));
        break;
    case WidgetKind::ListView:
        unsupported_kind(kind);
        break;
    case WidgetKind::IconList:
        unsupported_kind(kind);
        break;
    case WidgetKind::TextTrackingList:
        unsupported_kind(kind);
        break;
    case WidgetKind::TextList:
        unsupported_kind(kind);
        break;
    case WidgetKind::ModalDialog:
        unsupported_kind(kind);
        break;
    case WidgetKind::ProgressBarSimple:
        unsupported_kind(kind);
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
            record_scrollbar(out, world_rect, colors, metrics, orient, scroll_y, max_scroll, page_size);
            if (state.focused) {
                out.focus_ring(world_rect, colors.border_focus, metrics.corner_radius, 0, -1);
            }
        }
        break;
    case WidgetKind::SegmentedControl:
        unsupported_kind(kind);
        break;
    case WidgetKind::TextArea:
        unsupported_kind(kind);
        break;
    case WidgetKind::TextInput:
        unsupported_kind(kind);
        break;
    case WidgetKind::NumberInput:
        unsupported_kind(kind);
        break;
    case WidgetKind::ToggleGroup:
        unsupported_kind(kind);
        break;
    case WidgetKind::TableView:
        unsupported_kind(kind);
        break;
    case WidgetKind::TreeView:
        unsupported_kind(kind);
        break;
    case WidgetKind::Dropdown:
        unsupported_kind(kind);
        break;
    case WidgetKind::TabView:
        unsupported_kind(kind);
        break;
    case WidgetKind::Roller:
        unsupported_kind(kind);
        break;
    case WidgetKind::Spinner:
        unsupported_kind(kind);
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
        unsupported_kind(kind);
        break;
    case WidgetKind::MenuItem:
        unsupported_kind(kind);
        break;
    case WidgetKind::Radio:
        record_radio(out, world_rect, colors, metrics, state, kernel_.text(h), kernel_.checked(h));
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
        unsupported_kind(kind);
        break;
    case WidgetKind::Stepper:
        unsupported_kind(kind);
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
        unsupported_kind(kind);
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
    case WidgetKind::TextBox:
        unsupported_kind(kind);
        break;
    case WidgetKind::FoldablePanel:
        unsupported_kind(kind);
        break;
    case WidgetKind::ProgressFlowing:
        unsupported_kind(kind);
        break;
    case WidgetKind::CloudyGlass:
        unsupported_kind(kind);
        break;
    case WidgetKind::NumberList:
        unsupported_kind(kind);
        break;
    case WidgetKind::ProgressBarRound:
        unsupported_kind(kind);
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
    case WidgetKind::ConsoleBox:
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
                          const ResolvedMetrics& metrics, const StyleState& state, const char* text) {
    (void)state;
    out.draw_text_box(r, text ? text : "", colors.font, font_from_metrics(metrics),
                      TextAlignH::Left, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
}

void SoaGui::record_button(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r, const ResolvedColors& colors,
                           const ResolvedMetrics& metrics, const StyleState& state, const char* text) {
    const int rad = metrics.corner_radius;
    out.fill_round_rect(r, rad, colors.bg);
    out.stroke_round_rect(r, rad, colors.border);
    out.draw_text_box(r, text ? text : "", colors.font, font_from_metrics(metrics),
                      TextAlignH::Center, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
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
                         const ResolvedMetrics& metrics, const StyleState& state,
                         int scroll_y, int max_scroll) {
    (void)state;
    out.fill_rect(r, colors.bg);
    out.stroke_rect(r, colors.border);
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
