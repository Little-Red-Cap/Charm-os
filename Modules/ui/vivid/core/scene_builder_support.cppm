module;

#include <cstddef>
#include <cstdint>

export module charm.ui.scene.builder_support;

import charm.core.event;
import charm.core.geometry;
import charm.core.handle;
import charm.core.structured_view;
import charm.core.style;
import charm.ui.scene.pill_surface;
import charm.core.soa_factory;
import charm.core.soa_kernel;
import charm.core.soa_payload;
import charm.gfx.image;
import charm.gfx.render_style;
import charm.gfx.text_box;

export namespace ui::scene {
    using TextSlotId = std::uint16_t;
    constexpr TextSlotId kInvalidTextSlot = 0xFFFF;

    using ImageId = ui::gfx::ImageId;
    constexpr ImageId invalid_image_id() noexcept { return ui::gfx::invalid_image_id(); }
    constexpr bool image_id_valid(ImageId id) noexcept { return ui::gfx::image_id_valid(id); }
    using ImageShapeKind = ui::render::ImageShapeKind;

    using ScrollBarOrientation = ::ScrollBarOrientation;
    using SemanticAction = ::SemanticAction;
    using SemanticActionAdmission = ::SemanticActionAdmission;
    using SemanticActionAdmissionStatus = ::SemanticActionAdmissionStatus;
    using SemanticActionMask = ::SemanticActionMask;
    using SemanticActionRequest = ::SemanticActionRequest;
    using SemanticActionRequestLedger = ::SemanticActionRequestLedger;
    using SemanticActionRequestRejectReason = ::SemanticActionRequestRejectReason;
    using SemanticActionRequestStage = ::SemanticActionRequestStage;
    using SemanticActionRequestStatus = ::SemanticActionRequestStatus;
    using SemanticActionSnapshot = ::SemanticActionSnapshot;
    using SemanticFocusAdmission = ::SemanticFocusAdmission;
    using SemanticFocusAdmissionStatus = ::SemanticFocusAdmissionStatus;
    using SemanticFocusQuery = ::SemanticFocusQuery;
    using SemanticFocusQueryStatus = ::SemanticFocusQueryStatus;
    using SemanticFocusRequest = ::SemanticFocusRequest;
    using SemanticFocusRequestLedger = ::SemanticFocusRequestLedger;
    using SemanticFocusRequestStage = ::SemanticFocusRequestStage;
    using SemanticFocusRequestStatus = ::SemanticFocusRequestStatus;
    using SemanticFocusSnapshot = ::SemanticFocusSnapshot;
    using SemanticIntentResolution = ::SemanticIntentResolution;
    using SemanticIntentStatus = ::SemanticIntentStatus;
    using SemanticRole = ::SemanticRole;
    using SemanticTreeNode = ::SemanticTreeNode;
    using SemanticTreeSnapshot = ::SemanticTreeSnapshot;
    using ::semantic_action_admission_status_name;
    using ::semantic_action_mask;
    using ::semantic_action_present;
    using ::semantic_action_request_reject_reason_name;
    using ::semantic_action_request_ledger;
    using ::semantic_action_request_stage_name;
    using ::semantic_action_request_status_name;
    using ::semantic_default_actions_for_role;
    using ::semantic_focus_admission_status_name;
    using ::semantic_focus_query_status_name;
    using ::semantic_focus_request_ledger;
    using ::semantic_focus_request_stage_name;
    using ::semantic_focus_request_status_name;
    using ::semantic_intent_status_name;
    constexpr std::size_t kSemanticTreeMaxNodes = ::kSemanticTreeMaxNodes;
    constexpr std::uint16_t kSemanticTreeNoFocusIndex = ::kSemanticTreeNoFocusIndex;
    using TableViewHeaderStyle = ::TableViewHeaderStyle;
    using TableViewColDividerStyle = ::TableViewColDividerStyle;
    using TextAlignH = ::TextAlignH;
    using TextAlignV = ::TextAlignV;

    using ListViewTextFn = const char* (*)(const void*, std::uint16_t) noexcept;
    using ListViewSubtitleFn = const char* (*)(const void*, std::uint16_t) noexcept;
    using ListViewTailFn = const char* (*)(const void*, std::uint16_t) noexcept;
    using ListViewIconFn = soa_detail::ListViewIconFn;
    using ListViewRowFlagsFn = StructuredListRowFlagsFn;
    constexpr std::uint8_t kListViewRowFlagGroup = kStructuredListRowFlagGroup;
    constexpr std::uint8_t kListViewRowFlagDisabled = kStructuredListRowFlagDisabled;
    using TableViewTextFn = const char* (*)(const void*, std::uint16_t, std::uint8_t) noexcept;
    using TreeViewTextFn = const char* (*)(const void*, std::uint16_t) noexcept;
    using RollerTextFn = const char* (*)(const void*, std::uint16_t) noexcept;

