module;
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
export module charm.core.gui;

export import charm.gfx.canvas;
export import charm.gfx.render;
export import charm.core.event;
export import charm.core.object;
export import charm.core.handle;
export import charm.core.factory;
export import charm.core.layout;
export import charm.core.input_router;
export import charm.widgets.scroll_container;
export import charm.widgets.list_view;
export import charm.core.config;
export import charm.core.render_tree;
import service_trace;
import util.core;
import out.api;

#ifndef CHARM_VIVID_ENABLE_LAYER_CACHE
#define CHARM_VIVID_ENABLE_LAYER_CACHE 1
#endif


export
class Gui {
public:
    Gui(CanvasBase& cvs, UiFactory& factory, WidgetHandle root)
        : canvas(cvs),
          factory_(factory),
          root_(root),
          router_(factory, root) {}

    WidgetHandle focused() const noexcept { return router_.focused(); }
    WidgetHandle hovered() const noexcept { return router_.hovered(); }
    WidgetHandle pressed() const noexcept { return router_.pressed(); }
    WidgetHandle captured() const noexcept { return router_.captured(); }
    bool dragging() const noexcept { return router_.dragging(); }
    void set_dirty_tracking(bool on) noexcept { dirty_tracking_ = on; }
    bool dirty_tracking() const noexcept { return dirty_tracking_; }
    void set_layer_cache(bool on) noexcept {
#if CHARM_VIVID_ENABLE_LAYER_CACHE
        layer_cache_ = on; cache_valid_ = false;
#else
        (void)on; layer_cache_ = false;
#endif
    }
    bool layer_cache() const noexcept { return layer_cache_; }
    void set_subtree_cache(bool on) noexcept { subtree_cache_ = on; }
    bool subtree_cache() const noexcept { return subtree_cache_; }
    void set_cache_debug(bool on) noexcept { cache_debug_ = on; }
    bool cache_debug() const noexcept { return cache_debug_; }
    void invalidate_cache() noexcept {
#if CHARM_VIVID_ENABLE_LAYER_CACHE
        cache_valid_ = false;
#endif
    }
    int last_frame_nodes() const noexcept { return debug_nodes_; }
    int last_depth_hits() const noexcept { return debug_depth_hits_; }
    int last_cycle_hits() const noexcept { return debug_cycle_hits_; }
    void dump_trace() const noexcept {
        const auto total = trace_.size();
        const auto cap = trace_.capacity();
        const auto head = trace_.head();
        const auto& data = trace_.data();
        const auto start = (head + cap - total) % cap;
        auto out = out::raw();
        (void)out.template try_println<"trace_v1,t,id,kind,payload,count">();
        for (util::usize i = 0; i < total; ++i) {
            const auto idx = (start + i) % cap;
            const auto& rec = data[idx];
            (void)out.template try_println<"{},{},{},{},{}">(
                rec.time,
                rec.id,
                static_cast<unsigned>(rec.kind),
                rec.payload,
                rec.count);
        }
    }

    void dump_trace_demo() noexcept {
        if (trace_.size() == 0) {
            trace_counter(GuiTraceId::FrameNodes, 0, 0);
        }
        dump_trace();
    }

    struct TileRenderConfig {
        int tile_width{64};
        int tile_height{64};
        rgba clear_color{0, 0, 0, 0};
        bool clear_tile{true};
    };

    struct FullFrameConfig {
        bool use_dirty{true};
        bool clear_buffer{false};
        bool present{true};
        rgba clear_color{0, 0, 0, 0};
    };

    enum class RenderMode : std::uint8_t {
        FullFrame = 0,
        Tile = 1
    };

    struct RenderTarget {
        RenderMode mode{RenderMode::Tile};
        FrameBufferView buffer{};
        TileRenderConfig tile{};
        FullFrameConfig full{};
    };

    struct TileRenderStats {
        int tiles_total{0};
        int tiles_drawn{0};
        int nodes_drawn{0};
    };

    struct FullFrameStats {
        int rects_total{0};
        int rects_drawn{0};
        int rows_drawn{0};
    };

    struct RenderStats {
        RenderMode mode{RenderMode::Tile};
        TileRenderStats tile{};
        FullFrameStats full{};
    };

