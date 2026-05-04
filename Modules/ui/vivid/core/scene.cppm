module;

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

export module charm.ui.scene;

export import charm.core.event;
export import charm.core.geometry;
export import charm.core.handle;
import charm.core.structured_view;
export import charm.core.style;
import charm.core.style_sheet;
export import charm.ui.scene.pill_surface;
export import charm.ui.scene.layer_runtime;
import charm.core.soa_factory;
import charm.core.soa_gui;
import charm.core.soa_kernel;
import charm.core.soa_payload;
export import charm.gfx.canvas;
export import charm.gfx.color;
export import charm.gfx.image;
import charm.gfx.pixel_ops;
export import charm.gfx.render_style;
export import charm.gfx.text_box;
import charm.gfx.draw_cmd;

export using ::ScrollBarOrientation;
export using ::SemanticAction;
export using ::SemanticActionMask;
export using ::SemanticActionSnapshot;
export using ::SemanticFocusAdmission;
export using ::SemanticFocusAdmissionStatus;
export using ::SemanticFocusQuery;
export using ::SemanticFocusQueryStatus;
export using ::SemanticFocusRequest;
export using ::SemanticFocusRequestStatus;
export using ::SemanticFocusSnapshot;
export using ::SemanticIntentResolution;
export using ::SemanticIntentStatus;
export using ::SemanticRole;
export using ::SemanticTreeNode;
export using ::SemanticTreeSnapshot;
export using ::semantic_action_mask;
export using ::semantic_action_present;
export using ::semantic_default_role_for_kind;
export using ::semantic_default_actions_for_role;
export using ::semantic_focus_admission_status_name;
export using ::semantic_focus_query_status_name;
export using ::semantic_focus_request_status_name;
export using ::semantic_intent_status_name;
export using ::kSemanticTreeMaxNodes;
export using ::kSemanticTreeNoFocusIndex;
export using ::TableViewHeaderStyle;
export using ::TableViewColDividerStyle;

export namespace ui::scene {
    using TextSlotId = std::uint16_t;
    constexpr TextSlotId kInvalidTextSlot = 0xFFFF;

    using ImageId = ui::gfx::ImageId;
    constexpr ImageId invalid_image_id() noexcept { return ui::gfx::invalid_image_id(); }
    constexpr bool image_id_valid(ImageId id) noexcept { return ui::gfx::image_id_valid(id); }
    using ImageShapeKind = ui::render::ImageShapeKind;

    using ScrollBarOrientation = ::ScrollBarOrientation;
    using SemanticAction = ::SemanticAction;
    using SemanticActionMask = ::SemanticActionMask;
    using SemanticActionSnapshot = ::SemanticActionSnapshot;
    using SemanticFocusAdmission = ::SemanticFocusAdmission;
    using SemanticFocusAdmissionStatus = ::SemanticFocusAdmissionStatus;
    using SemanticFocusQuery = ::SemanticFocusQuery;
    using SemanticFocusQueryStatus = ::SemanticFocusQueryStatus;
    using SemanticFocusRequest = ::SemanticFocusRequest;
    using SemanticFocusRequestStatus = ::SemanticFocusRequestStatus;
    using SemanticFocusSnapshot = ::SemanticFocusSnapshot;
    using SemanticIntentResolution = ::SemanticIntentResolution;
    using SemanticIntentStatus = ::SemanticIntentStatus;
    using SemanticRole = ::SemanticRole;
    using SemanticTreeNode = ::SemanticTreeNode;
    using SemanticTreeSnapshot = ::SemanticTreeSnapshot;
    using ::semantic_action_mask;
    using ::semantic_action_present;
    using ::semantic_default_actions_for_role;
    using ::semantic_focus_admission_status_name;
    using ::semantic_focus_query_status_name;
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

    struct CmdStats {
        std::size_t cmd_count{0};
        std::size_t cmd_capacity{0};
        std::size_t cmd_bytes{0};
        std::size_t text_used{0};
        std::size_t text_capacity{0};
        std::size_t blob_used{0};
        std::size_t blob_capacity{0};
        std::size_t batch_shrink{0};
        std::size_t batch_shrink_line{0};
        std::size_t batch_shrink_path{0};
        std::size_t batch_shrink_rect{0};
        std::size_t batch_shrink_round{0};
        std::size_t batch_shrink_image{0};
        std::size_t batch_shrink_focus{0};
        bool cmd_overflowed{false};
        bool text_overflowed{false};
        bool blob_overflowed{false};
    };