    struct ImageSlotSpec {
        Rect image_rect{};
        int image_corner_radius{0};
        int plate_pad{0};
        ImageShapeKind image_shape{ImageShapeKind::RoundRect};
        std::uint8_t shape_extent{0};
        std::int16_t rotation_deg{0};
        bool create_plate{true};
        bool image_hit_testable{false};
        bool plate_hit_testable{false};
    };

    struct ImageSlotHandles {
        WidgetHandle image{};
        WidgetHandle plate{};
    };

    class SceneAccess {
    public:
        constexpr SceneAccess() noexcept {}
        explicit SceneAccess(SoaKernel& kernel) noexcept : kernel_(&kernel) {}

        bool valid() const noexcept { return kernel_ != nullptr; }

        TextSlotId alloc_text_slot() noexcept { return kernel_->alloc_text_slot(); }
        void free_text_slot(TextSlotId slot) noexcept { kernel_->free_text_slot(slot); }

        WidgetKind kind(WidgetHandle h) const noexcept { return kernel_->kind(h); }
        Rect world_rect(WidgetHandle h) const noexcept { return kernel_->world_rect(h); }

        void set_text(WidgetHandle h, const char* text) noexcept { kernel_->set_text(h, text); }
        void set_text_slot(WidgetHandle h, TextSlotId slot, const char* text) noexcept {
            kernel_->set_text_slot(h, slot, text);
        }
        void set_semantic(WidgetHandle h,
                          SemanticRole role,
                          const char* id,
                          const char* label) noexcept {
            kernel_->set_semantic(h, role, id, label);
        }
        void set_semantic_default(WidgetHandle h,
                                  const char* id,
                                  const char* label = nullptr) noexcept {
            kernel_->set_semantic_default(h, id, label);
        }
        void clear_semantic(WidgetHandle h) noexcept { kernel_->clear_semantic(h); }
        void set_semantic_actions(WidgetHandle h, SemanticActionMask actions) noexcept {
            kernel_->set_semantic_actions(h, actions);
        }
        void set_enabled(WidgetHandle h, bool on) noexcept { kernel_->set_enabled(h, on); }
        void set_focusable(WidgetHandle h, bool on) noexcept { kernel_->set_focusable(h, on); }
        bool pressed(WidgetHandle h) const noexcept { return kernel_->pressed(h); }
        bool focused(WidgetHandle h) const noexcept { return kernel_->focused(h); }
        SemanticFocusSnapshot semantic_snapshot(WidgetHandle h) const noexcept {
            return kernel_->semantic_snapshot(h);
        }
        SemanticActionSnapshot semantic_action_snapshot(WidgetHandle h) const noexcept {
            return kernel_->semantic_action_snapshot(h);
        }
        SemanticIntentResolution resolve_semantic_intent(
            WidgetHandle root,
            const char* id,
            SemanticAction action) const noexcept {
            return kernel_->resolve_semantic_intent(root, id, action);
        }
        SemanticActionAdmission admit_semantic_action(
            WidgetHandle root,
            const char* id,
            SemanticAction action) const noexcept {
            return kernel_->admit_semantic_action(root, id, action);
        }
        SemanticActionRequest request_semantic_action(
            WidgetHandle root,
            const char* id,
            SemanticAction action) noexcept {
            return kernel_->request_semantic_action(root, id, action);
        }
        SemanticFocusQuery query_semantic_focus(WidgetHandle root, const char* id) const noexcept {
            return kernel_->query_semantic_focus(root, id);
        }
        SemanticFocusAdmission admit_semantic_focus(WidgetHandle root, const char* id) const noexcept {
            return kernel_->admit_semantic_focus(root, id);
        }
        SemanticFocusRequest request_semantic_focus(WidgetHandle root, const char* id) noexcept {
            return kernel_->request_semantic_focus(root, id);
        }
        SemanticFocusSnapshot semantic_focus_snapshot() const noexcept {
            return kernel_->semantic_focus_snapshot();
        }
        SemanticTreeSnapshot semantic_tree_snapshot(
            WidgetHandle root,
            std::size_t max_nodes = kSemanticTreeMaxNodes) const noexcept {
            return kernel_->semantic_tree_snapshot(root, max_nodes);
        }

