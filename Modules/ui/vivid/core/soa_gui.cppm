module;

#include "vivid_features.generated.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <type_traits>
#include <utility>

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
import charm.core.soa_gui.style_support;
import charm.core.soa_gui.basic_recorders;
import charm.core.soa_gui.collection_recorders;
import charm.core.soa_gui.feedback_recorders;

using namespace ui::soa_gui_detail;

namespace {
    StyleState make_state(const SoaKernel& kernel, WidgetHandle h) noexcept {
        const StateCompact state = kernel.state_compact(h);
        return make_style_state(state.enabled(), state.hovered(), state.pressed(), state.focused(), state.variant);
    }

    constexpr std::size_t kMaxSegments = 8;

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
    using DrawCmdBuffer = ui::draw_cmd::DefaultDrawCmdBuffer;
    using CompactionWorkspace = ui::draw_cmd::DefaultDrawCmdCompactionWorkspace;

    SoaGui(CanvasBase& canvas,
           SoaKernel& kernel,
           WidgetHandle root,
           DrawCmdBuffer& cmd_buffer,
           CompactionWorkspace& compaction_workspace,
           ui::draw_cmd::DrawCmdExecutor& cmd_exec) noexcept;

    SoaGui(const SoaGui&) = delete;
    SoaGui& operator=(const SoaGui&) = delete;
    SoaGui(SoaGui&&) = delete;
    SoaGui& operator=(SoaGui&&) = delete;

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
    struct RenderTraversalFrame {
        static constexpr std::uint8_t kEntered = 1u << 0;
        static constexpr std::uint8_t kClipEnabled = 1u << 1;
        static constexpr std::uint8_t kClipPushed = 1u << 2;

        WidgetHandle h{};
        WidgetHandle child{};
        std::uint8_t flags{0};
        Rect clip_rect{};
        int offset_x{0};
        int offset_y{0};
        int child_offset_x{0};
        int child_offset_y{0};
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
        ui::draw_cmd::DrawScope draw_scope{};
#endif

        constexpr bool entered() const noexcept { return (flags & kEntered) != 0; }
        constexpr bool clip_enabled() const noexcept { return (flags & kClipEnabled) != 0; }
        constexpr bool clip_pushed() const noexcept { return (flags & kClipPushed) != 0; }
        constexpr void set_entered() noexcept { flags |= kEntered; }
        constexpr void set_clip_enabled() noexcept { flags |= kClipEnabled; }
        constexpr void set_clip_pushed() noexcept { flags |= kClipPushed; }
    };

public:
    static constexpr std::size_t kTraversalWorkspaceBytes =
        sizeof(std::array<RenderTraversalFrame, SoaKernel::kMaxNodes>)
        + SoaLayoutPass::kTraversalWorkspaceBytes
        + SoaKernel::kTraversalWorkspaceBytes;

private:
    CanvasBase& canvas_;
    SoaKernel& kernel_;
    WidgetHandle root_{};
    SoaLayoutPass layout_;
    std::uint32_t style_version_{0};
    std::uint32_t stylesheet_version_{0};
    DrawCmdBuffer& cmd_buffer_;
    CompactionWorkspace& compaction_workspace_;
    ui::draw_cmd::DrawCmdExecutor& cmd_exec_;
    std::array<RenderTraversalFrame, SoaKernel::kMaxNodes> render_stack_{};
    ui::draw_cmd::DrawCmdStats last_cmd_stats_{};
    ui::draw_cmd::DrawCmdExecStats last_exec_stats_{};

    void refresh_styles();
    ResolvedStyleView resolve_style(WidgetKind kind, const StyleState& state) const noexcept;
    void record_tree(ui::draw_cmd::DefaultDrawCmdBuffer& out);
    void record_node(WidgetHandle h, const Rect& world_rect, ui::draw_cmd::DefaultDrawCmdBuffer& out);
};

static_assert(!std::is_copy_constructible_v<SoaGui>
              && !std::is_copy_assignable_v<SoaGui>
              && !std::is_move_constructible_v<SoaGui>
              && !std::is_move_assignable_v<SoaGui>,
              "SoaGui must not outlive or detach from its injected runtime storage");

