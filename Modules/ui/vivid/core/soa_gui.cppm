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
export import charm.core.container;
export import charm.gfx.canvas;
export import charm.gfx.render;
export import charm.widgets.text;
export import charm.font.typography;
#if CHARM_VIVID_ENABLE_WIDGET_Button
export import charm.widgets.button;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Checkbox
export import charm.widgets.checkbox;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Label
export import charm.widgets.label;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_List
export import charm.widgets.list;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Progress
export import charm.widgets.progress;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Radio
export import charm.widgets.radio;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Slider
export import charm.widgets.slider;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Switch
export import charm.widgets.switcher;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ScrollContainer
export import charm.widgets.scroll_container;
#endif

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
        (void)kind;
        assert(false && "SoaGui unsupported WidgetKind");
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
    void dispatch_event(const Event& e);
    WidgetHandle hit_test(int x, int y) noexcept;

private:
    CanvasBase& canvas_;
    SoaKernel& kernel_;
    WidgetHandle root_{};
    SoaLayoutPass layout_;
    std::uint32_t style_version_{0};
    std::uint32_t stylesheet_version_{0};

    void refresh_styles();
    ResolvedStyleView resolve_style(WidgetKind kind, const StyleState& state) const noexcept;
    void draw_tree();
    void draw_node(WidgetHandle h, const Rect& world_rect);

    static void draw_label(CanvasBase& cvs, const Rect& r, const ResolvedColors& colors, const ResolvedMetrics& metrics,
                           const StyleState& state, const char* text);
    static void draw_button(CanvasBase& cvs, const Rect& r, const ResolvedColors& colors, const ResolvedMetrics& metrics,
                            const StyleState& state, const char* text);
    static void draw_switch(CanvasBase& cvs, const Rect& r, const ResolvedColors& colors, const ResolvedMetrics& metrics,
                            const StyleState& state, bool checked);
    static void draw_checkbox(CanvasBase& cvs, const Rect& r, const ResolvedColors& colors, const ResolvedMetrics& metrics,
                              const StyleState& state,
                              const char* text, bool checked);
    static void draw_radio(CanvasBase& cvs, const Rect& r, const ResolvedColors& colors, const ResolvedMetrics& metrics,
                           const StyleState& state,
                           const char* text, bool checked);
    static void draw_list(CanvasBase& cvs, const Rect& r, const ResolvedColors& colors, const ResolvedMetrics& metrics,
                          const StyleState& state);
    static void draw_list_item(CanvasBase& cvs, const Rect& r, const ResolvedColors& colors, const ResolvedMetrics& metrics,
                               const StyleState& state,
                               const char* text, bool selected);
    static void draw_scroll_container(CanvasBase& cvs, const Rect& r, const ResolvedColors& colors, const ResolvedMetrics& metrics,
                                      const StyleState& state);
    static void draw_slider(CanvasBase& cvs, const Rect& r, const ResolvedColors& colors, const ResolvedMetrics& metrics,
                            const StyleState& state,
                            int value, int min_value, int max_value);
    static void draw_progress(CanvasBase& cvs, const Rect& r, const ResolvedColors& colors, const ResolvedMetrics& metrics,
                              const StyleState& state,
                              int value, int min_value, int max_value);
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
    canvas_.begin_frame();
    draw_tree();
    canvas_.end_frame();
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


void SoaGui::draw_tree() {
    if (!root_) return;
    struct Frame {
        WidgetHandle h{};
        WidgetHandle child{};
        bool entered{false};
        CanvasBase::ClipState clip_state{};
        bool clip_applied{false};
        int offset_x{0};
        int offset_y{0};
        int child_offset_x{0};
        int child_offset_y{0};
        Rect world_rect{};
    };
    std::array<Frame, 256> stack{};
    std::size_t sp = 0;
    stack[sp++] = Frame{root_, {}, false, canvas_.save_clip(), false, 0, 0, 0, 0, Rect{}};

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
            if (frame.clip_state.enabled) {
                Rect out{};
                if (!rect_intersect(paint, frame.clip_state.rect, out)) {
                    --sp;
                    continue;
                }
            }
            draw_node(frame.h, frame.world_rect);
            frame.child = kernel_.first_child(frame.h);
            frame.child_offset_x = frame.offset_x + local_rect.x;
            frame.child_offset_y = frame.offset_y + local_rect.y;
            if (is_scrollable_kind(kernel_.kind(frame.h))) {
                frame.child_offset_y -= kernel_.scroll_y(frame.h);
            }
            if (kernel_.clip_children(frame.h)) {
                Rect clip_rect = frame.world_rect;
                Rect out{};
                bool ok = rect_valid(clip_rect);
                if (ok && frame.clip_state.enabled) {
                    ok = rect_intersect(clip_rect, frame.clip_state.rect, out);
                    clip_rect = out;
                }
                if (!ok) {
                    frame.child = {};
                } else {
                    canvas_.set_clip(clip_rect);
                    frame.clip_applied = true;
                }
            }
            continue;
        }

        if (!frame.child) {
            if (frame.clip_applied) {
                canvas_.restore_clip(frame.clip_state);
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
            canvas_.save_clip(),
            false,
            frame.child_offset_x,
            frame.child_offset_y,
            0,
            0,
            Rect{}
        };
    }
}

