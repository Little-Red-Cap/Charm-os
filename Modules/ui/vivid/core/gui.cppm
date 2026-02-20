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


export
class Gui {
public:
    Gui(DefaultCanvas& cvs, UiFactory& factory, WidgetHandle root)
        : canvas(cvs),
          factory_(factory),
          root_(root),
          cache_canvas_(cache_fb_),
          router_(factory, root) {}

    WidgetHandle focused() const noexcept { return router_.focused(); }
    WidgetHandle hovered() const noexcept { return router_.hovered(); }
    WidgetHandle pressed() const noexcept { return router_.pressed(); }
    WidgetHandle captured() const noexcept { return router_.captured(); }
    bool dragging() const noexcept { return router_.dragging(); }
    void set_dirty_tracking(bool on) noexcept { dirty_tracking_ = on; }
    bool dirty_tracking() const noexcept { return dirty_tracking_; }
    void set_layer_cache(bool on) noexcept { layer_cache_ = on; cache_valid_ = false; }
    bool layer_cache() const noexcept { return layer_cache_; }
    void set_subtree_cache(bool on) noexcept { subtree_cache_ = on; }
    bool subtree_cache() const noexcept { return subtree_cache_; }
    void set_cache_debug(bool on) noexcept { cache_debug_ = on; }
    bool cache_debug() const noexcept { return cache_debug_; }
    void invalidate_cache() noexcept { cache_valid_ = false; }
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

    // 渲染一帧
    void render() {
        static util::u32 frame_no = 0;
        canvas.begin_frame();
        sanitize_tree_and_trace(frame_no);

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
        frame_no++;
        canvas.end_frame();
    }

