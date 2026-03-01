module;
#include <array>
#include <cstddef>
#include <cstdint>

export module charm.core.soa_gui;

export import charm.core.soa_kernel;
export import charm.core.geometry;
export import charm.core.style;
export import charm.core.style_sheet;
export import charm.core.event;
export import charm.core.container;
export import charm.gfx.canvas;
export import charm.gfx.render;
export import charm.widgets.text;
export import charm.widgets.button;
export import charm.widgets.label;
export import charm.widgets.progress;
export import charm.widgets.slider;
export import charm.widgets.switcher;

namespace {
    constexpr std::size_t kWidgetKindCount =
        static_cast<std::size_t>(WidgetKind::Histogram) + 1;

    constexpr int clamp_int(int v, int lo, int hi) noexcept {
        return (v < lo) ? lo : (v > hi ? hi : v);
    }

    struct StyleTable {
        std::array<Style, kWidgetKindCount> styles{};
    };

    const Style& style_for_kind(const StyleTable& table, WidgetKind kind) noexcept {
        const auto idx = static_cast<std::size_t>(kind);
        if (idx >= table.styles.size()) {
            return table.styles[static_cast<std::size_t>(WidgetKind::Container)];
        }
        return table.styles[idx];
    }

    StyleState make_state(const SoaKernel& kernel, WidgetHandle h) noexcept {
        const bool enabled = kernel.enabled(h);
        const bool hovered = kernel.hovered(h);
        const bool pressed = kernel.pressed(h);
        const bool focused = kernel.focused(h);
        return make_style_state(enabled, hovered, pressed, focused, kernel.variant(h));
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
    WidgetHandle hit_test(int x, int y) const noexcept;

private:
    CanvasBase& canvas_;
    SoaKernel& kernel_;
    WidgetHandle root_{};
    WidgetHandle hovered_{};
    WidgetHandle pressed_{};
    StyleTable style_table_{};
    std::uint32_t style_version_{0};

    void refresh_styles();
    const Style& resolve_style(WidgetKind kind, const StyleState& state, Style& scratch) const noexcept;
    void handle_hover(int x, int y);
    void update_slider_value(WidgetHandle h, int x);
    void draw_tree();
    void draw_node(WidgetHandle h);

    static void draw_label(CanvasBase& cvs, const Rect& r, const Style& st, const StyleState& state, const char* text);
    static void draw_button(CanvasBase& cvs, const Rect& r, const Style& st, const StyleState& state, const char* text);
    static void draw_switch(CanvasBase& cvs, const Rect& r, const Style& st, const StyleState& state, bool checked);
    static void draw_slider(CanvasBase& cvs, const Rect& r, const Style& st, const StyleState& state,
                            int value, int min_value, int max_value);
    static void draw_progress(CanvasBase& cvs, const Rect& r, const Style& st, const StyleState& state,
                              int value, int min_value, int max_value);
};

SoaGui::SoaGui(CanvasBase& canvas, SoaKernel& kernel, WidgetHandle root) noexcept
    : canvas_(canvas), kernel_(kernel), root_(root) {
    refresh_styles();
}

void SoaGui::set_root(WidgetHandle root) noexcept {
    root_ = root;
}

WidgetHandle SoaGui::root() const noexcept {
    return root_;
}

void SoaGui::render() {
    refresh_styles();
    canvas_.begin_frame();
    draw_tree();
    canvas_.end_frame();
}

void SoaGui::dispatch_event(const Event& e) {
    if (!root_) return;
    switch (e.type) {
    case Event::Type::MouseMove:
        handle_hover(e.x, e.y);
        if (pressed_) {
            auto kind = kernel_.kind(pressed_);
            if (kind == WidgetKind::Slider) {
                update_slider_value(pressed_, e.x);
            }
        }
        break;
    case Event::Type::MouseDown:
        pressed_ = hit_test(e.x, e.y);
        if (pressed_) {
            kernel_.set_pressed(pressed_, true);
            if (kernel_.kind(pressed_) == WidgetKind::Slider) {
                update_slider_value(pressed_, e.x);
            }
        }
        break;
    case Event::Type::MouseUp:
        if (pressed_) {
            kernel_.set_pressed(pressed_, false);
            auto hit = hit_test(e.x, e.y);
            if (hit == pressed_) {
                if (kernel_.kind(pressed_) == WidgetKind::Switch) {
                    kernel_.set_checked(pressed_, !kernel_.checked(pressed_));
                }
            }
            pressed_ = {};
        }
        break;
    default:
        break;
    }
}

WidgetHandle SoaGui::hit_test(int x, int y) const noexcept {
    if (!root_) return {};
    struct Frame {
        WidgetHandle h{};
    };
    std::array<Frame, 256> stack{};
    std::size_t sp = 0;
    stack[sp++] = Frame{root_};
    WidgetHandle result{};
    while (sp > 0) {
        auto frame = stack[--sp];
        if (!kernel_.valid(frame.h)) continue;
        if (!kernel_.visible(frame.h)) continue;
        Rect r = kernel_.rect(frame.h);
        if (!r.contains(x, y)) continue;
        if (kernel_.hit_testable(frame.h)) {
            result = frame.h;
        }
        for (auto child = kernel_.last_child(frame.h); child; child = kernel_.prev_sibling(child)) {
            if (sp < stack.size()) {
                stack[sp++] = Frame{child};
            }
        }
    }
    return result;
}

void SoaGui::refresh_styles() {
    const auto version = Theme::instance().get_tokens().version;
    if (version == style_version_) return;
    style_version_ = version;
    const Style fallback = Theme::instance().get<Container>();
    style_table_.styles.fill(fallback);
    style_table_.styles[static_cast<std::size_t>(WidgetKind::Container)] = Theme::instance().get<Container>();
    style_table_.styles[static_cast<std::size_t>(WidgetKind::Label)] = Theme::instance().get<Label>();
    style_table_.styles[static_cast<std::size_t>(WidgetKind::Button)] = Theme::instance().get<Button>();
    style_table_.styles[static_cast<std::size_t>(WidgetKind::Switch)] = Theme::instance().get<Switch>();
    style_table_.styles[static_cast<std::size_t>(WidgetKind::Slider)] = Theme::instance().get<Slider>();
    style_table_.styles[static_cast<std::size_t>(WidgetKind::Progress)] = Theme::instance().get<Progress>();
}

const Style& SoaGui::resolve_style(WidgetKind kind, const StyleState& state, Style& scratch) const noexcept {
    const Style& base = style_for_kind(style_table_, kind);
    if (StyleSheet::instance().apply(kind, state, scratch, base)) {
        return scratch;
    }
    return base;
}

void SoaGui::handle_hover(int x, int y) {
    WidgetHandle hit = hit_test(x, y);
    if (hit == hovered_) return;
    if (hovered_) {
        kernel_.set_hovered(hovered_, false);
    }
    hovered_ = hit;
    if (hovered_) {
        kernel_.set_hovered(hovered_, true);
    }
}

void SoaGui::update_slider_value(WidgetHandle h, int x) {
    Rect r = kernel_.rect(h);
    Style scratch{};
    const StyleState state = make_state(kernel_, h);
    const Style& st = resolve_style(WidgetKind::Slider, state, scratch);
    const int pad = st.metrics.padding;
    const int inner_w = r.w - pad * 2;
    if (inner_w <= 0) return;
    const int min_v = kernel_.min_value(h);
    const int max_v = kernel_.max_value(h);
    const int range = (max_v > min_v) ? (max_v - min_v) : 1;
    const int x0 = r.x + pad;
    const int x1 = x0 + inner_w;
    const int clamped = clamp_int(x, x0, x1);
    const int value = min_v + (clamped - x0) * range / inner_w;
    kernel_.set_value(h, value);
}

void SoaGui::draw_tree() {
    if (!root_) return;
    struct Frame {
        WidgetHandle h{};
        WidgetHandle child{};
        bool entered{false};
        CanvasBase::ClipState clip_state{};
        bool clip_applied{false};
    };
    std::array<Frame, 256> stack{};
    std::size_t sp = 0;
    stack[sp++] = Frame{root_, {}, false, canvas_.save_clip(), false};

    while (sp > 0) {
        auto& frame = stack[sp - 1];
        if (!frame.entered) {
            frame.entered = true;
            if (!kernel_.valid(frame.h) || !kernel_.visible(frame.h)) {
                --sp;
                continue;
            }
            Rect r = kernel_.paint_bounds(frame.h);
            if (!rect_valid(r)) {
                r = kernel_.rect(frame.h);
            }
            if (frame.clip_state.enabled) {
                Rect out{};
                if (!rect_intersect(r, frame.clip_state.rect, out)) {
                    --sp;
                    continue;
                }
            }
            draw_node(frame.h);
            frame.child = kernel_.first_child(frame.h);
            if (kernel_.clip_children(frame.h)) {
                Rect clip_rect = kernel_.rect(frame.h);
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
        stack[sp++] = Frame{child, {}, false, canvas_.save_clip(), false};
    }
}

void SoaGui::draw_node(WidgetHandle h) {
    const WidgetKind kind = kernel_.kind(h);
    Style scratch{};
    const StyleState state = make_state(kernel_, h);
    const Style& st = resolve_style(kind, state, scratch);
    const Rect r = kernel_.rect(h);
    switch (kind) {
    case WidgetKind::Container:
        break;
    case WidgetKind::Label:
        draw_label(canvas_, r, st, state, kernel_.text(h));
        break;
    case WidgetKind::Button:
        draw_button(canvas_, r, st, state, kernel_.text(h));
        break;
    case WidgetKind::Switch:
        draw_switch(canvas_, r, st, state, kernel_.checked(h));
        break;
    case WidgetKind::Slider:
        draw_slider(canvas_, r, st, state, kernel_.value(h), kernel_.min_value(h), kernel_.max_value(h));
        break;
    case WidgetKind::Progress:
        draw_progress(canvas_, r, st, state, kernel_.value(h), kernel_.min_value(h), kernel_.max_value(h));
        break;
    default:
        break;
    }
}

void SoaGui::draw_label(CanvasBase& cvs, const Rect& r, const Style& st, const StyleState& state, const char* text) {
    rgba bg{};
    rgba border{};
    rgba font{};
    resolve_colors(st, state, bg, border, font);
    (void)bg;
    (void)border;
    draw_text_box(cvs, r, text ? text : "", font, resolve_font(st),
                  TextAlignH::Left, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
}

void SoaGui::draw_button(CanvasBase& cvs, const Rect& r, const Style& st, const StyleState& state, const char* text) {
    rgba bg{};
    rgba border{};
    rgba font{};
    resolve_colors(st, state, bg, border, font);
    const int rad = st.metrics.corner_radius;
    ui::render::draw_round_rect(cvs, r.x, r.y, r.w, r.h, rad, bg, true);
    ui::render::draw_round_rect(cvs, r.x, r.y, r.w, r.h, rad, border, false);
    draw_text_box(cvs, r, text ? text : "", font, resolve_font(st),
                  TextAlignH::Center, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
    ui::render::draw_focus_ring(cvs, r, st, state.focused, 0, rad);
}

void SoaGui::draw_switch(CanvasBase& cvs, const Rect& r, const Style& st, const StyleState& state, bool checked) {
    rgba bg{};
    rgba border{};
    rgba font{};
    resolve_colors(st, state, bg, border, font);
    (void)font;
    const rgba accent = resolve_accent(st, state);
    const int rad = r.h / 2;
    const rgba track = checked ? accent : bg;
    ui::render::draw_round_rect(cvs, r.x, r.y, r.w, r.h, rad, track, true);
    ui::render::draw_round_rect(cvs, r.x, r.y, r.w, r.h, rad, border, false);
    const int knob = r.h - 4;
    const int knob_x = checked ? (r.x + r.w - knob - 2) : (r.x + 2);
    ui::render::draw_round_rect(cvs, knob_x, r.y + 2, knob, knob, knob / 2, st.colors.on_accent, true);
}

void SoaGui::draw_slider(CanvasBase& cvs, const Rect& r, const Style& st, const StyleState& state,
                         int value, int min_value, int max_value) {
    rgba bg{};
    rgba border{};
    rgba font{};
    resolve_colors(st, state, bg, border, font);
    (void)bg;
    (void)font;
    const rgba accent = resolve_accent(st, state);
    const int pad = st.metrics.padding;
    const int track_h = 4;
    const int inner_w = r.w - pad * 2;
    if (inner_w <= 0) return;
    const int range = (max_value > min_value) ? (max_value - min_value) : 1;
    const int fill = (inner_w * (value - min_value)) / range;
    const int track_y = r.y + (r.h - track_h) / 2;
    ui::render::draw_rect(cvs, r.x + pad, track_y, inner_w, track_h, border, true);
    ui::render::draw_rect(cvs, r.x + pad, track_y, fill, track_h, accent, true);
    const int knob = r.h - pad * 2;
    const int knob_x = r.x + pad + fill - knob / 2;
    ui::render::draw_round_rect(cvs, knob_x, r.y + pad, knob, knob, knob / 2, accent, true);
}

void SoaGui::draw_progress(CanvasBase& cvs, const Rect& r, const Style& st, const StyleState& state,
                           int value, int min_value, int max_value) {
    rgba bg{};
    rgba border{};
    rgba font{};
    resolve_colors(st, state, bg, border, font);
    (void)bg;
    (void)font;
    const rgba accent = resolve_accent(st, state);
    const int pad = st.metrics.padding;
    const int inner_w = r.w - pad * 2;
    const int inner_h = r.h - pad * 2;
    if (inner_w <= 0 || inner_h <= 0) return;
    const int range = (max_value > min_value) ? (max_value - min_value) : 1;
    const int fill = (inner_w * (value - min_value)) / range;
    ui::render::draw_rect(cvs, r.x + pad, r.y + pad, inner_w, inner_h, border, false);
    ui::render::draw_rect(cvs, r.x + pad, r.y + pad, fill, inner_h, accent, true);
}