        void set_image(WidgetHandle h, ImageId image) noexcept { kernel_->set_image(h, image); }
        void set_image_slot(const ImageSlotHandles& slot,
                            ImageId image,
                            bool show_plate_when_empty = true) noexcept {
            if (!kernel_) return;
            if (slot.image) kernel_->set_image(slot.image, image);
            if (slot.plate) {
                kernel_->set_visible(slot.plate,
                                     show_plate_when_empty && !ui::gfx::image_id_valid(image));
            }
        }
        void clear_image_slot(const ImageSlotHandles& slot,
                              bool show_plate_when_empty = true) noexcept {
            set_image_slot(slot, invalid_image_id(), show_plate_when_empty);
        }
        void set_image_shape(WidgetHandle h,
                             ImageShapeKind kind,
                             std::uint8_t extent = 0) noexcept {
            kernel_->set_image_shape(h, kind, extent);
        }
        void set_image_rotation_deg(WidgetHandle h, std::int16_t degrees) noexcept {
            kernel_->set_image_rotation_deg(h, degrees);
        }
        void set_button_icon(WidgetHandle h, ImageId icon) noexcept { kernel_->set_button_icon(h, icon); }

        void set_list_view_source(WidgetHandle h,
                                  std::uint16_t count,
                                  const void* ctx,
                                  ListViewTextFn text_fn) noexcept {
            kernel_->set_list_view_source(h, count, ctx, text_fn);
        }
        void set_list_view_subtitle_source(WidgetHandle h,
                                           const void* ctx,
                                           ListViewSubtitleFn subtitle_fn) noexcept {
            kernel_->set_list_view_subtitle_source(h, ctx, subtitle_fn);
        }
        void set_list_view_tail_source(WidgetHandle h,
                                       const void* ctx,
                                       ListViewTailFn tail_fn) noexcept {
            kernel_->set_list_view_tail_source(h, ctx, tail_fn);
        }
        void set_list_view_row_flags_source(WidgetHandle h,
                                            const void* ctx,
                                            ListViewRowFlagsFn row_flags_fn) noexcept {
            kernel_->set_list_view_row_flags_source(h, ctx, row_flags_fn);
        }
        void set_list_view_tail_icon_source(WidgetHandle h,
                                            const void* ctx,
                                            ListViewIconFn icon_fn,
                                            std::uint8_t size) noexcept {
            kernel_->set_list_view_tail_icon_source(h, ctx, icon_fn, size);
        }
        void set_list_view_tail_action_icon_source(WidgetHandle h,
                                                   const void* ctx,
                                                   ListViewIconFn icon_fn,
                                                   std::uint8_t size) noexcept {
            kernel_->set_list_view_tail_action_icon_source(h, ctx, icon_fn, size);
        }
        void set_list_view_icon_corner_radius(WidgetHandle h, std::uint8_t radius) noexcept {
            kernel_->set_list_view_icon_corner_radius(h, radius);
        }
        void set_list_view_icon_source(WidgetHandle h,
                                       const void* ctx,
                                       ListViewIconFn icon_fn,
                                       std::uint8_t size) noexcept {
            kernel_->set_list_view_icon_source(h, ctx, icon_fn, size);
        }
        void set_list_view_selected(WidgetHandle h, int index) noexcept { kernel_->set_list_view_selected(h, index); }
        void set_list_view_active(WidgetHandle h, int index) noexcept { kernel_->set_list_view_active(h, index); }
        int list_view_selected(WidgetHandle h) const noexcept { return kernel_->list_view_selected(h); }
        int list_view_active(WidgetHandle h) const noexcept { return kernel_->list_view_active(h); }
        std::uint8_t list_view_item_row_flags(WidgetHandle h, std::uint16_t index) const noexcept {
            return kernel_->list_view_item_row_flags(h, index);
        }
        int consume_list_view_tail_action(WidgetHandle h) noexcept { return kernel_->consume_list_view_tail_action(h); }