    struct ExecStats {
        std::size_t cmd_count{0};
        std::size_t cmd_bytes{0};
        std::uint64_t alpha_blend_count{0};
        std::size_t clip_pushes{0};
        std::size_t clip_pops{0};
        std::size_t clip_push_overflow{0};
        std::size_t clip_pop_underflow{0};
        std::size_t clip_invalid{0};
        std::size_t failed_cmds{0};
        std::size_t fail_text{0};
        std::size_t fail_image{0};
        std::size_t fail_blob{0};
        std::size_t fail_path{0};
        std::size_t fail_clip{0};
        std::size_t fail_other{0};
        std::size_t dispatch_groups{0};
        std::size_t batch_flushes{0};
        std::size_t group_rect{0};
        std::size_t group_text{0};
        std::size_t group_image{0};
        std::size_t group_line{0};
        std::size_t group_path{0};
        std::size_t group_other{0};
        std::size_t cmd_rect{0};
        std::size_t cmd_text{0};
        std::size_t cmd_image{0};
        std::size_t cmd_line{0};
        std::size_t cmd_path{0};
        std::size_t cmd_other{0};
        bool overflowed{false};
    };

    struct LayerReplayResult {
        LayerReplayStatus status{LayerReplayStatus::InvalidPlan};
        SnapshotHandle source{};
        SnapshotKind kind{SnapshotKind::EmptyFallback};
        Rect target_bounds{};
        ExecStats stats{};

        [[nodiscard]] constexpr bool ok() const noexcept {
            return status == LayerReplayStatus::Ok;
        }
    };

    struct LayerCaptureResult {
        LayerCaptureStatus status{LayerCaptureStatus::NoSnapshotSlot};
        SnapshotHandle handle{};

        [[nodiscard]] constexpr bool ok() const noexcept {
            return status == LayerCaptureStatus::Ok;
        }
    };

    template<std::size_t MaxSlots>
    class PixelSnapshotPayloadStore {
    public:
        [[nodiscard]] std::uint32_t store(CanvasBase& source, Rect bounds) noexcept {
            if (bounds.w <= 0 || bounds.h <= 0) return kInvalidSnapshotPayloadSlot;
            if (bounds.w > layer_cache_width || bounds.h > layer_cache_height) {
                return kInvalidSnapshotPayloadSlot;
            }
            if (bounds.x < 0 || bounds.y < 0 ||
                bounds.x + bounds.w > source.width() ||
                bounds.y + bounds.h > source.height()) {
                return kInvalidSnapshotPayloadSlot;
            }
            if (source.bytes_per_pixel() != ui::draw_cmd::bytes_per_pixel(screen_pixel_format)) {
                return kInvalidSnapshotPayloadSlot;
            }
            for (std::size_t i = 0; i < occupied_.size(); ++i) {
                if (occupied_[i]) continue;
                if (!copy_from_canvas(buffers_[i], source, bounds)) {
                    return kInvalidSnapshotPayloadSlot;
                }
                occupied_[i] = true;
                widths_[i] = bounds.w;
                heights_[i] = bounds.h;
                return static_cast<std::uint32_t>(i);
            }
            return kInvalidSnapshotPayloadSlot;
        }

        [[nodiscard]] bool release(std::uint32_t slot) noexcept {
            if (slot >= occupied_.size() || !occupied_[slot]) return false;
            occupied_[slot] = false;
            widths_[slot] = 0;
            heights_[slot] = 0;
            return true;
        }

        [[nodiscard]] const std::byte* row(std::uint32_t slot, int y) const noexcept {
            if (slot >= occupied_.size() || !occupied_[slot]) return nullptr;
            if (y < 0 || y >= heights_[slot]) return nullptr;
            return buffers_[slot].data() + static_cast<std::size_t>(y) * stride_bytes;
        }

        [[nodiscard]] int width(std::uint32_t slot) const noexcept {
            return (slot < widths_.size() && occupied_[slot]) ? widths_[slot] : 0;
        }

        [[nodiscard]] int height(std::uint32_t slot) const noexcept {
            return (slot < heights_.size() && occupied_[slot]) ? heights_[slot] : 0;
        }

        static constexpr std::size_t stride_bytes =
            static_cast<std::size_t>(layer_cache_width)
            * ui::draw_cmd::bytes_per_pixel(screen_pixel_format);

    private:
        static constexpr std::size_t pixel_bytes =
            static_cast<std::size_t>(layer_cache_width)
            * static_cast<std::size_t>(layer_cache_height)
            * ui::draw_cmd::bytes_per_pixel(screen_pixel_format);

        static bool copy_from_canvas(std::array<std::byte, pixel_bytes>& target,
                                     CanvasBase& source,
                                     Rect bounds) noexcept {
            const auto row_bytes = static_cast<std::size_t>(bounds.w) * source.bytes_per_pixel();
            for (int y = 0; y < bounds.h; ++y) {
                const auto* src = source.row_ptr(bounds.y + y);
                if (!src) return false;
                const auto src_offset = static_cast<std::size_t>(bounds.x) * source.bytes_per_pixel();
                auto* dst = target.data() + static_cast<std::size_t>(y) * stride_bytes;
                std::memcpy(dst, src + src_offset, row_bytes);
            }
            return true;
        }

        std::array<std::array<std::byte, pixel_bytes>, MaxSlots> buffers_{};
        std::array<bool, MaxSlots> occupied_{};
        std::array<int, MaxSlots> widths_{};
        std::array<int, MaxSlots> heights_{};
    };