    // Render one frame.
    void render() {
        static util::u32 frame_no = 0;
        canvas.begin_frame();
        sanitize_tree_and_trace(frame_no);

#if CHARM_VIVID_ENABLE_LAYER_CACHE
        if (layer_cache_) {
            if (!cache_valid_) {
                cache_canvas_.begin_frame();
                render_tree(cache_canvas_);
                cache_canvas_.end_frame();
                cache_valid_ = true;
            }
            blit_cache();
        } else {
            render_tree(canvas);
        }
#else
        render_tree(canvas);
#endif
        frame_no++;
        canvas.end_frame();
    }

    template<ui::RenderBackend Backend>
    TileRenderStats render_tiles(Backend& backend,
                                 const FrameBufferView& tile_buffer,
                                 const TileRenderConfig& config) {
        TileRenderStats stats{};
        if (!tile_buffer.data) return stats;
        if (config.tile_width <= 0 || config.tile_height <= 0) return stats;

        static util::u32 frame_no = 0;
        sanitize_tree_and_trace(frame_no);
        register_layout_engines();
        debug_nodes_ = 0;
        debug_depth_hits_ = 0;
        debug_cycle_hits_ = 0;
        debug_clip_hits_ = 0;
        debug_cache_hits_ = 0;
        debug_invisible_ = 0;
        debug_missing_ = 0;
        render_tree_.clear();

        RuntimeCanvas logic_canvas(nullptr, screen_width, screen_height, tile_buffer.format);
        (void)run_traversal(logic_canvas, root_, false, true, false);

        const int screen_w = backend.width();
        const int screen_h = backend.height();
        const int buffer_w = static_cast<int>(tile_buffer.width);
        const int buffer_h = static_cast<int>(tile_buffer.height);
        if (buffer_w <= 0 || buffer_h <= 0) return stats;
        const int tile_w = (config.tile_width < buffer_w) ? config.tile_width : buffer_w;
        const int tile_h = (config.tile_height < buffer_h) ? config.tile_height : buffer_h;
        const std::size_t stride = (tile_buffer.stride_bytes != 0)
            ? tile_buffer.stride_bytes
            : static_cast<std::size_t>(buffer_w) * bytes_per_pixel(tile_buffer.format);
        RuntimeCanvas tile_canvas(tile_buffer.data,
                                  buffer_w,
                                  buffer_h,
                                  tile_buffer.format,
                                  stride);

        backend.begin_frame();
        for (int y = 0; y < screen_h; y += tile_h) {
            for (int x = 0; x < screen_w; x += tile_w) {
                const int w = ((x + tile_w) <= screen_w) ? tile_w : (screen_w - x);
                const int h = ((y + tile_h) <= screen_h) ? tile_h : (screen_h - y);
                if (w <= 0 || h <= 0) continue;
                Rect tile_rect{x, y, w, h};
                stats.tiles_total++;

                if (config.clear_tile) {
                    tile_canvas.clear(config.clear_color);
                }
                tile_canvas.set_origin(-tile_rect.x, -tile_rect.y);

                bool tile_hit = false;
                const std::size_t node_count = render_tree_.size();
                for (std::size_t i = 0; i < node_count; ++i) {
                    const auto& node = render_tree_.node(i);
                    Rect node_rect = node.draw_clip;
                    if (!rect_valid(node_rect)) continue;
                    Rect draw_clip{};
                    if (!rect_intersect(node_rect, tile_rect, draw_clip)) continue;
                    tile_hit = true;
                    tile_canvas.set_clip(draw_clip);
                    auto* obj = factory_.get(node.handle);
                    if (!obj) continue;
                    obj->draw(tile_canvas);
                    stats.nodes_drawn++;
                }

                if (!tile_hit) {
                    tile_canvas.clear_origin();
                    continue;
                }

                const std::size_t row_bytes = static_cast<std::size_t>(w)
                    * bytes_per_pixel(tile_buffer.format);
                for (int row = 0; row < h; ++row) {
                    const std::byte* src = tile_buffer.data
                        + static_cast<std::size_t>(row) * stride;
                    backend.blit_span(x, y + row, src, row_bytes);
                }
                backend.mark_dirty(x, y, w, h);
                stats.tiles_drawn++;
                tile_canvas.clear_origin();
            }
        }
        backend.end_frame();
        frame_no++;

        trace_counter(GuiTraceId::FrameNodes, (util::u64)debug_nodes_, 0);
        trace_counter(GuiTraceId::FrameDepthHits, (util::u64)debug_depth_hits_, 0);
        trace_counter(GuiTraceId::FrameCycleHits, (util::u64)debug_cycle_hits_, 0);
        trace_counter(GuiTraceId::FrameClipHits, (util::u64)debug_clip_hits_, 0);
        trace_counter(GuiTraceId::FrameCacheHits, (util::u64)debug_cache_hits_, 0);
        trace_counter(GuiTraceId::FrameInvisible, (util::u64)debug_invisible_, 0);
        trace_counter(GuiTraceId::FrameMissing, (util::u64)debug_missing_, 0);

        return stats;
    }

