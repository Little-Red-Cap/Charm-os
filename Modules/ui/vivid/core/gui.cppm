module;
#include <cstddef>
#include <cstdint>
#include <cstring>
export module charm.core.gui;

export import charm.gfx.canvas;
export import charm.core.event;
export import charm.core.object;
export import charm.core.handle;
export import charm.core.factory;
export import charm.core.layout;
export import charm.core.input_router;
export import charm.widgets.scroll_container;
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
    }

    void dispatch_touch_down(int id, int x, int y) {
        if (layer_cache_) {
            cache_valid_ = false;
        }
        router_.on_touch_down(id, x, y);
    }

    void dispatch_touch_move(int id, int x, int y) {
        if (layer_cache_) {
            cache_valid_ = false;
        }
        router_.on_touch_move(id, x, y);
    }

    void dispatch_touch_up(int id, int x, int y) {
        if (layer_cache_) {
            cache_valid_ = false;
        }
        router_.on_touch_up(id, x, y);
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
        debug_nodes_ = 0;
        debug_depth_hits_ = 0;
        debug_cycle_hits_ = 0;
        WidgetHandle stack[kMaxDepth]{};
        draw_recursive(target, root_, 0, stack);
        trace_counter(GuiTraceId::FrameNodes, (util::u64)debug_nodes_, 0);
        trace_counter(GuiTraceId::FrameDepthHits, (util::u64)debug_depth_hits_, 0);
        trace_counter(GuiTraceId::FrameCycleHits, (util::u64)debug_cycle_hits_, 0);
    }

    void blit_cache() {
        auto* dst = canvas.raw_buffer().data();
        const auto* src = cache_fb_.data();
        std::memcpy(dst, src, DefaultFrameBuffer::buffer_bytes);
    }

    void draw_recursive(DefaultCanvas& target, WidgetHandle h, int depth, WidgetHandle* stack) {
        if (depth > kMaxDepth) {
            ++debug_depth_hits_;
            return;
        }
        for (int i = 0; i < depth; ++i) {
            if (stack[i] == h) {
                ++debug_cycle_hits_;
                return;
            }
        }
        stack[depth] = h;
        auto* obj = factory_.get(h);
        if (!obj) return;
        if (!obj->is_visible()) return;
        ++debug_nodes_;
        apply_layout(factory_, *obj);
        if (h.kind == WidgetKind::ScrollContainer) {
            if (auto* sc = factory_.get_scroll_container(h)) {
                sc->apply_scroll([&](WidgetHandle ch){ return factory_.get(ch); });
            }
        }
        obj->draw(target);
        if (dirty_tracking_) {
            target.mark_dirty(obj->get_rect());
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
        }

        for (std::size_t i = 0; i < obj->child_count(); ++i) {
            auto ch = obj->child_at(i);
            auto* ch_obj = factory_.get(ch);
            if (!ch_obj) continue;
            if (!obj->should_draw_child(*ch_obj)) continue;
            draw_recursive(target, ch, depth + 1, stack);
        }

        if (clip_children) {
            target.restore_clip(clip_state);
        }
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
    DefaultFrameBuffer cache_fb_{};
    DefaultCanvas cache_canvas_;
    TraceBuffer trace_{};
    InputRouter router_;
};