    template<std::size_t MaxSlots>
    class CommandSnapshotPayloadStore {
    public:
        using Buffer = ui::draw_cmd::DefaultDrawCmdBuffer;

        [[nodiscard]] std::uint32_t store(const Buffer& source) noexcept {
            for (std::size_t i = 0; i < buffers_.size(); ++i) {
                if (occupied_[i]) continue;
                if (!copy_into(buffers_[i], source)) return kInvalidSnapshotPayloadSlot;
                occupied_[i] = true;
                return static_cast<std::uint32_t>(i);
            }
            return kInvalidSnapshotPayloadSlot;
        }

        [[nodiscard]] bool release(std::uint32_t slot) noexcept {
            if (slot >= buffers_.size() || !occupied_[slot]) return false;
            buffers_[slot].clear();
            occupied_[slot] = false;
            return true;
        }

        [[nodiscard]] const Buffer* get(std::uint32_t slot) const noexcept {
            if (slot >= buffers_.size() || !occupied_[slot]) return nullptr;
            return &buffers_[slot];
        }

        void clear() noexcept {
            for (std::size_t i = 0; i < buffers_.size(); ++i) {
                buffers_[i].clear();
                occupied_[i] = false;
            }
        }

    private:
        static bool copy_into(Buffer& target, const Buffer& source) noexcept {
            return target.load(source.cmd_data(),
                               source.cmd_bytes(),
                               source.size(),
                               source.text_data(),
                               source.text_used(),
                               source.blob_data(),
                               source.blob_used());
        }

        std::array<Buffer, MaxSlots> buffers_{};
        std::array<bool, MaxSlots> occupied_{};
    };

    struct TileStats {
        int tiles_total{0};
        int tiles_drawn{0};
        std::size_t cmd_count{0};
        std::size_t cmd_bytes{0};
        std::uint64_t alpha_blend_count{0};
        int tile_flush_count{0};
        std::size_t clip_push_overflow{0};
        std::size_t clip_pop_underflow{0};
        std::size_t clip_invalid{0};
        std::size_t dispatch_groups{0};
        std::size_t batch_flushes{0};
        std::size_t failed_cmds{0};
        std::size_t fail_text{0};
        std::size_t fail_image{0};
        std::size_t fail_blob{0};
        std::size_t fail_path{0};
        std::size_t fail_clip{0};
        std::size_t fail_other{0};
        std::size_t group_rect{0};
        std::size_t group_text{0};
        std::size_t group_image{0};
        std::size_t group_line{0};
        std::size_t group_path{0};
        std::size_t group_other{0};
        std::size_t cmd_rect{0};
        std::size_t cmd_text{0};
        std::size_t cmd_image{0};
        std::size_t cmd_line{0};
        std::size_t cmd_path{0};
        std::size_t cmd_other{0};
    };

    struct TileConfig {
        int tile_width{64};
        int tile_height{64};
        rgba clear_color{0, 0, 0, 0};
        bool clear_tile{true};
    };

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
        SceneAccess() noexcept = default;
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

    class SceneOverlay {
    public:
        explicit SceneOverlay(ui::draw_cmd::DefaultDrawCmdBuffer& buf) noexcept : buf_(buf) {}

        void fill_rect(const Rect& rect, const rgba& color) noexcept { buf_.fill_rect(rect, color); }
        void stroke_rect(const Rect& rect, const rgba& color) noexcept { buf_.stroke_rect(rect, color); }
        void fill_round_rect(const Rect& rect, int radius, const rgba& color) noexcept {
            buf_.fill_round_rect(rect, radius, color);
        }
        void stroke_round_rect(const Rect& rect, int radius, const rgba& color) noexcept {
            buf_.stroke_round_rect(rect, radius, color);
        }
        void fill_circle(const Rect& rect, const rgba& color) noexcept {
            const int radius = std::max(0, std::min(rect.w, rect.h) / 2);
            buf_.fill_circle(rect.x + rect.w / 2, rect.y + rect.h / 2, radius, color);
        }
        void stroke_circle(const Rect& rect, const rgba& color) noexcept {
            const int radius = std::max(0, std::min(rect.w, rect.h) / 2);
            buf_.stroke_circle(rect.x + rect.w / 2, rect.y + rect.h / 2, radius, color);
        }
    private:
        ui::draw_cmd::DefaultDrawCmdBuffer& buf_;
    };

    class Scene {
    public:
        using OverlayFn = void(*)(SceneOverlay& overlay, void* ctx) noexcept;

        explicit Scene(CanvasBase& canvas) noexcept
            : canvas_(canvas),
              factory_(kernel_),
              gui_(canvas_, kernel_, {}) {}

        SceneBuilder begin() noexcept { return SceneBuilder(kernel_, factory_); }
        void end(const SceneBuilder& builder) noexcept { set_root(builder.root()); }
        template <typename Fn>
        void build(Fn&& fn) noexcept {
            SceneBuilder builder{kernel_, factory_};
            fn(builder);
            set_root(builder.root());
        }