    template<ui::RenderBackend Backend>
    TileRenderStats render_tiles(Backend& backend,
                                 const FrameBufferView& tile_buffer) {
        return render_tiles(backend, tile_buffer, TileRenderConfig{});
    }

    template<ui::RenderBackend Backend>
    FullFrameStats render_fullframe(Backend& backend,
                                    const FrameBufferView& buffer,
                                    const FullFrameConfig& config) {
        FullFrameStats stats{};
        if (!buffer.data) return stats;
        if (buffer.width == 0 || buffer.height == 0) return stats;

        static util::u32 frame_no = 0;
        sanitize_tree_and_trace(frame_no);
        register_layout_engines();
        debug_nodes_ = 0;
        debug_depth_hits_ = 0;
        debug_cycle_hits_ = 0;
        debug_clip_hits_ = 0;
        debug_cache_hits_ = 0;
        debug_invisible_ = 0;
        debug_missing_ = 0;
        render_tree_.clear();

        const std::size_t stride = (buffer.stride_bytes != 0)
            ? buffer.stride_bytes
            : static_cast<std::size_t>(buffer.width) * bytes_per_pixel(buffer.format);
        RuntimeCanvas full_canvas(buffer.data,
                                  static_cast<int>(buffer.width),
                                  static_cast<int>(buffer.height),
                                  buffer.format,
                                  stride);

        if (config.clear_buffer) {
            full_canvas.clear(config.clear_color);
        }

        const bool prev_dirty = dirty_tracking_;
        dirty_tracking_ = config.use_dirty;
        full_canvas.begin_frame();
        render_tree(full_canvas);
        full_canvas.end_frame();
        dirty_tracking_ = prev_dirty;

        if (!config.present) {
            frame_no++;
            return stats;
        }

        const Rect full_rect{0, 0, static_cast<int>(buffer.width), static_cast<int>(buffer.height)};
        backend.begin_frame();
        if (!config.use_dirty || full_canvas.dirty_full()) {
            stats.rects_total = 1;
            stats.rects_drawn = 1;
            for (int y = 0; y < full_rect.h; ++y) {
                const std::byte* src = buffer.data + static_cast<std::size_t>(y) * stride;
                const std::size_t row_bytes = static_cast<std::size_t>(full_rect.w)
                    * bytes_per_pixel(buffer.format);
                backend.blit_span(0, y, src, row_bytes);
                stats.rows_drawn++;
            }
            backend.mark_dirty(full_rect.x, full_rect.y, full_rect.w, full_rect.h);
        } else {
            const auto& list = full_canvas.dirty_list();
            const util::usize count = list.size();
            stats.rects_total = static_cast<int>(count);
            for (util::usize i = 0; i < count; ++i) {
                Rect clipped{};
                if (!rect_intersect(list[i], full_rect, clipped)) {
                    continue;
                }
                if (!rect_valid(clipped)) {
                    continue;
                }
                stats.rects_drawn++;
                for (int y = clipped.y; y < clipped.y + clipped.h; ++y) {
                    const std::byte* src = buffer.data + static_cast<std::size_t>(y) * stride
                        + static_cast<std::size_t>(clipped.x) * bytes_per_pixel(buffer.format);
                    const std::size_t row_bytes = static_cast<std::size_t>(clipped.w)
                        * bytes_per_pixel(buffer.format);
                    backend.blit_span(clipped.x, y, src, row_bytes);
                    stats.rows_drawn++;
                }
                backend.mark_dirty(clipped.x, clipped.y, clipped.w, clipped.h);
            }
        }
        backend.end_frame();
        frame_no++;

        trace_counter(GuiTraceId::FrameNodes, (util::u64)debug_nodes_, 0);
        trace_counter(GuiTraceId::FrameDepthHits, (util::u64)debug_depth_hits_, 0);
        trace_counter(GuiTraceId::FrameCycleHits, (util::u64)debug_cycle_hits_, 0);
        trace_counter(GuiTraceId::FrameClipHits, (util::u64)debug_clip_hits_, 0);
        trace_counter(GuiTraceId::FrameCacheHits, (util::u64)debug_cache_hits_, 0);
        trace_counter(GuiTraceId::FrameInvisible, (util::u64)debug_invisible_, 0);
        trace_counter(GuiTraceId::FrameMissing, (util::u64)debug_missing_, 0);

        return stats;
    }