        void set_list_row_height(WidgetHandle h, int height) noexcept { kernel_->set_list_row_height(h, height); }
        void set_scroll_step(WidgetHandle h, int step) noexcept { kernel_->set_scroll_step(h, step); }
        void set_scroll_y(WidgetHandle h, int y) noexcept { kernel_->set_scroll_y(h, y); }
        int scroll_y(WidgetHandle h) const noexcept { return kernel_->scroll_y(h); }
        void set_style_patch(WidgetHandle h, const StylePatch& patch) noexcept { kernel_->set_style_override(h, patch); }
        void set_style_adjust(WidgetHandle h, const StylePatch& patch) noexcept { kernel_->set_style_adjust(h, patch); }
        void set_style_override(WidgetHandle h, const StylePatch& patch) noexcept { kernel_->set_style_override(h, patch); }
        void set_style_token(WidgetHandle h, const StyleToken& token) noexcept { kernel_->set_style_override(h, token.patch); }
        void set_text_color(WidgetHandle h, const rgba& color) noexcept {
            if (!kernel_ || !h) return;
            StylePatch patch{};
            patch.has_font_color = true;
            patch.font_color = color;
            patch.has_font_color_disabled = true;
            patch.font_color_disabled = color;
            kernel_->set_style_override(h, patch);
        }
        void recolor_surface(WidgetHandle h, const SurfaceRecolorSpec& spec) noexcept {
            if (!kernel_ || !h) return;
            kernel_->set_style_override(h, make_surface_recolor_patch(spec));
        }
        void recolor_surface(WidgetHandle h, const rgba& bg, const rgba& border) noexcept {
            recolor_surface(h, {
                .apply_bg_color = true,
                .apply_border_color = true,
                .bg_color = bg,
                .border_color = border,
            });
        }
        void recolor_surface(WidgetHandle h,
                             const rgba& bg,
                             const rgba& border,
                             const rgba& font_color) noexcept {
            recolor_surface(h, {
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_font_color = true,
                .bg_color = bg,
                .border_color = border,
                .font_color = font_color,
            });
        }
        void set_style_class(WidgetHandle h, StyleClassId id) noexcept { kernel_->set_style_class(h, id); }
        void clear_style_class(WidgetHandle h) noexcept { kernel_->clear_style_class(h); }
        void clear_style_patch(WidgetHandle h) noexcept { kernel_->clear_style_patch(h); }

        void set_visible(WidgetHandle h, bool v) noexcept { kernel_->set_visible(h, v); }
        void set_rect(WidgetHandle h, const Rect& r) noexcept { kernel_->set_rect(h, r); }
        void set_value(WidgetHandle h, int value) noexcept { kernel_->set_value(h, value); }
        int value(WidgetHandle h) const noexcept { return kernel_->value(h); }
        void set_checked(WidgetHandle h, bool v) noexcept { kernel_->set_checked(h, v); }
        bool checked(WidgetHandle h) const noexcept { return kernel_->checked(h); }
        void set_focused(WidgetHandle h, bool v) noexcept { kernel_->set_focused(h, v); }
        WidgetHandle input_focused() const noexcept { return kernel_->input_focused(); }
        void set_focus_scope(WidgetHandle scope,
                             WidgetHandle fallback = {},
                             bool trap = true) noexcept {
            kernel_->set_focus_scope(scope, fallback, trap);
        }
        void clear_focus_scope() noexcept { kernel_->clear_focus_scope(); }
        bool push_focus_scope(WidgetHandle scope,
                              WidgetHandle fallback = {},
                              bool trap = true) noexcept {
            return kernel_->push_focus_scope(scope, fallback, trap);
        }
        bool pop_focus_scope() noexcept { return kernel_->pop_focus_scope(); }
        WidgetHandle input_focus_scope() const noexcept { return kernel_->input_focus_scope(); }
        WidgetHandle input_focus_scope_fallback() const noexcept { return kernel_->input_focus_scope_fallback(); }
        bool input_focus_scope_trap() const noexcept { return kernel_->input_focus_scope_trap(); }
        std::size_t input_focus_scope_stack_size() const noexcept { return kernel_->input_focus_scope_stack_size(); }

        std::size_t input_event_count() const noexcept { return kernel_->input_event_count(); }
        const SoaInputEvent& input_event(std::size_t index) const noexcept { return kernel_->input_event(index); }

    private:
        SoaKernel* kernel_{nullptr};
    };

    class SceneBuilder {
    public:
        SceneBuilder(SoaKernel& kernel, SoaFactory& factory) noexcept
            : kernel_(kernel), factory_(factory) {}

        WidgetHandle root() const noexcept { return root_; }
        void set_root(WidgetHandle h) noexcept { root_ = h; }

        WidgetHandle create_container() noexcept { return factory_.create_container(); }
        WidgetHandle create_scroll_container() noexcept { return factory_.create_scroll_container(); }
        WidgetHandle create_image() noexcept { return factory_.create_image(); }
        WidgetHandle create_label(const char* text) noexcept { return factory_.create_label(text); }
        WidgetHandle create_label_static(const char* text) noexcept { return factory_.create_label_static(text); }
        WidgetHandle create_checkbox(const char* text) noexcept { return factory_.create_checkbox(text); }
        WidgetHandle create_radio(const char* text) noexcept { return factory_.create_radio(text); }
        WidgetHandle create_list_item(const char* text) noexcept { return factory_.create_list_item(text); }
        WidgetHandle create_progress() noexcept { return factory_.create_progress(); }
        WidgetHandle create_progress_bar_simple() noexcept { return factory_.create_progress_bar_simple(); }
        WidgetHandle create_progress_bar_round() noexcept { return factory_.create_progress_bar_round(); }
        WidgetHandle create_list_view() noexcept { return factory_.create_list_view(); }
        WidgetHandle create_scrollbar_for(WidgetHandle target) noexcept { return factory_.create_scrollbar_for(target); }
        WidgetHandle create_button_static(const char* text) noexcept { return factory_.create_button_static(text); }
        WidgetHandle create_slider() noexcept { return factory_.create_slider(); }
        WidgetHandle create_switch() noexcept { return factory_.create_switch(); }