    // 派发一个输入事件（全局坐标）
    void dispatch_event(const Event& e) {
        if (layer_cache_) {
            cache_valid_ = false;
        }
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
        if (layer_cache_) {
            cache_valid_ = false;
        }
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
        if (layer_cache_) {
            cache_valid_ = false;
        }
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
        if (layer_cache_) {
            cache_valid_ = false;
        }
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
    using LayerFrameBuffer = DefaultFrameBuffer;
    using LayerCanvas = DefaultCanvas;

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

    void render_tree(DefaultCanvas& target) {
        register_layout_engines();
        debug_nodes_ = 0;
        debug_depth_hits_ = 0;
        debug_cycle_hits_ = 0;
        WidgetHandle stack[kMaxDepth]{};
        render_tree_.clear();
        (void)draw_recursive(target, root_, 0, stack, true, true, static_cast<std::size_t>(-1));
        trace_counter(GuiTraceId::FrameNodes, (util::u64)debug_nodes_, 0);
        trace_counter(GuiTraceId::FrameDepthHits, (util::u64)debug_depth_hits_, 0);
        trace_counter(GuiTraceId::FrameCycleHits, (util::u64)debug_cycle_hits_, 0);
    }

    void blit_cache() {
        auto* dst = canvas.raw_buffer().data();
        const auto* src = cache_fb_.data();
        std::memcpy(dst, src, DefaultFrameBuffer::buffer_bytes);
    }

    bool draw_recursive(DefaultCanvas& target,
                        WidgetHandle h,
                        int depth,
                        WidgetHandle* stack,
                        bool allow_cache,
                        bool build_tree,
                        std::size_t parent_index) {
        if (depth > kMaxDepth) {
            ++debug_depth_hits_;
            return false;
        }
        for (int i = 0; i < depth; ++i) {
            if (stack[i] == h) {
                ++debug_cycle_hits_;
                return false;
            }
        }
        stack[depth] = h;
        auto* obj = factory_.get(h);
        if (!obj) return false;
        if (!obj->is_visible()) return false;
        ++debug_nodes_;
        apply_layout(factory_, *obj);
        Rect obj_rect = obj->get_rect();
        Rect cache_rect = clamp_to_screen(obj_rect);

        const bool cacheable = (allow_cache && subtree_cache_
            && layer_cache_slots > 0
            && obj->cache_policy() == ObjectBase::CachePolicy::Subtree);
        if (cacheable) {
            if (cache_rect.w <= 0 || cache_rect.h <= 0) return false;
            if (cache_rect.w > layer_cache_width || cache_rect.h > layer_cache_height) {
                obj->mark_cache_dirty();
            } else {
            auto* slot = cache_.find_or_assign(h);
            if (slot && slot->valid && !obj->cache_dirty()) {
                if (slot->rect.x == cache_rect.x && slot->rect.y == cache_rect.y
                    && slot->rect.w == cache_rect.w && slot->rect.h == cache_rect.h) {
                    blit_layer(*slot, target, cache_rect);
                    draw_cache_debug(target, cache_rect, true);
                    return false;
                }
                slot->valid = false;
            }
            if (slot) {
                obj->clear_cache_dirty();
                if (slot->rect.x != cache_rect.x || slot->rect.y != cache_rect.y
                    || slot->rect.w != cache_rect.w || slot->rect.h != cache_rect.h) {
                    slot->valid = false;
                }
                slot->rect = cache_rect;
                slot->canvas.set_origin(-cache_rect.x, -cache_rect.y);
                auto clip_state = slot->canvas.save_clip();
                slot->canvas.set_clip(cache_rect);
                slot->canvas.clear();
                slot->canvas.begin_frame();
                draw_recursive(slot->canvas, h, depth, stack, false, false, static_cast<std::size_t>(-1));
                slot->canvas.end_frame();
                slot->canvas.restore_clip(clip_state);
                slot->canvas.clear_origin();
                slot->valid = true;
                blit_layer(*slot, target, cache_rect);
                draw_cache_debug(target, cache_rect, false);
                return false;
            }
            }
        }

        obj->draw(target);
        bool subtree_dirty = false;
        Rect hint{};
        if (obj->take_dirty_hint(hint)) {
            subtree_dirty = true;
            if (dirty_tracking_) {
                target.mark_dirty(hint);
            }
        } else if (dirty_tracking_) {
            target.mark_dirty(obj_rect);
        }

        std::size_t node_index = static_cast<std::size_t>(-1);
        if (build_tree) {
            node_index = render_tree_.push(RenderNode{
                .handle = h,
                .rect = obj_rect,
                .clip = obj_rect,
                .parent = parent_index,
                .first_child = 0,
                .child_count = 0,
            });
        }

        const auto clip_policy = obj->clip_policy();
        const bool clip_children = (clip_policy != ObjectBase::ClipPolicy::None);
        auto clip_state = target.save_clip();
        if (clip_children) {
            Rect clip_rect = obj->get_rect();
            switch (clip_policy) {
            case ObjectBase::ClipPolicy::Rect:
                clip_rect = obj->get_rect();
                break;
            case ObjectBase::ClipPolicy::LayoutRect:
                clip_rect = obj->layout_rect();
                break;
            case ObjectBase::ClipPolicy::Custom:
                clip_rect = obj->children_clip_rect();
                break;
            default:
                break;
            }
            target.set_clip(clip_rect);
            if (node_index != static_cast<std::size_t>(-1)) {
                render_tree_.node_mut(node_index).clip = clip_rect;
            }
        }

        std::size_t child_start = build_tree ? render_tree_.size() : 0;
        for (std::size_t i = 0; i < obj->child_count(); ++i) {
            auto ch = obj->child_at(i);
            auto* ch_obj = factory_.get(ch);
            if (!ch_obj) continue;
            if (!obj->should_draw_child(*ch_obj)) continue;
            if (draw_recursive(target, ch, depth + 1, stack, true, build_tree, node_index)) {
                subtree_dirty = true;
            }
        }
        if (build_tree && node_index != static_cast<std::size_t>(-1)) {
            auto& node = render_tree_.node_mut(node_index);
            if (render_tree_.size() > child_start) {
                node.first_child = child_start;
                node.child_count = render_tree_.size() - child_start;
            }
        }

        if (clip_children) {
            target.restore_clip(clip_state);
        }

        if (subtree_dirty && obj->cache_policy() == ObjectBase::CachePolicy::Subtree) {
            obj->mark_cache_dirty();
        }
        return subtree_dirty;
    }

    DefaultCanvas& canvas;
    UiFactory& factory_;
    WidgetHandle root_;
    static constexpr int kMaxDepth = 128;
    int debug_nodes_{0};
    int debug_depth_hits_{0};
    int debug_cycle_hits_{0};
    bool dirty_tracking_{false};
    bool layer_cache_{false};
    bool cache_valid_{false};
    bool subtree_cache_{false};
    bool cache_debug_{false};
    DefaultFrameBuffer cache_fb_{};
    DefaultCanvas cache_canvas_;
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
    static void blit_layer(const LayerSlot& slot, DefaultCanvas& target, const Rect& r) {
        if (!slot.valid) return;
        const Rect cached = slot.rect;
        const int left = (r.x > cached.x) ? r.x : cached.x;
        const int top = (r.y > cached.y) ? r.y : cached.y;
        const int right = (r.x + r.w < cached.x + cached.w) ? (r.x + r.w) : (cached.x + cached.w);
        const int bottom = (r.y + r.h < cached.y + cached.h) ? (r.y + r.h) : (cached.y + cached.h);
        if (right <= left || bottom <= top) return;
        constexpr std::size_t dst_stride = DefaultFrameBuffer::stride_bytes;
        constexpr std::size_t dst_bpp = DefaultFrameBuffer::bytes_per_pixel;
        constexpr std::size_t src_stride = LayerFrameBuffer::stride_bytes;
        constexpr std::size_t src_bpp = LayerFrameBuffer::bytes_per_pixel;
        auto* dst = target.raw_buffer().data();
        const auto* src = slot.fb.data();
        for (int y = top; y < bottom; ++y) {
            const int src_y = y - cached.y;
            const std::size_t dst_off = static_cast<std::size_t>(y) * dst_stride
                + static_cast<std::size_t>(left) * dst_bpp;
            const std::size_t src_off = static_cast<std::size_t>(src_y) * src_stride
                + static_cast<std::size_t>(left - cached.x) * src_bpp;
            const std::size_t bytes = static_cast<std::size_t>(right - left) * dst_bpp;
            std::memcpy(dst + dst_off, src + src_off, bytes);
        }
    }

    static Rect clamp_to_screen(const Rect& r) noexcept {
        const int left = (r.x < 0) ? 0 : r.x;
        const int top = (r.y < 0) ? 0 : r.y;
        const int right = (r.x + r.w > screen_width) ? screen_width : (r.x + r.w);
        const int bottom = (r.y + r.h > screen_height) ? screen_height : (r.y + r.h);
        return Rect{left, top, right - left, bottom - top};
    }

    void mark_cache_dirty_chain(WidgetHandle h) noexcept {
        while (h) {
            auto* obj = factory_.get(h);
            if (!obj) return;
            obj->mark_cache_dirty();
            h = obj->parent();
        }
    }
    void draw_cache_debug(DefaultCanvas& target, const Rect& rect, bool hit) noexcept {
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
