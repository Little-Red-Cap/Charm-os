module;
#include <cstddef>
export module charm.core.input_router;

export import charm.core.event;
export import charm.core.object;
export import charm.core.handle;
export import charm.core.factory;
export import charm.widgets.scroll_container;

export
class InputRouter {
public:
    explicit InputRouter(UiFactory& factory, WidgetHandle root)
        : factory_(factory), root_(root) {}

    void set_root(WidgetHandle root) noexcept { root_ = root; }

    WidgetHandle focused() const noexcept { return focused_; }
    WidgetHandle hovered() const noexcept { return hovered_; }
    WidgetHandle pressed() const noexcept { return pressed_; }
    WidgetHandle captured() const noexcept { return captured_; }
    bool dragging() const noexcept { return dragging_; }

    void set_drag_threshold(int px) noexcept { drag_threshold_sq_ = px * px; }

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
                        begin_swipe(e.x, e.y);
                    }
                }
                if (dragging_) {
                    dispatch_to(pressed_, Event::drag(Event::Type::DragMove, e.x, e.y, dx, dy, e.button));
                    update_swipe(e.x, e.y);
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
                swipe_active_ = false;
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
                end_swipe(e.x, e.y);
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
            swipe_active_ = false;
        }
        if (captured_ && invalid(captured_)) {
            captured_ = {};
            dragging_ = false;
            swipe_active_ = false;
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

    void begin_swipe(int x, int y) {
        if (swipe_active_) return;
        swipe_active_ = true;
        swipe_last_x_ = x;
        swipe_last_y_ = y;
        dispatch_to(pressed_, Event::gesture(Event::Type::GestureSwipe, x, y, 0, 0,
                                             Event::GesturePhase::Begin, 1.0f));
    }

    void update_swipe(int x, int y) {
        if (!swipe_active_) return;
        const int dx = x - swipe_last_x_;
        const int dy = y - swipe_last_y_;
        swipe_last_x_ = x;
        swipe_last_y_ = y;
        dispatch_to(pressed_, Event::gesture(Event::Type::GestureSwipe, x, y, dx, dy,
                                             Event::GesturePhase::Update, 1.0f));
    }

    void end_swipe(int x, int y) {
        if (!swipe_active_) return;
        dispatch_to(pressed_, Event::gesture(Event::Type::GestureSwipe, x, y, 0, 0,
                                             Event::GesturePhase::End, 1.0f));
        swipe_active_ = false;
    }

    WidgetHandle hit_test(WidgetHandle h, int x, int y, int depth = 0) {
        if (depth > kMaxDepth) {
            return {};
        }
        static thread_local WidgetHandle stack[kMaxDepth]{};
        for (int i = 0; i < depth; ++i) {
            if (stack[i] == h) {
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
    bool swipe_active_{false};
    int swipe_last_x_{0};
    int swipe_last_y_{0};
    static constexpr int kMaxDepth = 128;
};