SoaGui::SoaGui(CanvasBase& canvas,
               SoaKernel& kernel,
               WidgetHandle root,
               DrawCmdBuffer& cmd_buffer,
               CompactionWorkspace& compaction_workspace,
               ui::draw_cmd::DrawCmdExecutor& cmd_exec) noexcept
    : canvas_(canvas),
      kernel_(kernel),
      root_(root),
      layout_(kernel),
      cmd_buffer_(cmd_buffer),
      compaction_workspace_(compaction_workspace),
      cmd_exec_(cmd_exec) {
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
        (void)cmd_buffer_.compact(compaction_workspace_);
        last_cmd_stats_ = cmd_buffer_.stats();
        last_cmd_stats_.workspace_overflowed = kernel_.workspace_overflowed();
        last_cmd_stats_.style_patch_overflowed = kernel_.style_patch_overflowed();
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
        (void)out.compact(compaction_workspace_);
        last_cmd_stats_ = out.stats();
        last_cmd_stats_.workspace_overflowed = kernel_.workspace_overflowed();
        last_cmd_stats_.style_patch_overflowed = kernel_.style_patch_overflowed();
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
        (void)cmd_buffer_.compact(compaction_workspace_);
        last_cmd_stats_ = cmd_buffer_.stats();
        last_cmd_stats_.workspace_overflowed = kernel_.workspace_overflowed();
        last_cmd_stats_.style_patch_overflowed = kernel_.style_patch_overflowed();
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
    auto& stack = render_stack_;
    std::size_t sp = 0;
    const auto base_clip = canvas_.save_clip();
    stack[sp++] = RenderTraversalFrame{
        root_,
        {},
        base_clip.enabled ? RenderTraversalFrame::kClipEnabled : std::uint8_t{0},
        base_clip.rect,
        0,
        0,
        0,
        0
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
        , ui::draw_cmd::draw_scope_default()
#endif
    };

    while (sp > 0) {
        auto& frame = stack[sp - 1];
        if (!frame.entered()) {
            frame.set_entered();
            if (!kernel_.valid(frame.h) || !kernel_.visible(frame.h)) {
                --sp;
                continue;
            }
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
            const std::uint16_t node_scope = kernel_.draw_scope(frame.h);
            if (node_scope != ui::draw_cmd::kDrawScopeDefault) {
                frame.draw_scope = ui::draw_cmd::DrawScope{node_scope};
            }
            out.set_draw_scope(frame.draw_scope);
#endif
            const Rect local_rect = kernel_.rect(frame.h);
            const Rect world_rect{
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
            if (frame.clip_enabled()) {
                Rect out_clip{};
                if (!rect_intersect(paint, frame.clip_rect, out_clip)) {
                    --sp;
                    continue;
                }
            }
            record_node(frame.h, world_rect, out);
            frame.child = kernel_.first_child(frame.h);
            frame.child_offset_x = frame.offset_x + local_rect.x;
            frame.child_offset_y = frame.offset_y + local_rect.y;
            if (is_scrollable_kind(kernel_.kind(frame.h))) {
                frame.child_offset_y -= kernel_.scroll_y(frame.h);
            }
            if (kernel_.clip_children(frame.h)) {
                Rect clip_rect = world_rect;
                Rect out_clip{};
                bool ok = rect_valid(clip_rect);
                if (ok && frame.clip_enabled()) {
                    ok = rect_intersect(clip_rect, frame.clip_rect, out_clip);
                    clip_rect = out_clip;
                }
                if (!ok) {
                    frame.child = {};
                } else {
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                    out.set_draw_scope(frame.draw_scope);
#endif
                    out.push_clip(clip_rect);
                    frame.set_clip_pushed();
                    frame.clip_rect = clip_rect;
                    frame.set_clip_enabled();
                }
            }
            continue;
        }

        if (!frame.child) {
            if (frame.clip_pushed()) {
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                out.set_draw_scope(frame.draw_scope);
#endif
                out.pop_clip();
            }
            --sp;
            continue;
        }

        WidgetHandle child = frame.child;
        frame.child = kernel_.next_sibling(child);
        if (sp >= stack.size()) {
            kernel_.note_workspace_overflow();
            continue;
        }
        stack[sp++] = RenderTraversalFrame{
            child,
            {},
            frame.clip_enabled() ? RenderTraversalFrame::kClipEnabled : std::uint8_t{0},
            frame.clip_rect,
            frame.child_offset_x,
            frame.child_offset_y,
            0,
            0
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
            , frame.draw_scope
#endif
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
        record_image(out,
                     world_rect,
                     kernel_.image(h),
                     metrics->corner_radius,
                     kernel_.image_shape_kind(h),
                     kernel_.image_shape_extent(h),
                     kernel_.image_rotation_deg(h));
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