void SoaGui::draw_node(WidgetHandle h, const Rect& world_rect) {
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
        draw_scroll_container(canvas_, world_rect, colors, metrics, state);
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
        draw_label(canvas_, world_rect, colors, metrics, state, kernel_.text(h));
        break;
    case WidgetKind::Button:
        draw_button(canvas_, world_rect, colors, metrics, state, kernel_.text(h));
        break;
    case WidgetKind::Checkbox:
        draw_checkbox(canvas_, world_rect, colors, metrics, state, kernel_.text(h), kernel_.checked(h));
        break;
    case WidgetKind::Led:
        unsupported_kind(kind);
        break;
    case WidgetKind::Slider:
        draw_slider(canvas_, world_rect, colors, metrics, state,
                    kernel_.value(h), kernel_.min_value(h), kernel_.max_value(h));
        break;
    case WidgetKind::Switch:
        draw_switch(canvas_, world_rect, colors, metrics, state, kernel_.checked(h));
        break;
    case WidgetKind::Progress:
        draw_progress(canvas_, world_rect, colors, metrics, state,
                      kernel_.value(h), kernel_.min_value(h), kernel_.max_value(h));
        break;
    case WidgetKind::List:
        draw_list(canvas_, world_rect, colors, metrics, state);
        break;
    case WidgetKind::ListItem:
        draw_list_item(canvas_, world_rect, colors, metrics, state, kernel_.text(h), kernel_.checked(h));
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
        unsupported_kind(kind);
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
        draw_radio(canvas_, world_rect, colors, metrics, state, kernel_.text(h), kernel_.checked(h));
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

void SoaGui::draw_label(CanvasBase& cvs, const Rect& r, const ResolvedColors& colors,
                        const ResolvedMetrics& metrics, const StyleState& state, const char* text) {
    (void)state;
    const auto font = colors.font;
    draw_text_box(cvs, r, text ? text : "", font, font_from_metrics(metrics),
                  TextAlignH::Left, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
}

void SoaGui::draw_button(CanvasBase& cvs, const Rect& r, const ResolvedColors& colors,
                         const ResolvedMetrics& metrics, const StyleState& state, const char* text) {
    const int rad = metrics.corner_radius;
    ui::render::draw_round_rect(cvs, r.x, r.y, r.w, r.h, rad, colors.bg, true);
    ui::render::draw_round_rect(cvs, r.x, r.y, r.w, r.h, rad, colors.border, false);
    draw_text_box(cvs, r, text ? text : "", colors.font, font_from_metrics(metrics),
                  TextAlignH::Center, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
    ui::render::draw_focus_ring(cvs, r, colors.border_focus, metrics.corner_radius, state.focused, 0, rad);
}

void SoaGui::draw_switch(CanvasBase& cvs, const Rect& r, const ResolvedColors& colors,
                         const ResolvedMetrics& metrics, const StyleState& state, bool checked) {
    (void)metrics;
    (void)state;
    const int rad = r.h / 2;
    const rgba track = checked ? colors.accent : colors.bg;
    ui::render::draw_round_rect(cvs, r.x, r.y, r.w, r.h, rad, track, true);
    ui::render::draw_round_rect(cvs, r.x, r.y, r.w, r.h, rad, colors.border, false);
    const int knob = r.h - 4;
    const int knob_x = checked ? (r.x + r.w - knob - 2) : (r.x + 2);
    ui::render::draw_round_rect(cvs, knob_x, r.y + 2, knob, knob, knob / 2, colors.on_accent, true);
}

void SoaGui::draw_checkbox(CanvasBase& cvs, const Rect& r, const ResolvedColors& colors,
                           const ResolvedMetrics& metrics, const StyleState& state,
                           const char* text, bool checked) {
    int box = r.h;
    if (box > r.w) box = r.w;
    const int box_x = r.x;
    const int box_y = r.y + (r.h - box) / 2;
    ui::render::draw_rect(cvs, box_x, box_y, box, box, colors.border, false);
    if (checked && box > 4) {
        ui::render::draw_rect(cvs, box_x + 2, box_y + 2, box - 4, box - 4, colors.accent, true);
    }
    Rect text_r{
        r.x + box + metrics.padding,
        r.y,
        r.w - box - metrics.padding,
        r.h
    };
    if (text_r.w < 0) text_r.w = 0;
    draw_text_box(cvs, text_r, text ? text : "", colors.font, font_from_metrics(metrics),
                  TextAlignH::Left, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
    ui::render::draw_focus_ring(cvs, r, colors.border_focus, metrics.corner_radius, state.focused);
}

void SoaGui::draw_radio(CanvasBase& cvs, const Rect& r, const ResolvedColors& colors,
                        const ResolvedMetrics& metrics, const StyleState& state,
                        const char* text, bool checked) {
    const int pad = metrics.padding;
    int radius = r.h / 2;
    if (radius < 2) radius = 2;
    const int cx = r.x + pad + radius;
    const int cy = r.y + r.h / 2;
    ui::render::draw_circle(cvs, cx, cy, radius, colors.border, false);
    if (checked && radius > 2) {
        ui::render::draw_circle(cvs, cx, cy, radius - 2, colors.accent, true);
    }
    Rect text_r{
        cx + radius + pad,
        r.y,
        r.w - (radius * 2 + pad * 2),
        r.h
    };
    if (text_r.w < 0) text_r.w = 0;
    draw_text_box(cvs, text_r, text ? text : "", colors.font, font_from_metrics(metrics),
                  TextAlignH::Left, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
    ui::render::draw_focus_ring(cvs, r, colors.border_focus, metrics.corner_radius, state.focused);
}

void SoaGui::draw_list(CanvasBase& cvs, const Rect& r, const ResolvedColors& colors,
                       const ResolvedMetrics& metrics, const StyleState& state) {
    (void)metrics;
    (void)state;
    ui::render::draw_rect(cvs, r.x, r.y, r.w, r.h, colors.bg, true);
    ui::render::draw_rect(cvs, r.x, r.y, r.w, r.h, colors.border, false);
}

void SoaGui::draw_list_item(CanvasBase& cvs, const Rect& r, const ResolvedColors& colors,
                            const ResolvedMetrics& metrics, const StyleState& state,
                            const char* text, bool selected) {
    rgba bg = colors.bg;
    rgba font = colors.font;
    if (selected) {
        bg = colors.accent;
        font = colors.on_accent;
    }
    ui::render::draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
    ui::render::draw_rect(cvs, r.x, r.y, r.w, r.h, colors.border, false);
    Rect text_r{
        r.x + metrics.padding,
        r.y,
        r.w - metrics.padding * 2,
        r.h
    };
    if (text_r.w < 0) text_r.w = 0;
    draw_text_box(cvs, text_r, text ? text : "", font, font_from_metrics(metrics),
                  TextAlignH::Left, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
    ui::render::draw_focus_ring(cvs, r, colors.border_focus, metrics.corner_radius, state.focused);
}

void SoaGui::draw_scroll_container(CanvasBase& cvs, const Rect& r, const ResolvedColors& colors,
                                   const ResolvedMetrics& metrics, const StyleState& state) {
    (void)metrics;
    ui::render::draw_rect(cvs, r.x, r.y, r.w, r.h, colors.bg, true);
    ui::render::draw_rect(cvs, r.x, r.y, r.w, r.h, colors.border, false);
    ui::render::draw_focus_ring(cvs, r, colors.border_focus, metrics.corner_radius, state.focused);
}

void SoaGui::draw_slider(CanvasBase& cvs, const Rect& r, const ResolvedColors& colors,
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
    ui::render::draw_rect(cvs, r.x + pad, track_y, inner_w, track_h, colors.border, true);
    ui::render::draw_rect(cvs, r.x + pad, track_y, fill, track_h, colors.accent, true);
    const int knob = r.h - pad * 2;
    const int knob_x = r.x + pad + fill - knob / 2;
    ui::render::draw_round_rect(cvs, knob_x, r.y + pad, knob, knob, knob / 2, colors.accent, true);
}

void SoaGui::draw_progress(CanvasBase& cvs, const Rect& r, const ResolvedColors& colors,
                           const ResolvedMetrics& metrics, const StyleState& state,
                           int value, int min_value, int max_value) {
    (void)state;
    const int pad = metrics.padding;
    const int inner_w = r.w - pad * 2;
    const int inner_h = r.h - pad * 2;
    if (inner_w <= 0 || inner_h <= 0) return;
    const int range = (max_value > min_value) ? (max_value - min_value) : 1;
    const int fill = (inner_w * (value - min_value)) / range;
    ui::render::draw_rect(cvs, r.x + pad, r.y + pad, inner_w, inner_h, colors.border, false);
    ui::render::draw_rect(cvs, r.x + pad, r.y + pad, fill, inner_h, colors.accent, true);
}