    template<ui::RenderBackend Backend>
    FullFrameStats render_fullframe(Backend& backend,
                                    const FrameBufferView& buffer) {
        return render_fullframe(backend, buffer, FullFrameConfig{});
    }

    template<ui::RenderBackend Backend>
    RenderStats render_with(Backend& backend,
                            const RenderTarget& target) {
        RenderStats stats{};
        stats.mode = target.mode;
        if (target.mode == RenderMode::FullFrame) {
            stats.full = render_fullframe(backend, target.buffer, target.full);
        } else {
            stats.tile = render_tiles(backend, target.buffer, target.tile);
        }
        return stats;
    }

    // Dispatch an input event (global coordinates).
    void dispatch_event(const Event& e) {
        #if CHARM_VIVID_ENABLE_LAYER_CACHE
        if (layer_cache_) {
            cache_valid_ = false;
        }
#endif
        router_.dispatch_event(e);
        if (subtree_cache_) {
            auto target = router_.last_event_target();
            if (target) {
                mark_cache_dirty_chain(target);
            } else {
                cache_.invalidate_all();
            }
        }
    }

    void dispatch_touch_down(int id, int x, int y) {
        #if CHARM_VIVID_ENABLE_LAYER_CACHE
        if (layer_cache_) {
            cache_valid_ = false;
        }
#endif
        router_.on_touch_down(id, x, y);
        if (subtree_cache_) {
            auto target = router_.last_event_target();
            if (target) {
                mark_cache_dirty_chain(target);
            } else {
                cache_.invalidate_all();
            }
        }
    }

    void dispatch_touch_move(int id, int x, int y) {
        #if CHARM_VIVID_ENABLE_LAYER_CACHE
        if (layer_cache_) {
            cache_valid_ = false;
        }
#endif
        router_.on_touch_move(id, x, y);
        if (subtree_cache_) {
            auto target = router_.last_event_target();
            if (target) {
                mark_cache_dirty_chain(target);
            } else {
                cache_.invalidate_all();
            }
        }
    }

    void dispatch_touch_up(int id, int x, int y) {
        #if CHARM_VIVID_ENABLE_LAYER_CACHE
        if (layer_cache_) {
            cache_valid_ = false;
        }
#endif
        router_.on_touch_up(id, x, y);
        if (subtree_cache_) {
            auto target = router_.last_event_target();
            if (target) {
                mark_cache_dirty_chain(target);
            } else {
                cache_.invalidate_all();
            }
        }
    }

private:
    enum class GuiTraceId : util::u32 {
        FrameNodes = 1,
        FrameDepthHits = 2,
        FrameCycleHits = 3,
        FrameClipHits = 4,
        FrameCacheHits = 5,
        FrameInvisible = 6,
        FrameMissing = 7,
        SanitizeRemoved = 10,
        SanitizeMissing = 11,
        SanitizeSelf = 12,
        SanitizeInvalidParent = 13,
        SanitizeCycle = 14,
    };

    static constexpr util::usize kTraceCapacity = 256;
    using TraceRecord = service::TraceRecord<util::u32, kTraceCapacity>;
    using TraceBuffer = service::TraceBuffer<util::u32, kTraceCapacity>;
    using LayerFrameBuffer = FrameBuffer<screen_pixel_format, static_cast<std::size_t>(layer_cache_width), static_cast<std::size_t>(layer_cache_height)>;
    using LayerCanvas = Canvas<screen_pixel_format, static_cast<std::size_t>(layer_cache_width), static_cast<std::size_t>(layer_cache_height)>;
    static constexpr int kMaxDepth = 128;

    void trace_counter(GuiTraceId id, util::u64 payload, util::u32 frame) noexcept {
        TraceRecord rec{};
        rec.time = frame;
        rec.id = static_cast<util::u32>(id);
        rec.payload = payload;
        rec.count = 1;
        rec.kind = service::TraceKind::counter;
        trace_.push(rec);
    }