        void set_button_icon(WidgetHandle h, ImageId icon) noexcept { factory_.set_button_icon(h, icon); }
        void set_button_icon_size(WidgetHandle h, std::uint8_t size) noexcept {
            factory_.set_button_icon_size(h, size);
        }
        void set_image_shape(WidgetHandle h,
                             ImageShapeKind kind,
                             std::uint8_t extent = 0) noexcept {
            kernel_.set_image_shape(h, kind, extent);
        }
        void set_image_rotation_deg(WidgetHandle h, std::int16_t degrees) noexcept {
            kernel_.set_image_rotation_deg(h, degrees);
        }

        void link(WidgetHandle parent, WidgetHandle child) noexcept { factory_.link(parent, child); }

        void set_rect(WidgetHandle h, const Rect& r) noexcept { kernel_.set_rect(h, r); }
        void set_semantic(WidgetHandle h,
                          SemanticRole role,
                          const char* id,
                          const char* label) noexcept {
            kernel_.set_semantic(h, role, id, label);
        }
        void set_semantic_default(WidgetHandle h,
                                  const char* id,
                                  const char* label = nullptr) noexcept {
            kernel_.set_semantic_default(h, id, label);
        }
        void set_semantic_actions(WidgetHandle h, SemanticActionMask actions) noexcept {
            kernel_.set_semantic_actions(h, actions);
        }
        void set_input_root(WidgetHandle h) noexcept { kernel_.set_input_root(h); }
        void set_focus_scope(WidgetHandle scope,
                             WidgetHandle fallback = {},
                             bool trap = true) noexcept {
            kernel_.set_focus_scope(scope, fallback, trap);
        }
        bool push_focus_scope(WidgetHandle scope,
                              WidgetHandle fallback = {},
                              bool trap = true) noexcept {
            return kernel_.push_focus_scope(scope, fallback, trap);
        }
        void set_clip_children(WidgetHandle h, bool v) noexcept { kernel_.set_clip_children(h, v); }
        void set_scroll_step(WidgetHandle h, int step) noexcept { kernel_.set_scroll_step(h, step); }
        void set_scroll_y(WidgetHandle h, int y) noexcept { kernel_.set_scroll_y(h, y); }
        void set_range(WidgetHandle h, int min, int max) noexcept { kernel_.set_range(h, min, max); }
        void set_value(WidgetHandle h, int value) noexcept { kernel_.set_value(h, value); }
        void set_hit_testable(WidgetHandle h, bool v) noexcept { kernel_.set_hit_testable(h, v); }
        void set_checked(WidgetHandle h, bool v) noexcept { kernel_.set_checked(h, v); }
        void set_list_row_height(WidgetHandle h, int height) noexcept { kernel_.set_list_row_height(h, height); }
        void set_label_align(WidgetHandle h, TextAlignH align_h, TextAlignV align_v) noexcept {
            kernel_.set_text_align(h, align_h, align_v);
        }
        void set_scrollbar_orientation(WidgetHandle h, ScrollBarOrientation o) noexcept {
            kernel_.set_scrollbar_orientation(h, o);
        }
        void set_variant(WidgetHandle h, std::uint8_t variant) noexcept { kernel_.set_variant(h, variant); }
        void set_style_patch(WidgetHandle h, const StylePatch& patch) noexcept { kernel_.set_style_override(h, patch); }
        void set_style_adjust(WidgetHandle h, const StylePatch& patch) noexcept { kernel_.set_style_adjust(h, patch); }
        void set_style_override(WidgetHandle h, const StylePatch& patch) noexcept { kernel_.set_style_override(h, patch); }
        void set_style_token(WidgetHandle h, const StyleToken& token) noexcept { kernel_.set_style_override(h, token.patch); }
        void set_text_color(WidgetHandle h, const rgba& color) noexcept {
            if (!h) return;
            StylePatch patch{};
            patch.has_font_color = true;
            patch.font_color = color;
            kernel_.set_style_override(h, patch);
        }
        void recolor_surface(WidgetHandle h, const SurfaceRecolorSpec& spec) noexcept {
            if (!h) return;
            kernel_.set_style_override(h, make_surface_recolor_patch(spec));
        }
        void recolor_surface(WidgetHandle h, const rgba& bg, const rgba& border) noexcept {
            recolor_surface(h, {
                .apply_bg_color = true,
                .apply_border_color = true,
                .bg_color = bg,
                .border_color = border,
            });
        }
        void recolor_surface(WidgetHandle h,
                             const rgba& bg,
                             const rgba& border,
                             const rgba& font_color) noexcept {
            recolor_surface(h, {
                .apply_bg_color = true,
                .apply_border_color = true,
                .apply_font_color = true,
                .bg_color = bg,
                .border_color = border,
                .font_color = font_color,
            });
        }
        void set_style_class(WidgetHandle h, StyleClassId id) noexcept { kernel_.set_style_class(h, id); }
        void clear_style_class(WidgetHandle h) noexcept { kernel_.clear_style_class(h); }
        void clear_style_patch(WidgetHandle h) noexcept { kernel_.clear_style_patch(h); }