        void set_root(WidgetHandle root) noexcept {
            root_ = root;
            gui_.set_root(root_);
        }
        WidgetHandle root() const noexcept { return root_; }

        void set_overlay(OverlayFn fn, void* ctx) noexcept {
            overlay_fn_ = fn;
            overlay_ctx_ = ctx;
        }

        void render() {
            record_current_scene();
            reset_alpha_blend_count();
            last_exec_stats_ = to_scene_stats(cmd_exec_.execute(canvas_, cmd_buf_));
            last_exec_stats_.alpha_blend_count = alpha_blend_count();
        }

        template <ui::RenderBackend Backend>
        TileStats render_tiles(Backend& backend,
                               const FrameBufferView& tile_buffer,
                               const TileConfig& config) {
            record_current_scene();
            const ui::draw_cmd::DrawCmdTileConfig cfg{
                config.tile_width,
                config.tile_height,
                config.clear_color,
                config.clear_tile
            };
            reset_alpha_blend_count();
            auto stats = to_scene_stats(cmd_exec_.execute_tiles(backend, tile_buffer, cmd_buf_, cfg));
            stats.alpha_blend_count = alpha_blend_count();
            return stats;
        }

        void dispatch_event(const Event& e) { gui_.dispatch_event(e); }
        WidgetHandle hit_test(int x, int y) noexcept { return gui_.hit_test(x, y); }

        Rect world_rect(WidgetHandle h) const noexcept { return kernel_.world_rect(h); }
        const char* text(WidgetHandle h) const noexcept { return kernel_.text(h); }
        SemanticFocusSnapshot semantic_snapshot(WidgetHandle h) const noexcept {
            return kernel_.semantic_snapshot(h);
        }
        SemanticActionSnapshot semantic_action_snapshot(WidgetHandle h) const noexcept {
            return kernel_.semantic_action_snapshot(h);
        }
        SemanticIntentResolution resolve_semantic_intent(
            WidgetHandle root,
            const char* id,
            SemanticAction action) const noexcept {
            return kernel_.resolve_semantic_intent(root, id, action);
        }
        SemanticFocusQuery query_semantic_focus(WidgetHandle root, const char* id) const noexcept {
            return kernel_.query_semantic_focus(root, id);
        }
        SemanticFocusAdmission admit_semantic_focus(WidgetHandle root, const char* id) const noexcept {
            return kernel_.admit_semantic_focus(root, id);
        }
        SemanticFocusRequest request_semantic_focus(WidgetHandle root, const char* id) noexcept {
            return kernel_.request_semantic_focus(root, id);
        }
        SemanticFocusSnapshot semantic_focus_snapshot() const noexcept {
            return kernel_.semantic_focus_snapshot();
        }
        SemanticTreeSnapshot semantic_tree_snapshot(
            WidgetHandle root,
            std::size_t max_nodes = kSemanticTreeMaxNodes) const noexcept {
            return kernel_.semantic_tree_snapshot(root, max_nodes);
        }
        SceneAccess access() noexcept { return SceneAccess(kernel_); }