    void sanitize_tree_and_trace(util::u32 frame_no) {
        factory_.sanitize_tree(root_);
        const WidgetHandle ov = factory_.overlay();
        if (ov) factory_.sanitize_tree(ov);
        const auto& rep = factory_.last_sanitize_report();
        if (rep.removed > 0) {
            static int frame_mod = 0;
            frame_mod = (frame_mod + 1) % 60;
            if (frame_mod == 0) {
                trace_counter(GuiTraceId::SanitizeRemoved, (util::u64)rep.removed, frame_no);
                trace_counter(GuiTraceId::SanitizeMissing, (util::u64)rep.missing, frame_no);
                trace_counter(GuiTraceId::SanitizeSelf, (util::u64)rep.self_ref, frame_no);
                trace_counter(GuiTraceId::SanitizeInvalidParent, (util::u64)rep.invalid_parent, frame_no);
                trace_counter(GuiTraceId::SanitizeCycle, (util::u64)rep.cycle, frame_no);
            }
        }
    }

    struct NodeState {
        WidgetHandle handle{};
        ObjectBase* obj{nullptr};
        Rect paint_rect{};
        Rect cache_rect{};
        Rect visible_rect{};
        CanvasBase::ClipState clip_state{};
        bool clip_applied{false};
        bool subtree_dirty{false};
        bool skip_children{false};
        std::size_t node_index{static_cast<std::size_t>(-1)};
        std::size_t child_start{0};
    };

    struct RenderBackend {
        static void draw(ObjectBase& obj, CanvasBase& target) noexcept {
            obj.draw(target);
        }
    };

    struct RenderContext {
        Gui& gui;
        CanvasBase& target;
        bool allow_cache{true};
        bool build_tree{true};
        bool draw_enabled{true};

        bool enter(NodeState& state, WidgetHandle h, std::size_t parent_index) {
            auto* obj = gui.factory_.get(h);
            if (!obj) {
                ++gui.debug_missing_;
                return false;
            }
            if (!obj->is_visible()) {
                ++gui.debug_invisible_;
                return false;
            }
            state.handle = h;
            state.obj = obj;
            ++gui.debug_nodes_;
            apply_layout(gui.factory_, *obj);
            state.paint_rect = obj->paint_bounds();
            state.cache_rect = clamp_to_screen(state.paint_rect);
            state.visible_rect = state.cache_rect;
            state.clip_state = target.save_clip();
            if (state.clip_state.enabled) {
                if (!rect_intersect(state.cache_rect, state.clip_state.rect, state.visible_rect)) {
                    ++gui.debug_clip_hits_;
                    return false;
                }
            }
            if (!rect_valid(state.visible_rect)) {
                ++gui.debug_clip_hits_;
                return false;
            }

            if (allow_cache
                && draw_enabled
                && gui.subtree_cache_
                && layer_cache_slots > 0
                && obj->cache_policy() == ObjectBase::CachePolicy::Subtree) {
                if (try_cache(state)) {
                    return false;
                }
            }

            if (draw_enabled) {
                RenderBackend::draw(*obj, target);
                Rect hint{};
                if (obj->take_dirty_hint(hint)) {
                    state.subtree_dirty = true;
                    if (gui.dirty_tracking_) {
                        target.mark_dirty(hint);
                    }
                } else if (gui.dirty_tracking_) {
                    target.mark_dirty(state.visible_rect);
                }
            }

            if (build_tree) {
                state.node_index = gui.render_tree_.push(RenderNode{
                    .handle = h,
                    .rect = state.paint_rect,
                    .draw_clip = state.visible_rect,
                    .clip = state.paint_rect,
                    .clip_enabled = false,
                    .parent = parent_index,
                    .first_child = 0,
                    .child_count = 0,
                });
            }

            if (!apply_child_clip(state)) {
                state.skip_children = true;
            }
            state.child_start = build_tree ? gui.render_tree_.size() : 0;
            return true;
        }

        void exit(NodeState& state) {
            if (build_tree && state.node_index != static_cast<std::size_t>(-1)) {
                auto& node = gui.render_tree_.node_mut(state.node_index);
                if (gui.render_tree_.size() > state.child_start) {
                    node.first_child = state.child_start;
                    node.child_count = gui.render_tree_.size() - state.child_start;
                }
            }

            if (state.clip_applied) {
                target.restore_clip(state.clip_state);
            }

            if (state.subtree_dirty && state.obj
                && state.obj->cache_policy() == ObjectBase::CachePolicy::Subtree) {
                state.obj->mark_cache_dirty();
            }
        }

