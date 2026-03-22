module;

#include <algorithm>
#include <cstddef>
#include <cstdint>

export module charm.ui.scene;

export import charm.core.event;
export import charm.core.geometry;
export import charm.core.handle;
export import charm.core.soa_factory;
export import charm.core.soa_gui;
export import charm.core.soa_kernel;
export import charm.core.soa_payload;
export import charm.gfx.canvas;
export import charm.gfx.color;
export import charm.gfx.draw_cmd;
export import charm.gfx.image;
export import charm.gfx.render_style;

export namespace ui::scene {
    class SceneAccess {
    public:
        SceneAccess() noexcept = default;
        explicit SceneAccess(SoaKernel& kernel) noexcept : kernel_(&kernel) {}

        bool valid() const noexcept { return kernel_ != nullptr; }

        soa_detail::TextSlotId alloc_text_slot() noexcept { return kernel_->alloc_text_slot(); }
        void free_text_slot(soa_detail::TextSlotId slot) noexcept { kernel_->free_text_slot(slot); }

        WidgetKind kind(WidgetHandle h) const noexcept { return kernel_->kind(h); }
        Rect world_rect(WidgetHandle h) const noexcept { return kernel_->world_rect(h); }

        void set_text(WidgetHandle h, const char* text) noexcept { kernel_->set_text(h, text); }
        void set_text_slot(WidgetHandle h, soa_detail::TextSlotId slot, const char* text) noexcept {
            kernel_->set_text_slot(h, slot, text);
        }

        void set_image(WidgetHandle h, ui::gfx::ImageId image) noexcept { kernel_->set_image(h, image); }
        void set_button_icon(WidgetHandle h, ui::gfx::ImageId icon) noexcept { kernel_->set_button_icon(h, icon); }

        void set_list_view_source(WidgetHandle h,
                                  std::uint16_t count,
                                  const void* ctx,
                                  soa_detail::ListViewTextFn text_fn) noexcept {
            kernel_->set_list_view_source(h, count, ctx, text_fn);
        }
        void set_list_view_selected(WidgetHandle h, int index) noexcept { kernel_->set_list_view_selected(h, index); }
        int list_view_selected(WidgetHandle h) const noexcept { return kernel_->list_view_selected(h); }

        void set_list_row_height(WidgetHandle h, int height) noexcept { kernel_->set_list_row_height(h, height); }
        void set_scroll_step(WidgetHandle h, int step) noexcept { kernel_->set_scroll_step(h, step); }

        void set_visible(WidgetHandle h, bool v) noexcept { kernel_->set_visible(h, v); }
        void set_value(WidgetHandle h, int value) noexcept { kernel_->set_value(h, value); }
        int value(WidgetHandle h) const noexcept { return kernel_->value(h); }
        bool checked(WidgetHandle h) const noexcept { return kernel_->checked(h); }
        void set_focused(WidgetHandle h, bool v) noexcept { kernel_->set_focused(h, v); }

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
        WidgetHandle create_progress() noexcept { return factory_.create_progress(); }
        WidgetHandle create_list_view() noexcept { return factory_.create_list_view(); }
        WidgetHandle create_scrollbar_for(WidgetHandle target) noexcept { return factory_.create_scrollbar_for(target); }
        WidgetHandle create_button_static(const char* text) noexcept { return factory_.create_button_static(text); }
        WidgetHandle create_slider() noexcept { return factory_.create_slider(); }
        WidgetHandle create_switch() noexcept { return factory_.create_switch(); }

        void set_button_icon(WidgetHandle h, ui::gfx::ImageId icon) noexcept { factory_.set_button_icon(h, icon); }
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
            last_cmd_stats_ = gui_.record_commands(cmd_buf_);
            if (overlay_fn_) {
                SceneOverlay overlay{cmd_buf_};
                overlay_fn_(overlay, overlay_ctx_);
            }
            last_exec_stats_ = cmd_exec_.execute(canvas_, cmd_buf_);
        }

        template <ui::RenderBackend Backend>
        ui::draw_cmd::DrawCmdTileStats render_tiles(Backend& backend,
                                                    const FrameBufferView& tile_buffer,
                                                    const ui::draw_cmd::DrawCmdTileConfig& config) {
            cmd_buf_.clear();
            last_cmd_stats_ = gui_.record_commands(cmd_buf_);
            if (overlay_fn_) {
                SceneOverlay overlay{cmd_buf_};
                overlay_fn_(overlay, overlay_ctx_);
            }
            return cmd_exec_.execute_tiles(backend, tile_buffer, cmd_buf_, config);
        }

        void dispatch_event(const Event& e) { gui_.dispatch_event(e); }
        WidgetHandle hit_test(int x, int y) noexcept { return gui_.hit_test(x, y); }

        Rect world_rect(WidgetHandle h) const noexcept { return kernel_.world_rect(h); }
        SceneAccess access() noexcept { return SceneAccess(kernel_); }

        ui::draw_cmd::DrawCmdStats last_cmd_stats() const noexcept { return last_cmd_stats_; }
        ui::draw_cmd::DrawCmdExecStats last_exec_stats() const noexcept { return last_exec_stats_; }

    private:
        CanvasBase& canvas_;
        SoaKernel kernel_{};
        SoaFactory factory_;
        SoaGui gui_;
        WidgetHandle root_{};
        ui::draw_cmd::DefaultDrawCmdBuffer cmd_buf_{};
        ui::draw_cmd::DrawCmdExecutor cmd_exec_{};
        ui::draw_cmd::DrawCmdStats last_cmd_stats_{};
        ui::draw_cmd::DrawCmdExecStats last_exec_stats_{};
        OverlayFn overlay_fn_{nullptr};
        void* overlay_ctx_{nullptr};
    };
}
