module;
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>

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
                             const ResolvedMetrics& metrics, const StyleState& state, const char* text);
    static void record_button(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r, const ResolvedColors& colors,
                              const ResolvedMetrics& metrics, const StyleState& state, const char* text,
                              ui::draw_cmd::ImageId icon, std::uint8_t icon_size);
    static void record_image(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
                             ui::draw_cmd::ImageId image);
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
        ui::draw_cmd::set_image_registry_locked(true);
        record_tree(cmd_buffer_);
        ui::draw_cmd::set_image_registry_locked(false);
        last_cmd_stats_ = cmd_buffer_.stats();
        canvas_.begin_frame();
        last_exec_stats_ = cmd_exec_.execute(canvas_, cmd_buffer_);
        canvas_.end_frame();
    }

    ui::draw_cmd::DrawCmdStats SoaGui::record_commands(ui::draw_cmd::DefaultDrawCmdBuffer& out) {
        refresh_styles();
        layout_.run_if_needed(root_);
        out.clear();
        ui::draw_cmd::set_image_registry_locked(true);
        record_tree(out);
        ui::draw_cmd::set_image_registry_locked(false);
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
    ui::draw_cmd::set_image_registry_locked(true);
    record_tree(cmd_buffer_);
    ui::draw_cmd::set_image_registry_locked(false);
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
        record_image(out, world_rect, kernel_.image(h));
        break;
    case WidgetKind::Label:
        record_label(out, world_rect, colors, metrics, state, kernel_.text(h));
        break;
        case WidgetKind::Button:
        case WidgetKind::IconButton:
            record_button(out, world_rect, colors, metrics, state, kernel_.text(h),
                          kernel_.button_icon(h), kernel_.button_icon_size(h));
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
        record_list_view(out, world_rect, colors, metrics, state, kernel_, h);
        break;
    case WidgetKind::IconList:
        record_list_view(out, world_rect, colors, metrics, state, kernel_, h);
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
            record_text_list(out, world_rect, colors, metrics, state,
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
            record_text_list(out, world_rect, colors, metrics, state,
                             items.data(), count, -1,
                             kernel_.scroll_y(h), kernel_.list_row_height(h));
        }
        break;
    case WidgetKind::ModalDialog:
        unsupported_kind(kind);
        break;
    case WidgetKind::ProgressBarSimple:
        record_progress_bar_simple(out, world_rect, colors, metrics,
                                   kernel_.value(h), kernel_.min_value(h), kernel_.max_value(h));
        break;
    case WidgetKind::ProgressBarRound:
        record_progress_bar_round(out, world_rect, colors, metrics,
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
            record_scrollbar(out, world_rect, colors, metrics, orient, scroll_y, max_scroll, page_size);
            if (state.focused) {
                out.focus_ring(world_rect, colors.border_focus, metrics.corner_radius, 0, -1);
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
            record_segmented_control(out, world_rect, colors, metrics, state, state.variant,
                                     labels.data(), count, kernel_.segmented_selected(h));
        }
        break;
    case WidgetKind::TextArea:
        record_text_box(out, world_rect, colors, metrics, state, kernel_.text(h),
                        TextAlignV::Top, TextWrap::Word);
        break;
    case WidgetKind::TextInput:
        record_text_box(out, world_rect, colors, metrics, state, kernel_.text(h),
                        TextAlignV::Center, TextWrap::None);
        break;
    case WidgetKind::NumberInput:
        record_text_box(out, world_rect, colors, metrics, state, kernel_.text(h),
                        TextAlignV::Center, TextWrap::None);
        break;
    case WidgetKind::TextBox:
        record_text_box(out, world_rect, colors, metrics, state, kernel_.text(h),
                        TextAlignV::Top, TextWrap::Word);
        break;
    case WidgetKind::ToggleGroup:
        break;
        break;
    case WidgetKind::TableView:
        record_table_view(out, world_rect, colors, metrics, state, kernel_, h);
        break;
    case WidgetKind::TreeView:
        record_tree_view(out, world_rect, colors, metrics, state, kernel_, h);
        break;
    case WidgetKind::Dropdown:
        unsupported_kind(kind);
        break;
    case WidgetKind::Roller:
        record_roller(out, world_rect, colors, metrics, state, kernel_, h);
        break;
    case WidgetKind::Spinner:
        record_spinner(out, world_rect, colors, kernel_.spinner_phase(h));
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
        record_list(out, world_rect, colors, metrics, state, 0, 0);
        if (state.focused) {
            out.focus_ring(world_rect, colors.border_focus, metrics.corner_radius, 0, -1);
        }
        break;
    case WidgetKind::MenuItem:
        record_list_item(out, world_rect, colors, metrics, state,
                         kernel_.text(h), kernel_.checked(h));
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
        {
            const std::uint8_t count = kernel_.stepper_count(h);
            std::array<const char*, soa_detail::kMaxStepperSteps> labels{};
            for (std::uint8_t i = 0; i < count && i < labels.size(); ++i) {
                labels[i] = kernel_.stepper_label(h, i);
            }
            record_stepper(out, world_rect, colors, metrics, state,
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
        record_progress_wheel(out, world_rect, colors, metrics,
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
        record_progress_flowing(out, world_rect, colors, metrics,
                                kernel_.value(h), kernel_.min_value(h), kernel_.max_value(h));
        break;
    case WidgetKind::CloudyGlass:
        unsupported_kind(kind);
        break;
    case WidgetKind::NumberList:
        record_number_list(out, world_rect, colors, metrics, state, kernel_, h);
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
                          const ResolvedMetrics& metrics, const StyleState& state, const char* text) {
    (void)state;
    out.draw_text_box(r, text ? text : "", colors.font, font_from_metrics(metrics),
                      TextAlignH::Left, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
}

void SoaGui::record_button(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r, const ResolvedColors& colors,
                           const ResolvedMetrics& metrics, const StyleState& state, const char* text,
                           ui::draw_cmd::ImageId icon, std::uint8_t icon_size) {
    const int rad = metrics.corner_radius;
    out.fill_round_rect(r, rad, colors.bg);
    out.stroke_round_rect(r, rad, colors.border);
    Rect text_rect = r;
    TextAlignH align = TextAlignH::Center;
    if (ui::draw_cmd::image_id_valid(icon)) {
        int icon_px = static_cast<int>(icon_size);
        if (icon_px <= 0) {
            icon_px = r.h - metrics.padding * 2;
        }
        if (icon_px > r.h) icon_px = r.h;
        if (icon_px < 4) icon_px = r.h;
        const int icon_x = r.x + metrics.padding;
        const int icon_y = r.y + (r.h - icon_px) / 2;
        out.draw_icon(Rect{icon_x, icon_y, icon_px, icon_px}, icon);
        text_rect.x = icon_x + icon_px + metrics.padding;
        text_rect.w = r.x + r.w - metrics.padding - text_rect.x;
        if (text_rect.w < 0) text_rect.w = 0;
        align = TextAlignH::Left;
    }
    out.draw_text_box(text_rect, text ? text : "", colors.font, font_from_metrics(metrics),
                      align, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
    if (state.focused) {
        out.focus_ring(r, colors.border_focus, metrics.corner_radius, 0, rad);
    }
}

void SoaGui::record_image(ui::draw_cmd::DefaultDrawCmdBuffer& out, const Rect& r,
                          ui::draw_cmd::ImageId image) {
    if (!ui::draw_cmd::image_id_valid(image)) return;
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

    const int icon_size_raw = static_cast<int>(kernel.list_view_icon_size(h));
    for (int i = start; i < end; ++i) {
        Rect row{clip_rect.x, y, clip_rect.w, row_h};
        if (i == selected) {
            out.fill_rect(row, colors.accent);
        }
        const rgba font = (i == selected) ? colors.on_accent : colors.font;
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
        const char* text = kernel.list_view_item_text(h, static_cast<std::uint16_t>(i));
        out.draw_text_box(text_rect, text ? text : "", font, font_from_metrics(metrics),
                          TextAlignH::Left, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
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