    private:
        bool try_cache(NodeState& state) {
            if (state.cache_rect.w <= 0 || state.cache_rect.h <= 0) {
                return true;
            }
            if (state.cache_rect.w > layer_cache_width || state.cache_rect.h > layer_cache_height) {
                state.obj->mark_cache_dirty();
                return false;
            }
            auto* slot = gui.cache_.find_or_assign(state.handle);
            if (!slot) return false;
            if (slot->valid && !state.obj->cache_dirty()) {
                if (slot->rect.x == state.cache_rect.x && slot->rect.y == state.cache_rect.y
                    && slot->rect.w == state.cache_rect.w && slot->rect.h == state.cache_rect.h) {
                    blit_layer(*slot, target, state.visible_rect);
                    gui.draw_cache_debug(target, state.visible_rect, true);
                    ++gui.debug_cache_hits_;
                    return true;
                }
                slot->valid = false;
            }
            state.obj->clear_cache_dirty();
            if (slot->rect.x != state.cache_rect.x || slot->rect.y != state.cache_rect.y
                || slot->rect.w != state.cache_rect.w || slot->rect.h != state.cache_rect.h) {
                slot->valid = false;
            }
            slot->rect = state.cache_rect;
            slot->canvas.set_origin(-state.cache_rect.x, -state.cache_rect.y);
            auto clip_state = slot->canvas.save_clip();
            slot->canvas.set_clip(state.cache_rect);
            slot->canvas.clear();
            slot->canvas.begin_frame();
            gui.run_traversal(slot->canvas, state.handle, false, false, true);
            slot->canvas.end_frame();
            slot->canvas.restore_clip(clip_state);
            slot->canvas.clear_origin();
            slot->valid = true;
            blit_layer(*slot, target, state.visible_rect);
            gui.draw_cache_debug(target, state.visible_rect, false);
            return true;
        }

        bool apply_child_clip(NodeState& state) {
            const auto clip_policy = state.obj->clip_policy();
            if (clip_policy == ObjectBase::ClipPolicy::None) {
                return true;
            }
            Rect clip_rect = state.obj->get_rect();
            switch (clip_policy) {
            case ObjectBase::ClipPolicy::Rect:
                clip_rect = state.obj->get_rect();
                break;
            case ObjectBase::ClipPolicy::LayoutRect:
                clip_rect = state.obj->layout_rect();
                break;
            case ObjectBase::ClipPolicy::Custom:
                clip_rect = state.obj->children_clip_rect();
                break;
            default:
                break;
            }
            bool clip_ok = rect_valid(clip_rect);
            if (state.clip_state.enabled) {
                clip_ok = clip_ok && rect_intersect(clip_rect, state.clip_state.rect, clip_rect);
            }
            if (!clip_ok) {
                if (state.node_index != static_cast<std::size_t>(-1)) {
                    auto& node = gui.render_tree_.node_mut(state.node_index);
                    node.clip = Rect{};
                    node.clip_enabled = true;
                }
                return false;
            }
            target.set_clip(clip_rect);
            state.clip_applied = true;
            if (state.node_index != static_cast<std::size_t>(-1)) {
                auto& node = gui.render_tree_.node_mut(state.node_index);
                node.clip = clip_rect;
                node.clip_enabled = true;
            }
            return true;
        }
    };

    struct Traversal {
        RenderContext& ctx;