    private:
        SoaKernel& kernel_;
        SoaFactory& factory_;
        WidgetHandle root_{};
    };

    enum class LayoutAxis : std::uint8_t {
        Row,
        Column
    };

    enum class LayoutAlign : std::uint8_t {
        Start,
        Center,
        End,
        Stretch
    };

    class LayoutCursor {
    public:
        LayoutCursor(SceneBuilder& builder,
                     const Rect& rect,
                     LayoutAxis axis,
                     int gap,
                     int padding,
                     LayoutAlign cross = LayoutAlign::Center) noexcept
            : builder_(&builder),
              rect_(rect),
              axis_(axis),
              gap_(gap),
              padding_(padding),
              cross_align_(cross) {}

        LayoutCursor(SceneBuilder& builder,
                     const Rect& rect,
                     LayoutAxis axis,
                     int gap,
                     int padding,
                     int origin_x,
                     int origin_y,
                     LayoutAlign cross = LayoutAlign::Center) noexcept
            : builder_(&builder),
              rect_(rect),
              axis_(axis),
              gap_(gap),
              padding_(padding),
              cross_align_(cross),
              origin_x_(origin_x),
              origin_y_(origin_y),
              use_origin_(true) {}

        Rect content_rect() const noexcept {
            Rect c{
                rect_.x + padding_,
                rect_.y + padding_,
                rect_.w - padding_ * 2,
                rect_.h - padding_ * 2
            };
            if (c.w < 0) c.w = 0;
            if (c.h < 0) c.h = 0;
            return c;
        }

        Rect place_rect(int w, int h) noexcept {
            const Rect c = content_rect();
            if (axis_ == LayoutAxis::Row) {
                int height = h;
                int y = c.y;
                if (cross_align_ == LayoutAlign::Stretch) {
                    height = c.h;
                } else if (cross_align_ == LayoutAlign::Center) {
                    y = c.y + (c.h - h) / 2;
                } else if (cross_align_ == LayoutAlign::End) {
                    y = c.y + c.h - h;
                }
                Rect r{c.x + cursor_, y, w, height};
                if (use_origin_) {
                    r.x += origin_x_;
                    r.y += origin_y_;
                }
                cursor_ += w + gap_;
                return r;
            }
            int width = w;
            int x = c.x;
            if (cross_align_ == LayoutAlign::Stretch) {
                width = c.w;
            } else if (cross_align_ == LayoutAlign::Center) {
                x = c.x + (c.w - w) / 2;
            } else if (cross_align_ == LayoutAlign::End) {
                x = c.x + c.w - w;
            }
            Rect r{x, c.y + cursor_, width, h};
            if (use_origin_) {
                r.x += origin_x_;
                r.y += origin_y_;
            }
            cursor_ += h + gap_;
            return r;
        }

        void place(WidgetHandle h, int w, int hgt) noexcept {
            if (!builder_) return;
            const Rect r = place_rect(w, hgt);
            builder_->set_rect(h, r);
        }

        void skip(int amount) noexcept { cursor_ += amount; }

    private:
        SceneBuilder* builder_{nullptr};
        Rect rect_{};
        LayoutAxis axis_{LayoutAxis::Row};
        int gap_{0};
        int padding_{0};
        LayoutAlign cross_align_{LayoutAlign::Center};
        int cursor_{0};
        int origin_x_{0};
        int origin_y_{0};
        bool use_origin_{false};
    };

    inline LayoutCursor make_row(SceneBuilder& builder,
                                 const Rect& rect,
                                 int gap = 0,
                                 int padding = 0,
                                 LayoutAlign cross = LayoutAlign::Center) noexcept {
        return LayoutCursor(builder, rect, LayoutAxis::Row, gap, padding, cross);
    }

    inline LayoutCursor make_column(SceneBuilder& builder,
                                    const Rect& rect,
                                    int gap = 0,
                                    int padding = 0,
                                    LayoutAlign cross = LayoutAlign::Center) noexcept {
        return LayoutCursor(builder, rect, LayoutAxis::Column, gap, padding, cross);
    }