        CmdStats last_cmd_stats() const noexcept { return last_cmd_stats_; }
        ExecStats last_exec_stats() const noexcept { return last_exec_stats_; }
        LayerStats layer_stats() const noexcept { return snapshot_store_.stats(); }
        LayerEpoch current_layer_epoch() const noexcept {
            return make_layer_epoch();
        }
        SnapshotHandle reserve_snapshot(const SnapshotSpec& spec) noexcept {
            return snapshot_store_.reserve(spec, make_layer_epoch());
        }
        bool release_snapshot(SnapshotHandle handle) noexcept {
            if (const auto* record = snapshot_store_.record(handle);
                record && record->payload_slot != kInvalidSnapshotPayloadSlot) {
                if (record->kind == SnapshotKind::CommandBuffer) {
                    (void)command_snapshot_payloads_.release(record->payload_slot);
                } else if (record->kind == SnapshotKind::PixelSurface) {
                    (void)pixel_snapshot_payloads_.release(record->payload_slot);
                }
            }
            return snapshot_store_.release(handle);
        }
        bool mark_snapshot_stale(SnapshotHandle handle) noexcept {
            return snapshot_store_.mark_stale(handle);
        }
        bool refresh_snapshot_epoch(SnapshotHandle handle) noexcept {
            return snapshot_store_.refresh_epoch(handle, make_layer_epoch());
        }
        const SnapshotRecord* snapshot_record(SnapshotHandle handle) const noexcept {
            return snapshot_store_.record(handle);
        }
        bool snapshot_current(SnapshotHandle handle) const noexcept {
            const auto* record = snapshot_store_.record(handle);
            return record && !record->stale && record->epoch == make_layer_epoch();
        }
        bool validate_snapshot(SnapshotHandle handle) noexcept {
            if (snapshot_current(handle)) return true;
            if (snapshot_store_.record(handle)) {
                (void)snapshot_store_.mark_stale(handle);
            }
            return false;
        }
        bool validate_snapshot_for_compose(SnapshotHandle handle) noexcept {
            const auto* record = snapshot_store_.record(handle);
            if (!record || record->stale) return false;
            if (record->kind == SnapshotKind::PixelSurface) return true;
            return validate_snapshot(handle);
        }
        bool update_command_snapshot(SnapshotHandle handle) noexcept {
            return snapshot_store_.update_command_snapshot(
                handle,
                static_cast<std::uint32_t>(last_cmd_stats_.cmd_count),
                static_cast<std::uint32_t>(last_cmd_stats_.cmd_bytes));
        }
        LayerCaptureResult capture_command_snapshot_result(const SnapshotSpec& spec) noexcept {
            LayerCaptureResult result{};
            SnapshotSpec command_spec = spec;
            command_spec.preferred_kind = SnapshotKind::CommandBuffer;
            const auto handle = reserve_snapshot(command_spec);
            if (!handle) return result;
            result.handle = handle;
            record_current_scene();
            if (!update_command_snapshot(handle)) {
                (void)release_snapshot(handle);
                result.handle = {};
                result.status = LayerCaptureStatus::RecordFailed;
                return result;
            }
            if (!store_command_snapshot_payload(handle)) {
                (void)release_snapshot(handle);
                result.handle = {};
                result.status = LayerCaptureStatus::StoreFailed;
                return result;
            }
            result.status = LayerCaptureStatus::Ok;
            return result;
        }
        SnapshotHandle capture_command_snapshot(const SnapshotSpec& spec) noexcept {
            return capture_command_snapshot_result(spec).handle;
        }
        LayerCaptureResult capture_pixel_snapshot_result(const SnapshotSpec& spec) noexcept {
            LayerCaptureResult result{};
            SnapshotSpec pixel_spec = spec;
            pixel_spec.preferred_kind = SnapshotKind::PixelSurface;
            pixel_spec.preferred_format = screen_pixel_format;
            const auto handle = reserve_snapshot(pixel_spec);
            if (!handle) return result;
            result.handle = handle;
            const auto payload_slot = pixel_snapshot_payloads_.store(canvas_, pixel_spec.bounds);
            if (payload_slot == kInvalidSnapshotPayloadSlot) {
                (void)release_snapshot(handle);
                result.handle = {};
                result.status = LayerCaptureStatus::StoreFailed;
                return result;
            }
            const auto bytes = static_cast<std::uint32_t>(
                snapshot_pixel_bytes(screen_pixel_format, pixel_spec.bounds.w, pixel_spec.bounds.h));
            if (!snapshot_store_.update_pixel_snapshot(handle, screen_pixel_format, bytes)) {
                (void)release_snapshot(handle);
                result.handle = {};
                result.status = LayerCaptureStatus::RecordFailed;
                return result;
            }
            if (auto* record = snapshot_store_.mutable_record(handle)) {
                record->payload_slot = payload_slot;
            }
            result.status = LayerCaptureStatus::Ok;
            return result;
        }
        SnapshotHandle capture_pixel_snapshot(const SnapshotSpec& spec) noexcept {
            return capture_pixel_snapshot_result(spec).handle;
        }
        bool update_pixel_snapshot(SnapshotHandle handle,
                                   PixelFormat format,
                                   std::uint32_t bytes) noexcept {
            return snapshot_store_.update_pixel_snapshot(handle, format, bytes);
        }
        LayerComposeResult compose_snapshot_dry_run(const LayerComposeSpec& spec) noexcept {
            if (!validate_snapshot_for_compose(spec.source)) {
                LayerComposeResult result{};
                if (const auto* record = snapshot_store_.record(spec.source)) {
                    result.stale = record->stale;
                    result.kind = record->kind;
                    result.source_bounds = record->bounds;
                    result.source_bytes = record->bytes;
                }
                return result;
            }
            return snapshot_store_.compose_dry_run(spec);
        }
        LayerComposePlan make_snapshot_compose_plan(const LayerComposeSpec& spec) noexcept {
            if (!validate_snapshot_for_compose(spec.source)) return {};
            return snapshot_store_.make_compose_plan(spec);
        }
        LayerBudgetResult check_layer_budget(const LayerComposePlan& plan,
                                             const LayerBudget& budget) const noexcept {
            return snapshot_store_.check_budget(plan, budget);
        }
        LayerReplayResult replay_command_snapshot(const LayerComposePlan& plan) noexcept {
            LayerReplayResult out{};
            out.source = plan.source;
            out.kind = plan.kind;
            out.target_bounds = plan.target_bounds;
            if (!plan.valid) return out;
            if (plan.kind != SnapshotKind::CommandBuffer) {
                out.status = LayerReplayStatus::UnsupportedKind;
                return out;
            }
            const auto* record = snapshot_store_.record(plan.source);
            if (!record) {
                out.status = LayerReplayStatus::MissingSnapshot;
                return out;
            }
            if (record->stale || record->epoch != make_layer_epoch()) {
                (void)snapshot_store_.mark_stale(plan.source);
                out.status = LayerReplayStatus::StaleSnapshot;
                return out;
            }
            const auto* payload = command_snapshot_payloads_.get(record->payload_slot);
            if (!payload) {
                out.status = LayerReplayStatus::MissingPayload;
                return out;
            }
            reset_alpha_blend_count();
            out.stats = to_scene_stats(cmd_exec_.execute(canvas_,
                                                         *payload,
                                                         &plan.target_bounds));
            out.stats.alpha_blend_count = alpha_blend_count();
            out.status = out.stats.failed_cmds == 0
                ? LayerReplayStatus::Ok
                : LayerReplayStatus::ExecuteFailed;
            return out;
        }
        LayerReplayResult compose_pixel_snapshot(const LayerComposePlan& plan) noexcept {
            LayerReplayResult out{};
            out.source = plan.source;
            out.kind = plan.kind;
            out.target_bounds = plan.target_bounds;
            if (!plan.valid) return out;
            if (plan.kind != SnapshotKind::PixelSurface) {
                out.status = LayerReplayStatus::UnsupportedKind;
                return out;
            }
            const auto* record = snapshot_store_.record(plan.source);
            if (!record) {
                out.status = LayerReplayStatus::MissingSnapshot;
                return out;
            }
            if (record->stale) {
                (void)snapshot_store_.mark_stale(plan.source);
                out.status = LayerReplayStatus::StaleSnapshot;
                return out;
            }
            if (record->payload_slot == kInvalidSnapshotPayloadSlot
                || pixel_snapshot_payloads_.width(record->payload_slot) <= 0
                || pixel_snapshot_payloads_.height(record->payload_slot) <= 0) {
                out.status = LayerReplayStatus::MissingPayload;
                return out;
            }
            if (plan.target_bounds.x < 0 || plan.target_bounds.y < 0 ||
                plan.target_bounds.x + plan.target_bounds.w > canvas_.width() ||
                plan.target_bounds.y + plan.target_bounds.h > canvas_.height()) {
                out.status = LayerReplayStatus::ExecuteFailed;
                return out;
            }
            if (plan.transform.opacity == 0) {
                out.stats.cmd_count = 0;
                out.stats.cmd_bytes = 0;
                out.status = LayerReplayStatus::Ok;
                return out;
            }
            std::uint64_t alpha_blend_pixels = 0;
            const auto row_bytes =
                static_cast<std::size_t>(plan.source_visible.w) * canvas_.bytes_per_pixel();
            for (int y = 0; y < plan.source_visible.h; ++y) {
                const int source_y = plan.source_visible.y - plan.source_bounds.y + y;
                const int source_x = plan.source_visible.x - plan.source_bounds.x;
                const auto* src = pixel_snapshot_payloads_.row(record->payload_slot, source_y);
                if (!src) {
                    out.status = LayerReplayStatus::ExecuteFailed;
                    return out;
                }
                const auto src_offset =
                    static_cast<std::size_t>(source_x) * canvas_.bytes_per_pixel();
                const auto* src_line = src + src_offset;
                if (plan.transform.opacity == 255) {
                    canvas_.blit_span(plan.target_bounds.x,
                                      plan.target_bounds.y + y,
                                      src_line,
                                      row_bytes);
                } else {
                    alpha_blend_pixels += blend_pixel_snapshot_row(plan.target_bounds.x,
                                                                   plan.target_bounds.y + y,
                                                                   src_line,
                                                                   plan.source_visible.w,
                                                                   plan.transform.opacity);
                }
            }
            canvas_.mark_dirty(plan.target_bounds);
            snapshot_store_.note_pixel_blit(plan.composite_pixels);
            out.stats.cmd_count = 0;
            out.stats.cmd_bytes = 0;
            out.stats.alpha_blend_count = alpha_blend_pixels;
            out.status = LayerReplayStatus::Ok;
            return out;
        }