        bool run(WidgetHandle root, std::size_t parent_index) {
            struct Frame {
                WidgetHandle handle{};
                std::size_t parent_index{static_cast<std::size_t>(-1)};
                std::size_t next_child{0};
                int depth{0};
                bool entered{false};
                bool subtree_dirty{false};
                NodeState state{};
            };

            std::array<Frame, static_cast<std::size_t>(kMaxDepth + 1)> stack{};
            std::size_t sp = 0;
            stack[0].handle = root;
            stack[0].parent_index = parent_index;
            stack[0].depth = 0;
            sp = 1;

            bool root_dirty = false;
            while (sp > 0) {
                auto& frame = stack[sp - 1];
                if (!frame.entered) {
                    if (frame.depth > kMaxDepth) {
                        ++ctx.gui.debug_depth_hits_;
                        --sp;
                        continue;
                    }
                    if (!ctx.enter(frame.state, frame.handle, frame.parent_index)) {
                        --sp;
                        continue;
                    }
                    frame.entered = true;
                    frame.subtree_dirty = frame.state.subtree_dirty;
                    frame.next_child = 0;
                }

                if (frame.state.skip_children || !frame.state.obj
                    || frame.next_child >= frame.state.obj->child_count()) {
                    frame.state.subtree_dirty = frame.subtree_dirty;
                    ctx.exit(frame.state);
                    const bool dirty = frame.subtree_dirty;
                    --sp;
                    if (sp == 0) {
                        root_dirty = dirty;
                    } else if (dirty) {
                        stack[sp - 1].subtree_dirty = true;
                    }
                    continue;
                }

                const auto ch = frame.state.obj->child_at(frame.next_child++);
                auto* ch_obj = ctx.gui.factory_.get(ch);
                if (!ch_obj) {
                    continue;
                }
                if (!frame.state.obj->should_draw_child(*ch_obj)) {
                    continue;
                }
                if (frame.depth + 1 > kMaxDepth) {
                    ++ctx.gui.debug_depth_hits_;
                    continue;
                }
                bool cycle = false;
                for (std::size_t i = 0; i < sp; ++i) {
                    if (stack[i].handle == ch) {
                        cycle = true;
                        break;
                    }
                }
                if (cycle) {
                    ++ctx.gui.debug_cycle_hits_;
                    continue;
                }
                stack[sp].handle = ch;
                stack[sp].parent_index = frame.state.node_index;
                stack[sp].depth = frame.depth + 1;
                stack[sp].entered = false;
                stack[sp].subtree_dirty = false;
                stack[sp].next_child = 0;
                stack[sp].state = NodeState{};
                ++sp;
            }
            return root_dirty;
        }
    };

    void render_tree(CanvasBase& target) {
        register_layout_engines();
        debug_nodes_ = 0;
        debug_depth_hits_ = 0;
        debug_cycle_hits_ = 0;
        debug_clip_hits_ = 0;
        debug_cache_hits_ = 0;
        debug_invisible_ = 0;
        debug_missing_ = 0;
        render_tree_.clear();
        (void)run_traversal(target, root_, true, true, true);
        trace_counter(GuiTraceId::FrameNodes, (util::u64)debug_nodes_, 0);
        trace_counter(GuiTraceId::FrameDepthHits, (util::u64)debug_depth_hits_, 0);
        trace_counter(GuiTraceId::FrameCycleHits, (util::u64)debug_cycle_hits_, 0);
        trace_counter(GuiTraceId::FrameClipHits, (util::u64)debug_clip_hits_, 0);
        trace_counter(GuiTraceId::FrameCacheHits, (util::u64)debug_cache_hits_, 0);
        trace_counter(GuiTraceId::FrameInvisible, (util::u64)debug_invisible_, 0);
        trace_counter(GuiTraceId::FrameMissing, (util::u64)debug_missing_, 0);
    }

    bool run_traversal(CanvasBase& target,
                       WidgetHandle root,
                       bool allow_cache,
                       bool build_tree,
                       bool draw_enabled) {
        RenderContext ctx{*this, target, allow_cache, build_tree, draw_enabled};
        Traversal traversal{ctx};
        return traversal.run(root, static_cast<std::size_t>(-1));
    }

#if CHARM_VIVID_ENABLE_LAYER_CACHE
    void blit_cache() {
        auto* dst = canvas.data();
        const auto* src = cache_fb_.data();
        std::memcpy(dst, src, DefaultFrameBuffer::buffer_bytes);
    }
#endif