    inline LayoutCursor make_row_local(SceneBuilder& builder,
                                       const Rect& rect,
                                       int origin_x,
                                       int origin_y,
                                       int gap = 0,
                                       int padding = 0,
                                       LayoutAlign cross = LayoutAlign::Center) noexcept {
        return LayoutCursor(builder, rect, LayoutAxis::Row, gap, padding, origin_x, origin_y, cross);
    }

    inline LayoutCursor make_column_local(SceneBuilder& builder,
                                          const Rect& rect,
                                          int origin_x,
                                          int origin_y,
                                          int gap = 0,
                                          int padding = 0,
                                          LayoutAlign cross = LayoutAlign::Center) noexcept {
        return LayoutCursor(builder, rect, LayoutAxis::Column, gap, padding, origin_x, origin_y, cross);
    }

    class RowBuilder {
    public:
        RowBuilder(SceneBuilder& builder,
                   const Rect& rect,
                   int gap = 0,
                   int padding = 0,
                   LayoutAlign cross = LayoutAlign::Center,
                   StyleClassId style_class = kStyleClassInvalid,
                   bool clip = false) noexcept
            : builder_(&builder),
              root_(builder.create_container()),
              cursor_(builder, Rect{0, 0, rect.w, rect.h}, LayoutAxis::Row, gap, padding, cross) {
            builder_->set_rect(root_, rect);
            if (style_class != kStyleClassInvalid) {
                builder_->set_style_class(root_, style_class);
            }
            if (clip) builder_->set_clip_children(root_, true);
        }

        WidgetHandle root() const noexcept { return root_; }

        Rect content_rect() const noexcept { return cursor_.content_rect(); }

        Rect next_rect(int w, int h) noexcept { return cursor_.place_rect(w, h); }

        void add(WidgetHandle child, int w, int h) noexcept {
            if (!builder_ || !root_ || !child) return;
            builder_->link(root_, child);
            cursor_.place(child, w, h);
        }

        void add_at(WidgetHandle child, const Rect& r) noexcept {
            if (!builder_ || !root_ || !child) return;
            builder_->link(root_, child);
            builder_->set_rect(child, r);
        }

    private:
        SceneBuilder* builder_{nullptr};
        WidgetHandle root_{};
        LayoutCursor cursor_;
    };

    class ColumnBuilder {
    public:
        ColumnBuilder(SceneBuilder& builder,
                      const Rect& rect,
                      int gap = 0,
                      int padding = 0,
                      LayoutAlign cross = LayoutAlign::Center,
                      StyleClassId style_class = kStyleClassInvalid,
                      bool clip = false) noexcept
            : builder_(&builder),
              root_(builder.create_container()),
              cursor_(builder, Rect{0, 0, rect.w, rect.h}, LayoutAxis::Column, gap, padding, cross) {
            builder_->set_rect(root_, rect);
            if (style_class != kStyleClassInvalid) {
                builder_->set_style_class(root_, style_class);
            }
            if (clip) builder_->set_clip_children(root_, true);
        }

        WidgetHandle root() const noexcept { return root_; }

        Rect content_rect() const noexcept { return cursor_.content_rect(); }

        Rect next_rect(int w, int h) noexcept { return cursor_.place_rect(w, h); }

        void add(WidgetHandle child, int w, int h) noexcept {
            if (!builder_ || !root_ || !child) return;
            builder_->link(root_, child);
            cursor_.place(child, w, h);
        }

        void add_at(WidgetHandle child, const Rect& r) noexcept {
            if (!builder_ || !root_ || !child) return;
            builder_->link(root_, child);
            builder_->set_rect(child, r);
        }

    private:
        SceneBuilder* builder_{nullptr};
        WidgetHandle root_{};
        LayoutCursor cursor_;
    };

    class CardBuilder {
    public:
        CardBuilder(SceneBuilder& builder,
                    const Rect& rect,
                    int gap = 0,
                    int padding = 0,
                    LayoutAlign cross = LayoutAlign::Center,
                    StyleClassId style_class = kStyleClassInvalid,
                    bool clip = true) noexcept
            : builder_(&builder),
              root_(builder.create_container()),
              cursor_(builder, Rect{0, 0, rect.w, rect.h}, LayoutAxis::Column, gap, padding, cross) {
            builder_->set_rect(root_, rect);
            if (style_class != kStyleClassInvalid) {
                builder_->set_style_class(root_, style_class);
            }
            if (clip) builder_->set_clip_children(root_, true);
        }

        WidgetHandle root() const noexcept { return root_; }

        Rect content_rect() const noexcept { return cursor_.content_rect(); }