    private:
        void record_current_scene() noexcept {
            cmd_buf_.clear();
            last_cmd_stats_ = to_scene_stats(gui_.record_commands(cmd_buf_));
            if (overlay_fn_) {
                SceneOverlay overlay{cmd_buf_};
                overlay_fn_(overlay, overlay_ctx_);
            }
        }

        LayerEpoch make_layer_epoch() const noexcept {
            const auto& tokens = Theme::instance().get_tokens();
            LayerEpoch epoch{};
            epoch.layout = kernel_.layout_dirty_version();
            epoch.style = StyleSheet::instance().stylesheet_version();
            epoch.theme = tokens.version;
            return epoch;
        }

        bool store_command_snapshot_payload(SnapshotHandle handle) noexcept {
            auto* record = snapshot_store_.mutable_record(handle);
            if (!record) return false;
            const auto payload_slot = command_snapshot_payloads_.store(cmd_buf_);
            if (payload_slot == kInvalidSnapshotPayloadSlot) {
                return false;
            }
            record->payload_slot = payload_slot;
            return true;
        }

        static rgba decode_snapshot_pixel(const std::byte* src) noexcept {
            if (!src) return {};
            if constexpr (screen_pixel_format == PixelFormat::RGB565) {
                std::uint16_t packed{};
                std::memcpy(&packed, src, sizeof(packed));
                const rgb value = unpack_rgb565(packed);
                return {value.r, value.g, value.b, 255};
            } else if constexpr (screen_pixel_format == PixelFormat::RGB888) {
                return {
                    static_cast<std::uint8_t>(src[0]),
                    static_cast<std::uint8_t>(src[1]),
                    static_cast<std::uint8_t>(src[2]),
                    255
                };
            } else {
                std::uint32_t packed{};
                std::memcpy(&packed, src, sizeof(packed));
                return unpack_argb8888(packed);
            }
        }

