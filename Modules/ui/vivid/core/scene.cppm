module;

#include <algorithm>
#include <cstddef>
#include <cstdint>

export module charm.ui.scene;

export import charm.core.event;
export import charm.core.geometry;
export import charm.core.handle;
import charm.core.soa_factory;
import charm.core.soa_gui;
import charm.core.soa_kernel;
import charm.core.soa_payload;
export import charm.gfx.canvas;
export import charm.gfx.color;
export import charm.gfx.image;
export import charm.gfx.render_style;
import charm.gfx.draw_cmd;

export namespace ui::scene {
    using TextSlotId = std::uint16_t;
    constexpr TextSlotId kInvalidTextSlot = 0xFFFF;

    using ImageId = ui::gfx::ImageId;
    constexpr ImageId invalid_image_id() noexcept { return ui::gfx::invalid_image_id(); }
    constexpr bool image_id_valid(ImageId id) noexcept { return ui::gfx::image_id_valid(id); }

    using ListViewTextFn = const char* (*)(const void*, std::uint16_t) noexcept;
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

    struct TileStats {
        int tiles_total{0};
        int tiles_drawn{0};
        std::size_t cmd_count{0};
        std::size_t cmd_bytes{0};
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

        void set_image(WidgetHandle h, ImageId image) noexcept { kernel_->set_image(h, image); }
        void set_button_icon(WidgetHandle h, ImageId icon) noexcept { kernel_->set_button_icon(h, icon); }

        void set_list_view_source(WidgetHandle h,
                                  std::uint16_t count,
                                  const void* ctx,
                                  ListViewTextFn text_fn) noexcept {
            kernel_->set_list_view_source(h, count, ctx, text_fn);
        }
        void set_list_view_selected(WidgetHandle h, int index) noexcept { kernel_->set_list_view_selected(h, index); }
        int list_view_selected(WidgetHandle h) const noexcept { return kernel_->list_view_selected(h); }

        void set_list_row_height(WidgetHandle h, int height) noexcept { kernel_->set_list_row_height(h, height); }
        void set_scroll_step(WidgetHandle h, int step) noexcept { kernel_->set_scroll_step(h, step); }

        void set_visible(WidgetHandle h, bool v) noexcept { kernel_->set_visible(h, v); }
        void set_value(WidgetHandle h, int value) noexcept { kernel_->set_value(h, value); }
        int value(WidgetHandle h) const noexcept { return kernel_->value(h); }
        void set_checked(WidgetHandle h, bool v) noexcept { kernel_->set_checked(h, v); }
        bool checked(WidgetHandle h) const noexcept { return kernel_->checked(h); }
        void set_focused(WidgetHandle h, bool v) noexcept { kernel_->set_focused(h, v); }
        WidgetHandle input_focused() const noexcept { return kernel_->input_focused(); }

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
        WidgetHandle create_list_view() noexcept { return factory_.create_list_view(); }
        WidgetHandle create_scrollbar_for(WidgetHandle target) noexcept { return factory_.create_scrollbar_for(target); }
        WidgetHandle create_button_static(const char* text) noexcept { return factory_.create_button_static(text); }
        WidgetHandle create_slider() noexcept { return factory_.create_slider(); }
        WidgetHandle create_switch() noexcept { return factory_.create_switch(); }

        void set_button_icon(WidgetHandle h, ImageId icon) noexcept { factory_.set_button_icon(h, icon); }
        void set_button_icon_size(WidgetHandle h, std::uint8_t size) noexcept {
            factory_.set_button_icon_size(h, size);
        }

        void link(WidgetHandle parent, WidgetHandle child) noexcept { factory_.link(parent, child); }

        void set_rect(WidgetHandle h, const Rect& r) noexcept { kernel_.set_rect(h, r); }
        void set_input_root(WidgetHandle h) noexcept { kernel_.set_input_root(h); }
        void set_clip_children(WidgetHandle h, bool v) noexcept { kernel_.set_clip_children(h, v); }
        void set_scroll_step(WidgetHandle h, int step) noexcept { kernel_.set_scroll_step(h, step); }
        void set_range(WidgetHandle h, int min, int max) noexcept { kernel_.set_range(h, min, max); }
        void set_value(WidgetHandle h, int value) noexcept { kernel_.set_value(h, value); }
        void set_hit_testable(WidgetHandle h, bool v) noexcept { kernel_.set_hit_testable(h, v); }
        void set_checked(WidgetHandle h, bool v) noexcept { kernel_.set_checked(h, v); }
        void set_list_row_height(WidgetHandle h, int height) noexcept { kernel_.set_list_row_height(h, height); }
        void set_scrollbar_orientation(WidgetHandle h, ScrollBarOrientation o) noexcept {
            kernel_.set_scrollbar_orientation(h, o);
        }
        void set_variant(WidgetHandle h, std::uint8_t variant) noexcept { kernel_.set_variant(h, variant); }

    private:
        SoaKernel& kernel_;
        SoaFactory& factory_;
        WidgetHandle root_{};
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
            cmd_buf_.clear();
            last_cmd_stats_ = to_scene_stats(gui_.record_commands(cmd_buf_));
            if (overlay_fn_) {
                SceneOverlay overlay{cmd_buf_};
                overlay_fn_(overlay, overlay_ctx_);
            }
            last_exec_stats_ = to_scene_stats(cmd_exec_.execute(canvas_, cmd_buf_));
        }

        template <ui::RenderBackend Backend>
        TileStats render_tiles(Backend& backend,
                               const FrameBufferView& tile_buffer,
                               const TileConfig& config) {
            cmd_buf_.clear();
            last_cmd_stats_ = to_scene_stats(gui_.record_commands(cmd_buf_));
            if (overlay_fn_) {
                SceneOverlay overlay{cmd_buf_};
                overlay_fn_(overlay, overlay_ctx_);
            }
            const ui::draw_cmd::DrawCmdTileConfig cfg{
                config.tile_width,
                config.tile_height,
                config.clear_color,
                config.clear_tile
            };
            return to_scene_stats(cmd_exec_.execute_tiles(backend, tile_buffer, cmd_buf_, cfg));
        }

        void dispatch_event(const Event& e) { gui_.dispatch_event(e); }
        WidgetHandle hit_test(int x, int y) noexcept { return gui_.hit_test(x, y); }

        Rect world_rect(WidgetHandle h) const noexcept { return kernel_.world_rect(h); }
        SceneAccess access() noexcept { return SceneAccess(kernel_); }

        CmdStats last_cmd_stats() const noexcept { return last_cmd_stats_; }
        ExecStats last_exec_stats() const noexcept { return last_exec_stats_; }

    private:
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
        ui::draw_cmd::DrawCmdExecutor cmd_exec_{};
        CmdStats last_cmd_stats_{};
        ExecStats last_exec_stats_{};
        OverlayFn overlay_fn_{nullptr};
        void* overlay_ctx_{nullptr};
    };
}