        Rect next_rect(int w, int h) noexcept { return cursor_.place_rect(w, h); }

        void add(WidgetHandle child, int w, int h) noexcept {
            if (!builder_ || !root_ || !child) return;
            builder_->link(root_, child);
            cursor_.place(child, w, h);
        }

        void add_at(WidgetHandle child, const Rect& r) noexcept {
            if (!builder_ || !root_ || !child) return;
            builder_->link(root_, child);
            builder_->set_rect(child, r);
        }

    private:
        SceneBuilder* builder_{nullptr};
        WidgetHandle root_{};
        LayoutCursor cursor_;
    };

    inline void configure_image_surface(SceneBuilder& builder,
                                        WidgetHandle image,
                                        int corner_radius,
                                        ImageShapeKind image_shape = ImageShapeKind::RoundRect,
                                        std::uint8_t shape_extent = 0,
                                        std::int16_t rotation_deg = 0,
                                        bool hit_testable = false) noexcept {
        if (!image) return;
        const auto clamp_u8 = [](int value) noexcept -> std::uint8_t {
            if (value <= 0) return 0;
            if (value >= 255) return 255;
            return static_cast<std::uint8_t>(value);
        };
        StylePatch patch{};
        patch.has_corner_radius = true;
        patch.corner_radius = corner_radius;
        builder.set_style_override(image, patch);
        builder.set_image_shape(image,
                                image_shape,
                                (shape_extent != 0) ? shape_extent : clamp_u8(corner_radius));
        builder.set_image_rotation_deg(image, rotation_deg);
        builder.set_hit_testable(image, hit_testable);
    }

    template<typename PlateStyleFn>
    inline ImageSlotHandles create_image_slot(SceneBuilder& builder,
                                              WidgetHandle parent,
                                              const ImageSlotSpec& spec,
                                              PlateStyleFn apply_plate_style) noexcept {
        const auto expand_rect = [](const Rect& rect, int pad) noexcept -> Rect {
            return {rect.x - pad, rect.y - pad, rect.w + pad * 2, rect.h + pad * 2};
        };

        ImageSlotHandles handles{};
        if (spec.create_plate) {
            handles.plate = builder.create_container();
            builder.set_rect(handles.plate, expand_rect(spec.image_rect, spec.plate_pad));
            apply_plate_style(builder, handles.plate);
            builder.set_hit_testable(handles.plate, spec.plate_hit_testable);
            builder.link(parent, handles.plate);
        }

        handles.image = builder.create_image();
        builder.set_rect(handles.image, spec.image_rect);
        configure_image_surface(builder,
                                handles.image,
                                spec.image_corner_radius,
                                spec.image_shape,
                                spec.shape_extent,
                                spec.rotation_deg,
                                spec.image_hit_testable);
        builder.link(parent, handles.image);
        return handles;
    }

    class TileBuilder {
    public:
        TileBuilder(SceneBuilder& builder,
                    const Rect& rect,
                    int image_size,
                    int gap = 0,
                    bool image_top = true,
                    StyleClassId style_class = kStyleClassInvalid,
                    bool clip = false,
                    const char* label_text = "") noexcept
            : builder_(&builder),
              root_(builder.create_container()),
              image_(builder.create_image()),
              label_(builder.create_label_static(label_text)),
              cursor_(builder, Rect{0, 0, rect.w, rect.h},
                      image_top ? LayoutAxis::Column : LayoutAxis::Row,
                      gap, 0, LayoutAlign::Center) {
            builder_->set_rect(root_, rect);
            if (style_class != kStyleClassInvalid) {
                builder_->set_style_class(root_, style_class);
            }
            if (clip) builder_->set_clip_children(root_, true);
            builder_->link(root_, image_);
            builder_->link(root_, label_);
            if (image_top) {
                cursor_.place(image_, image_size, image_size);
                cursor_.place(label_, rect.w, rect.h - image_size - gap);
            } else {
                cursor_.place(image_, image_size, image_size);
                cursor_.place(label_, rect.w - image_size - gap, rect.h);
            }
        }

        WidgetHandle root() const noexcept { return root_; }
        WidgetHandle image() const noexcept { return image_; }
        WidgetHandle label() const noexcept { return label_; }

    private:
        SceneBuilder* builder_{nullptr};
        WidgetHandle root_{};
        WidgetHandle image_{};
        WidgetHandle label_{};
        LayoutCursor cursor_;
    };

    struct PageHooks {
        void (*on_show)(SceneAccess&, WidgetHandle, void*) noexcept {nullptr};
        void (*on_hide)(SceneAccess&, WidgetHandle, void*) noexcept {nullptr};
        void* ctx{nullptr};
    };
}