        std::uint64_t blend_pixel_snapshot_row(int x,
                                               int y,
                                               const std::byte* src,
                                               int width,
                                               std::uint8_t opacity) noexcept {
            if (!src || width <= 0 || opacity == 0) return 0;
            const auto bpp = canvas_.bytes_per_pixel();
            std::uint64_t blended = 0;
            for (int i = 0; i < width; ++i) {
                rgba pixel = decode_snapshot_pixel(src + static_cast<std::size_t>(i) * bpp);
                pixel.a = static_cast<std::uint8_t>(
                    (static_cast<std::uint16_t>(pixel.a) * opacity) / 255u);
                if (pixel.a > 0 && pixel.a < 255) {
                    ++blended;
                }
                canvas_.set_pixel(x + i, y, pixel);
            }
            return blended;
        }

        static CmdStats to_scene_stats(const ui::draw_cmd::DrawCmdStats& stats) noexcept {
            CmdStats out{};
            out.cmd_count = stats.cmd_count;
            out.cmd_capacity = stats.cmd_capacity;
            out.cmd_bytes = stats.cmd_bytes;
            out.text_used = stats.text_used;
            out.text_capacity = stats.text_capacity;
            out.blob_used = stats.blob_used;
            out.blob_capacity = stats.blob_capacity;
            out.batch_shrink = stats.batch_shrink;
            out.batch_shrink_line = stats.batch_shrink_line;
            out.batch_shrink_path = stats.batch_shrink_path;
            out.batch_shrink_rect = stats.batch_shrink_rect;
            out.batch_shrink_round = stats.batch_shrink_round;
            out.batch_shrink_image = stats.batch_shrink_image;
            out.batch_shrink_focus = stats.batch_shrink_focus;
            out.cmd_overflowed = stats.cmd_overflowed;
            out.text_overflowed = stats.text_overflowed;
            out.blob_overflowed = stats.blob_overflowed;
            return out;
        }

        static ExecStats to_scene_stats(const ui::draw_cmd::DrawCmdExecStats& stats) noexcept {
            ExecStats out{};
            out.cmd_count = stats.cmd_count;
            out.cmd_bytes = stats.cmd_bytes;
            out.clip_pushes = stats.clip_pushes;
            out.clip_pops = stats.clip_pops;
            out.clip_push_overflow = stats.clip_push_overflow;
            out.clip_pop_underflow = stats.clip_pop_underflow;
            out.clip_invalid = stats.clip_invalid;
            out.failed_cmds = stats.failed_cmds;
            out.fail_text = stats.fail_text;
            out.fail_image = stats.fail_image;
            out.fail_blob = stats.fail_blob;
            out.fail_path = stats.fail_path;
            out.fail_clip = stats.fail_clip;
            out.fail_other = stats.fail_other;
            out.dispatch_groups = stats.dispatch_groups;
            out.batch_flushes = stats.batch_flushes;
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
            out.overflowed = stats.overflowed;
            return out;
        }

        static TileStats to_scene_stats(const ui::draw_cmd::DrawCmdTileStats& stats) noexcept {
            TileStats out{};
            out.tiles_total = stats.tiles_total;
            out.tiles_drawn = stats.tiles_drawn;
            out.cmd_count = stats.cmd_count;
            out.cmd_bytes = stats.cmd_bytes;
            out.tile_flush_count = stats.tile_flush_count;
            out.clip_push_overflow = stats.clip_push_overflow;
            out.clip_pop_underflow = stats.clip_pop_underflow;
            out.clip_invalid = stats.clip_invalid;
            out.dispatch_groups = stats.dispatch_groups;
            out.batch_flushes = stats.batch_flushes;
            out.failed_cmds = stats.failed_cmds;
            out.fail_text = stats.fail_text;
            out.fail_image = stats.fail_image;
            out.fail_blob = stats.fail_blob;
            out.fail_path = stats.fail_path;
            out.fail_clip = stats.fail_clip;
            out.fail_other = stats.fail_other;
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

        CanvasBase& canvas_;
        SoaKernel kernel_{};
        SoaFactory factory_;
        SoaGui gui_;
        WidgetHandle root_{};
        ui::draw_cmd::DefaultDrawCmdBuffer cmd_buf_{};
        CommandSnapshotPayloadStore<static_cast<std::size_t>(layer_cache_slots)> command_snapshot_payloads_{};
        PixelSnapshotPayloadStore<static_cast<std::size_t>(layer_cache_slots)> pixel_snapshot_payloads_{};
        ui::draw_cmd::DrawCmdExecutor cmd_exec_{};
        CmdStats last_cmd_stats_{};
        ExecStats last_exec_stats_{};
        DefaultSnapshotStore snapshot_store_{};
        OverlayFn overlay_fn_{nullptr};
        void* overlay_ctx_{nullptr};
    };

