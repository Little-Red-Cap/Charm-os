module;
#include <cstddef>
#include <cstdint>
export module charm.core.gui;

export import charm.gfx.canvas;
export import charm.core.event;
export import charm.core.object;
export import charm.core.handle;
export import charm.core.factory;
export import charm.core.layout;
export import charm.widgets.scroll_container;
import service_trace;
import util.core;
import out.api;


export
class Gui {
public:
    Gui(DefaultCanvas& cvs, UiFactory& factory, WidgetHandle root)
        : canvas(cvs), factory_(factory), root_(root) {}

    WidgetHandle focused() const noexcept { return focused_; }
    WidgetHandle hovered() const noexcept { return hovered_; }
    WidgetHandle pressed() const noexcept { return pressed_; }
    WidgetHandle captured() const noexcept { return captured_; }
    bool dragging() const noexcept { return dragging_; }
    void set_dirty_tracking(bool on) noexcept { dirty_tracking_ = on; }
    bool dirty_tracking() const noexcept { return dirty_tracking_; }
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
        debug_nodes_ = 0;
        debug_depth_hits_ = 0;
        debug_cycle_hits_ = 0;
        WidgetHandle stack[kMaxDepth]{};
        draw_recursive(root_, 0, stack);

        trace_counter(GuiTraceId::FrameNodes, (util::u64)debug_nodes_, frame_no);
        trace_counter(GuiTraceId::FrameDepthHits, (util::u64)debug_depth_hits_, frame_no);
        trace_counter(GuiTraceId::FrameCycleHits, (util::u64)debug_cycle_hits_, frame_no);
        frame_no++;
        canvas.end_frame();
    }

    // 派发一个输入事件（全局坐标）
    void dispatch_event(const Event& e) {
        sanitize_handles();
        const WidgetHandle overlay = factory_.overlay();
        auto* overlay_obj = factory_.get(overlay);
        const bool overlay_visible = overlay_obj && overlay_obj->is_visible();

        switch (e.type) {
        case Event::Type::MouseMove: {
            auto target_root = overlay_visible ? overlay : root_;
            auto target = captured_ ? captured_ : hit_test(target_root, e.x, e.y);
            if (target != hovered_) {
                if (hovered_) {
                    dispatch_to(hovered_, Event::mouse(Event::Type::HoverLeave, e.x, e.y, e.button));
                }
                set_state(hovered_, ObjectBase::State::Hovered, false);
                if (target) {
                    dispatch_to(target, Event::mouse(Event::Type::HoverEnter, e.x, e.y, e.button));
                }
                set_state(target, ObjectBase::State::Hovered, true);
                hovered_ = target;
            }
            if (pressed_) {
                const int dx = e.x - drag_last_x_;
                const int dy = e.y - drag_last_y_;
                drag_last_x_ = e.x;
                drag_last_y_ = e.y;
                if (!dragging_) {
                    const int total_dx = e.x - drag_start_x_;
                    const int total_dy = e.y - drag_start_y_;
                    if ((total_dx * total_dx + total_dy * total_dy) >= drag_threshold_sq_) {
                        dragging_ = true;
                        dispatch_to(pressed_, Event::drag(Event::Type::DragStart, e.x, e.y, 0, 0, e.button));
                    }
                }
                if (dragging_) {
                    dispatch_to(pressed_, Event::drag(Event::Type::DragMove, e.x, e.y, dx, dy, e.button));
                } else {
                    dispatch_to(pressed_, e);
                }
            } else if (hovered_) {
                dispatch_to(hovered_, e);
            }
            break;
        }
        case Event::Type::MouseDown: {
            if (overlay_visible) {
                if (dispatch_to(overlay, e)) return;
            }
            auto target_root = overlay_visible ? overlay : root_;
            auto target = hit_test(target_root, e.x, e.y);
            if (target) {
                auto* target_obj = factory_.get(target);
                set_state(pressed_, ObjectBase::State::Pressed, false);
                pressed_ = target;
                captured_ = target;
                set_state(pressed_, ObjectBase::State::Pressed, true);
                dragging_ = false;
                drag_start_x_ = e.x;
                drag_start_y_ = e.y;
                drag_last_x_ = e.x;
                drag_last_y_ = e.y;
                if (target_obj && target_obj->is_focusable()) {
                    set_focus(target);
                }
                dispatch_to(target, e);
            }
            break;
        }
        case Event::Type::MouseUp: {
            if (overlay_visible) {
                if (dispatch_to(overlay, e)) return;
            }
            auto target_root = overlay_visible ? overlay : root_;
            auto target = captured_ ? captured_ : hit_test(target_root, e.x, e.y);
            if (pressed_) {
                const bool was_dragging = dragging_;
                if (dragging_) {
                    dispatch_to(pressed_, Event::drag(Event::Type::DragEnd, e.x, e.y, 0, 0, e.button));
                    dragging_ = false;
                }
                set_state(pressed_, ObjectBase::State::Pressed, false);
                dispatch_to(pressed_, e);
                if (!was_dragging && pressed_ == target) {
                    dispatch_to(pressed_, Event::mouse(Event::Type::Click, e.x, e.y, e.button));
                }
                pressed_ = {};
                captured_ = {};
            } else if (target) {
                dispatch_to(target, e);
            }
            break;
        }
        case Event::Type::MouseWheel: {
            if (overlay_visible) {
                if (dispatch_to(overlay, e)) return;
            }
            auto target_root = overlay_visible ? overlay : root_;
            auto target = captured_ ? captured_ : hit_test(target_root, e.x, e.y);
            if (target) dispatch_to(target, e);
            break;
        }
        case Event::Type::KeyDown:
        case Event::Type::KeyUp: {
            if (overlay_visible) {
                if (dispatch_to(overlay, e)) return;
            }
            if (e.type == Event::Type::KeyDown) {
                if (e.key_code == Event::Key::Tab) {
                    auto next = next_focus(true);
                    if (next) set_focus(next);
                } else if (e.key_code == Event::Key::Up || e.key_code == Event::Key::Left) {
                    auto prev = next_focus(false);
                    if (prev) set_focus(prev);
                } else if (e.key_code == Event::Key::Down || e.key_code == Event::Key::Right) {
                    auto next = next_focus(true);
                    if (next) set_focus(next);
                } else if (e.key_code == Event::Key::Enter || e.key_code == Event::Key::Space) {
                    if (focused_) {
                        dispatch_to(focused_, Event::mouse(Event::Type::Click, 0, 0, 0));
                    }
                }
            }
            if (focused_) dispatch_to(focused_, e);
            break;
        }
        default:
            break;
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

    void trace_counter(GuiTraceId id, util::u64 payload, util::u32 frame) noexcept {
        TraceRecord rec{};
        rec.time = frame;
        rec.id = static_cast<util::u32>(id);
        rec.payload = payload;
        rec.count = 1;
        rec.kind = service::TraceKind::counter;
        trace_.push(rec);
    }

    void sanitize_handles() {
        auto invalid = [&](WidgetHandle h) -> bool {
            auto* obj = factory_.get(h);
            return !obj || !obj->is_visible() || !obj->is_enabled();
        };
        const WidgetHandle ov = factory_.overlay();
        if (ov && invalid(ov)) {
            factory_.clear_overlay(ov);
        }
        if (hovered_ && invalid(hovered_)) {
            set_state(hovered_, ObjectBase::State::Hovered, false);
            hovered_ = {};
        }
        if (pressed_ && invalid(pressed_)) {
            set_state(pressed_, ObjectBase::State::Pressed, false);
            pressed_ = {};
            captured_ = {};
            dragging_ = false;
        }
        if (captured_ && invalid(captured_)) {
            captured_ = {};
            dragging_ = false;
        }
        if (focused_) {
            auto* obj = factory_.get(focused_);
            if (!obj || !obj->is_visible() || !obj->is_enabled() || !obj->is_focusable()) {
                set_focus({});
            }
        }
    }

bool dispatch_to(WidgetHandle target, const Event& e) {
        auto* obj = factory_.get(target);
        if (!obj) return false;
        if (obj->on_event(e)) return true;
        auto p = obj->parent();
        while (p) {
            auto* parent_obj = factory_.get(p);
            if (!parent_obj) break;
            if (parent_obj->on_event(e)) return true;
            p = parent_obj->parent();
        }
        return false;
    }

    void set_state(WidgetHandle h, ObjectBase::State s, bool on) {
        auto* obj = factory_.get(h);
        if (!obj) return;
        obj->set_state(s, on);
    }

    void set_focus(WidgetHandle h) {
        if (focused_ == h) return;
        if (focused_) {
            dispatch_to(focused_, Event::key(Event::Type::FocusOut, Event::Key::Unknown));
        }
        set_state(focused_, ObjectBase::State::Focused, false);
        focused_ = h;
        set_state(focused_, ObjectBase::State::Focused, true);
        if (focused_) {
            dispatch_to(focused_, Event::key(Event::Type::FocusIn, Event::Key::Unknown));
        }
    }

    WidgetHandle next_focus(bool forward) {
        WidgetHandle first{};
        WidgetHandle next{};
        bool found = false;
        auto visit = [&](auto&& self, WidgetHandle h) -> void {
            auto* obj = factory_.get(h);
            if (!obj) return;
            if (!obj->is_visible()) return;
            if (!obj->is_enabled()) return;

            if (obj->is_enabled() && obj->is_focusable()) {
                if (!first) first = h;
                if (forward) {
                    if (found && !next) {
                        next = h;
                    }
                } else {
                    if (!found && h != focused_) {
                        next = h;
                    }
                }
                if (h == focused_) {
                    found = true;
                }
            }

            for (std::size_t i = 0; i < obj->child_count(); ++i) {
                self(self, obj->child_at(i));
            }
        };
        visit(visit, root_);
        const WidgetHandle overlay = factory_.overlay();
        auto* overlay_obj = factory_.get(overlay);
        const bool overlay_visible = overlay_obj && overlay_obj->is_visible();
        if (overlay_visible) {
            visit(visit, overlay);
        }
        if (next) return next;
        return first;
    }

    void draw_recursive(WidgetHandle h, int depth, WidgetHandle* stack) {
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
        switch (obj->layout_mode()) {
        case ObjectBase::LayoutMode::Flex:
            apply_flex_layout(factory_, *obj);
            break;
        case ObjectBase::LayoutMode::Flow:
            apply_flow_layout(factory_, *obj);
            break;
        case ObjectBase::LayoutMode::Grid:
            apply_grid_layout(factory_, *obj);
            break;
        default:
            apply_anchor_layout(factory_, *obj);
            break;
        }
        if (h.kind == WidgetKind::ScrollContainer) {
            if (auto* sc = factory_.get_scroll_container(h)) {
                sc->apply_scroll([&](WidgetHandle ch){ return factory_.get(ch); });
            }
        }
        obj->draw(canvas);
        if (dirty_tracking_) {
            canvas.mark_dirty(obj->get_rect());
        }

        const bool clip_children = (h.kind == WidgetKind::ScrollContainer);
        auto clip_state = canvas.save_clip();
        if (clip_children) {
            canvas.set_clip(obj->get_rect());
        }

        for (std::size_t i = 0; i < obj->child_count(); ++i) {
            auto ch = obj->child_at(i);
            auto* ch_obj = factory_.get(ch);
            if (!ch_obj) continue;
            if (!obj->should_draw_child(*ch_obj)) continue;
            draw_recursive(ch, depth + 1, stack);
        }

        if (clip_children) {
            canvas.restore_clip(clip_state);
        }
    }

    WidgetHandle hit_test(WidgetHandle h, int x, int y, int depth = 0) {
        if (depth > kMaxDepth) {
            ++debug_depth_hits_;
            return {};
        }
        static thread_local WidgetHandle stack[kMaxDepth]{};
        for (int i = 0; i < depth; ++i) {
            if (stack[i] == h) {
                ++debug_cycle_hits_;
                return {};
            }
        }
        stack[depth] = h;
        auto* obj = factory_.get(h);
        if (!obj) return {};
        if (!obj->is_visible()) return {};
        if (!obj->is_enabled()) return {};
        if (h.kind == WidgetKind::ScrollContainer) {
            if (auto* sc = factory_.get_scroll_container(h)) {
                sc->apply_scroll([&](WidgetHandle ch){ return factory_.get(ch); });
            }
        }

        for (std::size_t i = obj->child_count(); i > 0; --i) {
            auto ch = obj->child_at(i - 1);
            auto* ch_obj = factory_.get(ch);
            if (!ch_obj) continue;
            if (!obj->should_draw_child(*ch_obj)) continue;
            auto target = hit_test(ch, x, y, depth + 1);
            if (target) return target;
        }

        if (obj->get_rect().contains(x, y)) {
            return h;
        }
        return {};
    }

    DefaultCanvas& canvas;
    UiFactory& factory_;
    WidgetHandle root_;
    WidgetHandle hovered_{};
    WidgetHandle pressed_{};
    WidgetHandle focused_{};
    WidgetHandle captured_{};
    bool dragging_{false};
    int drag_start_x_{0};
    int drag_start_y_{0};
    int drag_last_x_{0};
    int drag_last_y_{0};
    int drag_threshold_sq_{25};
    static constexpr int kMaxDepth = 128;
    int debug_nodes_{0};
    int debug_depth_hits_{0};
    int debug_cycle_hits_{0};
    bool dirty_tracking_{false};
    TraceBuffer trace_{};
};
