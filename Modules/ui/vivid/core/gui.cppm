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

        bool enter(NodeState& state, WidgetHandle h, std::size_t parent_index) {
            auto* obj = gui.factory_.get(h);
            if (!obj) return false;
            if (!obj->is_visible()) return false;
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
                    return false;
                }
            }
            if (!rect_valid(state.visible_rect)) {
                return false;
            }

            if (allow_cache
                && gui.subtree_cache_
                && layer_cache_slots > 0
                && obj->cache_policy() == ObjectBase::CachePolicy::Subtree) {
                if (try_cache(state)) {
                    return false;
                }
            }

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

            if (build_tree) {
                state.node_index = gui.render_tree_.push(RenderNode{
                    .handle = h,
                    .rect = state.paint_rect,
                    .clip = state.paint_rect,
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
            gui.run_traversal(slot->canvas, state.handle, false, false);
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
                    gui.render_tree_.node_mut(state.node_index).clip = Rect{};
                }
                return false;
            }
            target.set_clip(clip_rect);
            state.clip_applied = true;
            if (state.node_index != static_cast<std::size_t>(-1)) {
                gui.render_tree_.node_mut(state.node_index).clip = clip_rect;
            }
            return true;
        }
    };

    struct Traversal {
        RenderContext& ctx;
        std::array<WidgetHandle, kMaxDepth> stack{};

        bool run(WidgetHandle root, std::size_t parent_index) {
            return visit(root, 0, parent_index);
        }

    private:
        bool visit(WidgetHandle h, int depth, std::size_t parent_index) {
            if (depth > kMaxDepth) {
                ++ctx.gui.debug_depth_hits_;
                return false;
            }
            for (int i = 0; i < depth; ++i) {
                if (stack[static_cast<std::size_t>(i)] == h) {
                    ++ctx.gui.debug_cycle_hits_;
                    return false;
                }
            }
            stack[static_cast<std::size_t>(depth)] = h;
            NodeState state{};
            if (!ctx.enter(state, h, parent_index)) {
                return false;
            }
            bool subtree_dirty = state.subtree_dirty;
            if (!state.skip_children && state.obj) {
                const std::size_t count = state.obj->child_count();
                for (std::size_t i = 0; i < count; ++i) {
                    auto ch = state.obj->child_at(i);
                    auto* ch_obj = ctx.gui.factory_.get(ch);
                    if (!ch_obj) continue;
                    if (!state.obj->should_draw_child(*ch_obj)) continue;
                    if (visit(ch, depth + 1, state.node_index)) {
                        subtree_dirty = true;
                    }
                }
            }
            state.subtree_dirty = subtree_dirty;
            ctx.exit(state);
            return subtree_dirty;
        }
    };

    void render_tree(CanvasBase& target) {
        register_layout_engines();
        debug_nodes_ = 0;
        debug_depth_hits_ = 0;
        debug_cycle_hits_ = 0;
        render_tree_.clear();
        (void)run_traversal(target, root_, true, true);
        trace_counter(GuiTraceId::FrameNodes, (util::u64)debug_nodes_, 0);
        trace_counter(GuiTraceId::FrameDepthHits, (util::u64)debug_depth_hits_, 0);
        trace_counter(GuiTraceId::FrameCycleHits, (util::u64)debug_cycle_hits_, 0);
    }

    bool run_traversal(CanvasBase& target,
                       WidgetHandle root,
                       bool allow_cache,
                       bool build_tree) {
        RenderContext ctx{*this, target, allow_cache, build_tree};
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