    class PageLayer {
    public:
        PageLayer() noexcept = default;
        explicit PageLayer(WidgetHandle root) noexcept : root_(root) {}

        void set_root(WidgetHandle root) noexcept {
            root_ = root;
            if (!root_) {
                visible_ = false;
                snapshot_ = {};
                state_ = LayerState::Hidden;
            } else if (state_ == LayerState::Hidden || !snapshot_) {
                state_ = visible_ ? LayerState::Live : LayerState::Hidden;
            }
        }

        WidgetHandle root() const noexcept { return root_; }
        void set_hooks(const PageHooks& hooks) noexcept { hooks_ = hooks; }
        bool visible() const noexcept { return visible_; }
        LayerState state() const noexcept { return state_; }
        bool live() const noexcept { return state_ == LayerState::Live; }
        bool frozen() const noexcept { return state_ == LayerState::Frozen; }
        bool transitioning() const noexcept { return state_ == LayerState::Transitioning; }
        bool stale_snapshot() const noexcept { return state_ == LayerState::StaleSnapshot; }
        SnapshotHandle snapshot() const noexcept { return snapshot_; }

        void show(SceneAccess& access) noexcept { set_visible(access, true); }
        void hide(SceneAccess& access) noexcept { set_visible(access, false); }

        void set_visible(SceneAccess& access, bool on) noexcept {
            if (!root_) return;
            access.set_visible(root_, on);
            const bool changed = (visible_ != on);
            visible_ = on;
            if (!snapshot_ || state_ == LayerState::Live || state_ == LayerState::Hidden) {
                state_ = on ? LayerState::Live : LayerState::Hidden;
            }
            if (changed) {
                if (on) {
                    if (hooks_.on_show) hooks_.on_show(access, root_, hooks_.ctx);
                } else {
                    if (hooks_.on_hide) hooks_.on_hide(access, root_, hooks_.ctx);
                }
            }
        }

        [[nodiscard]] LayerCaptureResult freeze(Scene& scene,
                                                const SnapshotSpec& spec) noexcept {
            if (!root_) return {};
            release_snapshot(scene);
            LayerCaptureResult result = (spec.preferred_kind == SnapshotKind::PixelSurface)
                ? scene.capture_pixel_snapshot_result(spec)
                : scene.capture_command_snapshot_result(spec);
            if (result.ok() && result.handle) {
                snapshot_ = result.handle;
                state_ = LayerState::Frozen;
            }
            return result;
        }

        [[nodiscard]] LayerCaptureResult freeze(Scene& scene,
                                                SceneAccess access,
                                                const SnapshotSpec& spec,
                                                bool hide_live_root) noexcept {
            const auto result = freeze(scene, spec);
            if (result.ok() && hide_live_root) {
                set_visible(access, false);
                state_ = LayerState::Frozen;
            }
            return result;
        }

        void mark_transitioning() noexcept {
            if (snapshot_) state_ = LayerState::Transitioning;
        }

        bool mark_stale(Scene& scene) noexcept {
            if (!snapshot_) return false;
            const bool marked = scene.mark_snapshot_stale(snapshot_);
            if (marked) state_ = LayerState::StaleSnapshot;
            return marked;
        }

        bool release_snapshot(Scene& scene) noexcept {
            if (!snapshot_) return false;
            const auto handle = snapshot_;
            snapshot_ = {};
            const bool released = scene.release_snapshot(handle);
            state_ = visible_ ? LayerState::Live : LayerState::Hidden;
            return released;
        }

        void thaw(Scene& scene, SceneAccess access, bool show_live_root = true) noexcept {
            (void)release_snapshot(scene);
            if (show_live_root) {
                set_visible(access, true);
            } else {
                state_ = visible_ ? LayerState::Live : LayerState::Hidden;
            }
        }

        void reset_snapshot_tracking() noexcept {
            snapshot_ = {};
            state_ = visible_ ? LayerState::Live : LayerState::Hidden;
        }

    private:
        WidgetHandle root_{};
        bool visible_{false};
        LayerState state_{LayerState::Hidden};
        SnapshotHandle snapshot_{};
        PageHooks hooks_{};
    };
}