    CanvasBase& canvas;
    UiFactory& factory_;
    WidgetHandle root_;
    int debug_nodes_{0};
    int debug_depth_hits_{0};
    int debug_cycle_hits_{0};
    int debug_clip_hits_{0};
    int debug_cache_hits_{0};
    int debug_invisible_{0};
    int debug_missing_{0};
    bool dirty_tracking_{false};
    bool layer_cache_{false};
    #if CHARM_VIVID_ENABLE_LAYER_CACHE
    bool cache_valid_{false};
#endif
    bool subtree_cache_{false};
    bool cache_debug_{false};
    #if CHARM_VIVID_ENABLE_LAYER_CACHE
    DefaultFrameBuffer cache_fb_{};
    DefaultCanvas cache_canvas_{cache_fb_};
#endif
    struct LayerSlot {
        LayerSlot() : canvas(fb) {}
        LayerFrameBuffer fb{};
        LayerCanvas canvas;
        WidgetHandle owner{};
        Rect rect{};
        bool valid{false};
        bool used{false};
    };
    struct LayerCache {
        LayerSlot* find_or_assign(WidgetHandle h) {
            for (auto& slot : slots) {
                if (slot.used && slot.owner == h) return &slot;
            }
            for (auto& slot : slots) {
                if (!slot.used) {
                    slot.used = true;
                    slot.owner = h;
                    slot.valid = false;
                    return &slot;
                }
            }
            return nullptr;
        }
        void invalidate_all() noexcept {
            for (auto& slot : slots) {
                slot.valid = false;
            }
        }
        std::array<LayerSlot, static_cast<std::size_t>(layer_cache_slots)> slots{};
    };
    static void blit_layer(const LayerSlot& slot, CanvasBase& target, const Rect& r) {
        if (!slot.valid) return;
        const Rect cached = slot.rect;
        const int left = (r.x > cached.x) ? r.x : cached.x;
        const int top = (r.y > cached.y) ? r.y : cached.y;
        const int right = (r.x + r.w < cached.x + cached.w) ? (r.x + r.w) : (cached.x + cached.w);
        const int bottom = (r.y + r.h < cached.y + cached.h) ? (r.y + r.h) : (cached.y + cached.h);
        if (right <= left || bottom <= top) return;
        const std::size_t dst_stride = target.stride_bytes();
        const std::size_t dst_bpp = target.bytes_per_pixel();
        const std::size_t dst_row_bytes = dst_stride / dst_bpp;
        if (dst_row_bytes == 0) return;
        if (left < 0 || right > static_cast<int>(dst_row_bytes)) return;
        constexpr std::size_t src_stride = LayerFrameBuffer::stride_bytes;
        constexpr std::size_t src_bpp = LayerFrameBuffer::bytes_per_pixel;
        const auto* src = slot.fb.data();
        for (int y = top; y < bottom; ++y) {
            const int src_y = y - cached.y;
            const std::size_t src_off = static_cast<std::size_t>(src_y) * src_stride
                + static_cast<std::size_t>(left - cached.x) * src_bpp;
            const std::size_t bytes = static_cast<std::size_t>(right - left) * dst_bpp;
            target.blit_span(left, y, src + src_off, bytes);
        }
    }
    static std::size_t bytes_per_pixel(PixelFormat fmt) noexcept {
        switch (fmt) {
        case PixelFormat::RGB565: return PixelTraits<PixelFormat::RGB565>::bytes_per_pixel;
        case PixelFormat::RGB888: return PixelTraits<PixelFormat::RGB888>::bytes_per_pixel;
        case PixelFormat::ARGB8888: return PixelTraits<PixelFormat::ARGB8888>::bytes_per_pixel;
        default: return PixelTraits<PixelFormat::ARGB8888>::bytes_per_pixel;
        }
    }
    static Rect clamp_to_screen(const Rect& r) noexcept {
        const Rect screen{0, 0, screen_width, screen_height};
        Rect out{};
        if (!rect_intersect(r, screen, out)) {
            return Rect{};
        }
        return out;
    }

    void mark_cache_dirty_chain(WidgetHandle h) noexcept {
        while (h) {
            auto* obj = factory_.get(h);
            if (!obj) return;
            obj->mark_cache_dirty();
            h = obj->parent();
        }
    }
    void draw_cache_debug(CanvasBase& target, const Rect& rect, bool hit) noexcept {
        if (!cache_debug_) return;
        const rgba color = hit ? rgba{0, 200, 0, 255} : rgba{200, 0, 0, 255};
        ui::render::draw_rect(target, rect.x, rect.y, rect.w, rect.h, color, false);
    }
    LayerCache cache_{};
    RenderTree<512> render_tree_{};
    TraceBuffer trace_{};
    InputRouter router_;

    static void register_layout_engines() {
        static bool registered = false;
        if (registered) return;
        registered = true;
        register_layout_engine(ScrollContainer::kLayoutId, &Gui::layout_scroll_container);
        register_layout_engine(ListView::kLayoutId, &Gui::layout_list_view);
    }

    static void layout_scroll_container(UiFactory& factory,
                                        ObjectBase& container,
                                        const ObjectBase::LayoutSpec&) {
        auto& sc = static_cast<ScrollContainer&>(container);
        sc.apply_scroll([&](WidgetHandle ch){ return factory.get(ch); });
    }

    static void layout_list_view(UiFactory&, ObjectBase& container, const ObjectBase::LayoutSpec&) {
        auto& view = static_cast<ListView&>(container);
        view.update_visible_window();
    }
};

