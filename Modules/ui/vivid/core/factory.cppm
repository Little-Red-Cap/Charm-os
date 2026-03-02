module;
#include <cstdint>
#include <cstddef>
#include <optional>
#include <type_traits>
#include <utility>
#include <new>

#include "features.hpp"
export module charm.core.factory.basic;

import charm.core.handle;
import charm.core.object;
import charm.core.container;
import service.handle_pool;
#if CHARM_VIVID_ENABLE_WIDGET_ScrollContainer
import charm.widgets.scroll_container;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Image
import charm.widgets.image;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ImageBox
import charm.widgets.image_box;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Label
import charm.widgets.label;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Button
import charm.widgets.button;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Checkbox
import charm.widgets.checkbox;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Slider
import charm.widgets.slider;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Switch
import charm.widgets.switcher;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Progress
import charm.widgets.progress;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_List
import charm.widgets.list;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ListView
import charm.widgets.list_view;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ScrollBar
import charm.widgets.scrollbar;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextArea
import charm.widgets.text_area;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextInput
import charm.widgets.text_input;
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Radio
import charm.widgets.radio;
#endif

template <typename T, std::size_t N>
using HandlePool = service::HandlePool<T, N>;

export
class UiFactory {
public:
#if CHARM_VIVID_ENABLE_WIDGET_Container
    WidgetHandle create_container() noexcept { return make_handle(containers_.create(), WidgetKind::Container); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ScrollContainer
    WidgetHandle create_scroll_container() noexcept { return make_handle(scrolls_.create(), WidgetKind::ScrollContainer); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Image
    WidgetHandle create_image() noexcept { return make_handle(images_.create(), WidgetKind::Image); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ImageBox
    WidgetHandle create_image_box() noexcept { return make_handle(image_boxes_.create(), WidgetKind::ImageBox); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Label
    WidgetHandle create_label(const char* text) noexcept { return make_handle(labels_.create(text), WidgetKind::Label); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Button
    WidgetHandle create_button(const char* text) noexcept { return make_handle(buttons_.create(text), WidgetKind::Button); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Checkbox
    WidgetHandle create_checkbox(const char* text) noexcept { return make_handle(checkboxes_.create(text), WidgetKind::Checkbox); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Slider
    WidgetHandle create_slider() noexcept { return make_handle(sliders_.create(), WidgetKind::Slider); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Switch
    WidgetHandle create_switch() noexcept { return make_handle(switches_.create(), WidgetKind::Switch); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Progress
    WidgetHandle create_progress() noexcept { return make_handle(progresses_.create(), WidgetKind::Progress); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_List
    WidgetHandle create_list() noexcept { return make_handle(lists_.create(), WidgetKind::List); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ListItem && CHARM_VIVID_ENABLE_WIDGET_List
    WidgetHandle create_list_item(const char* text) noexcept { return make_handle(list_items_.create(text), WidgetKind::ListItem); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ListView
    WidgetHandle create_list_view() noexcept { return make_handle(list_views_.create(), WidgetKind::ListView); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ScrollBar
    WidgetHandle create_scroll_bar() noexcept { return make_handle(scroll_bars_.create(), WidgetKind::ScrollBar); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextArea
    WidgetHandle create_text_area(const char* text) noexcept { return make_handle(text_areas_.create(text), WidgetKind::TextArea); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextInput
    WidgetHandle create_text_input(const char* text) noexcept {
        auto handle = make_handle(text_inputs_.create(), WidgetKind::TextInput);
        if (text && handle) {
            if (auto* ti = get_text_input(handle)) {
                ti->set_text(text);
            }
        }
        return handle;
    }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Radio
    WidgetHandle create_radio(const char* text) noexcept { return make_handle(radios_.create(text), WidgetKind::Radio); }
#endif
    void set_overlay(WidgetHandle h) noexcept { overlay_ = h; }
    void clear_overlay(WidgetHandle h) noexcept { if (overlay_ == h) overlay_ = {}; }
    WidgetHandle overlay() const noexcept { return overlay_; }

#if CHARM_VIVID_ENABLE_WIDGET_Container
    Container* get_container(const WidgetHandle& h) noexcept { return get_from(containers_, h, WidgetKind::Container); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ScrollContainer
    ScrollContainer* get_scroll_container(const WidgetHandle& h) noexcept { return get_from(scrolls_, h, WidgetKind::ScrollContainer); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Image
    Image* get_image(const WidgetHandle& h) noexcept { return get_from(images_, h, WidgetKind::Image); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ImageBox
    ImageBox* get_image_box(const WidgetHandle& h) noexcept { return get_from(image_boxes_, h, WidgetKind::ImageBox); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Label
    Label* get_label(const WidgetHandle& h) noexcept { return get_from(labels_, h, WidgetKind::Label); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Button
    Button* get_button(const WidgetHandle& h) noexcept { return get_from(buttons_, h, WidgetKind::Button); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Checkbox
    Checkbox* get_checkbox(const WidgetHandle& h) noexcept { return get_from(checkboxes_, h, WidgetKind::Checkbox); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Slider
    Slider* get_slider(const WidgetHandle& h) noexcept { return get_from(sliders_, h, WidgetKind::Slider); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Switch
    Switch* get_switch(const WidgetHandle& h) noexcept { return get_from(switches_, h, WidgetKind::Switch); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Progress
    Progress* get_progress(const WidgetHandle& h) noexcept { return get_from(progresses_, h, WidgetKind::Progress); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_List
    List* get_list(const WidgetHandle& h) noexcept { return get_from(lists_, h, WidgetKind::List); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ListItem && CHARM_VIVID_ENABLE_WIDGET_List
    ListItem* get_list_item(const WidgetHandle& h) noexcept { return get_from(list_items_, h, WidgetKind::ListItem); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ListView
    ListView* get_list_view(const WidgetHandle& h) noexcept { return get_from(list_views_, h, WidgetKind::ListView); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ScrollBar
    ScrollBar* get_scroll_bar(const WidgetHandle& h) noexcept { return get_from(scroll_bars_, h, WidgetKind::ScrollBar); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextArea
    TextArea* get_text_area(const WidgetHandle& h) noexcept { return get_from(text_areas_, h, WidgetKind::TextArea); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextInput
    TextInput* get_text_input(const WidgetHandle& h) noexcept { return get_from(text_inputs_, h, WidgetKind::TextInput); }
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Radio
    Radio* get_radio(const WidgetHandle& h) noexcept { return get_from(radios_, h, WidgetKind::Radio); }
#endif

    ObjectBase* get(const WidgetHandle& h) noexcept {
        switch (h.kind) {
#if CHARM_VIVID_ENABLE_WIDGET_Container
            case WidgetKind::Container: return get_container(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ScrollContainer
            case WidgetKind::ScrollContainer: return get_scroll_container(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Image
            case WidgetKind::Image: return get_image(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ImageBox
            case WidgetKind::ImageBox: return get_image_box(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Label
            case WidgetKind::Label: return get_label(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Button
            case WidgetKind::Button: return get_button(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Checkbox
            case WidgetKind::Checkbox: return get_checkbox(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Slider
            case WidgetKind::Slider: return get_slider(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Switch
            case WidgetKind::Switch: return get_switch(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Progress
            case WidgetKind::Progress: return get_progress(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_List
            case WidgetKind::List: return get_list(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ListItem && CHARM_VIVID_ENABLE_WIDGET_List
            case WidgetKind::ListItem: return get_list_item(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ListView
            case WidgetKind::ListView: return get_list_view(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ScrollBar
            case WidgetKind::ScrollBar: return get_scroll_bar(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextArea
            case WidgetKind::TextArea: return get_text_area(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextInput
            case WidgetKind::TextInput: return get_text_input(h);
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Radio
            case WidgetKind::Radio: return get_radio(h);
#endif
            default: return nullptr;
        }
    }

    void detach_from_parent_and_children(const WidgetHandle& h) noexcept {
        auto* obj = get(h);
        if (!obj) return;
        auto parent = obj->parent();
        if (parent) {
            if (auto* p = get(parent)) {
                p->remove_child(h);
            }
            obj->set_parent({});
        }
        const std::size_t count = obj->child_count();
        for (std::size_t i = 0; i < count; ++i) {
            auto child = obj->child_at(i);
            if (auto* c = get(child)) {
                if (c->parent() == h) c->set_parent({});
            }
        }
        obj->clear_children();
    }

    bool link(const WidgetHandle& parent, const WidgetHandle& child) noexcept {
        auto* p = get(parent);
        auto* c = get(child);
        if (!p || !c) return false;
        if (parent == child) return false;
        if (creates_cycle(parent, child)) return false;
        if (c->parent() && c->parent() != parent) {
            if (auto* old = get(c->parent())) {
                old->remove_child(child);
            }
        }
        if (p->has_child(child)) {
            c->set_parent(parent);
            return true;
        }
        if (!p->add_child(child)) return false;
        c->set_parent(parent);
        return true;
    }

    bool insert_before(const WidgetHandle& parent, const WidgetHandle& child, const WidgetHandle& before) noexcept {
        auto* p = get(parent);
        auto* c = get(child);
        if (!p || !c) return false;
        if (parent == child) return false;
        if (creates_cycle(parent, child)) return false;
        if (c->parent() && c->parent() != parent) {
            if (auto* old = get(c->parent())) {
                old->remove_child(child);
            }
        }
        if (p->has_child(child)) {
            p->remove_child(child);
        }
        if (!p->insert_child_before(child, before)) return false;
        c->set_parent(parent);
        return true;
    }

    bool insert_after(const WidgetHandle& parent, const WidgetHandle& child, const WidgetHandle& after) noexcept {
        auto* p = get(parent);
        auto* c = get(child);
        if (!p || !c) return false;
        if (parent == child) return false;
        if (creates_cycle(parent, child)) return false;
        if (c->parent() && c->parent() != parent) {
            if (auto* old = get(c->parent())) {
                old->remove_child(child);
            }
        }
        if (p->has_child(child)) {
            p->remove_child(child);
        }
        if (!p->insert_child_after(child, after)) return false;
        c->set_parent(parent);
        return true;
    }

    bool unlink(const WidgetHandle& parent, const WidgetHandle& child) noexcept {
        auto* p = get(parent);
        auto* c = get(child);
        if (!p || !c) return false;
        if (!p->remove_child(child)) return false;
        if (c->parent() == parent) c->set_parent({});
        return true;
    }

    void unlink_all_children(const WidgetHandle& parent) noexcept {
        auto* p = get(parent);
        if (!p) return;
        while (p->child_count() > 0) {
            auto child = p->child_at(0);
            auto* c = get(child);
            p->remove_child(child);
            if (c && c->parent() == parent) c->set_parent({});
        }
    }

    bool bring_to_front(const WidgetHandle& parent, const WidgetHandle& child) noexcept {
        auto* p = get(parent);
        if (!p) return false;
        return p->move_child_to_front(child);
    }

    bool send_to_back(const WidgetHandle& parent, const WidgetHandle& child) noexcept {
        auto* p = get(parent);
        if (!p) return false;
        return p->move_child_to_back(child);
    }

    template<typename Fn>
    void for_each_child(const WidgetHandle& parent, Fn&& fn) noexcept {
        auto* p = get(parent);
        if (!p) return;
        for (std::size_t i = 0; i < p->child_count(); ++i) {
            fn(p->child_at(i));
        }
    }

    template<typename Fn>
    void for_each_child_reverse(const WidgetHandle& parent, Fn&& fn) noexcept {
        auto* p = get(parent);
        if (!p) return;
        for (std::size_t i = p->child_count(); i > 0; --i) {
            fn(p->child_at(i - 1));
        }
    }

    std::size_t child_index(const WidgetHandle& parent, const WidgetHandle& child) noexcept {
        auto* p = get(parent);
        if (!p) return 0;
        return p->child_index(child);
    }

    template<typename Fn>
    void traverse(const WidgetHandle& root, Fn&& fn) noexcept {
        auto* obj = get(root);
        if (!obj) return;
        fn(root, *obj);
        for (std::size_t i = 0; i < obj->child_count(); ++i) {
            traverse(obj->child_at(i), std::forward<Fn>(fn));
        }
    }

    void destroy(const WidgetHandle& h) noexcept {
        detach_from_parent_and_children(h);
        switch (h.kind) {
        #if CHARM_VIVID_ENABLE_WIDGET_Container
        case WidgetKind::Container: destroy_from(containers_, h, WidgetKind::Container); break;
        #endif
        #if CHARM_VIVID_ENABLE_WIDGET_ScrollContainer
        case WidgetKind::ScrollContainer: destroy_from(scrolls_, h, WidgetKind::ScrollContainer); break;
        #endif
        #if CHARM_VIVID_ENABLE_WIDGET_Image
        case WidgetKind::Image: destroy_from(images_, h, WidgetKind::Image); break;
        #endif
        #if CHARM_VIVID_ENABLE_WIDGET_ImageBox
        case WidgetKind::ImageBox: destroy_from(image_boxes_, h, WidgetKind::ImageBox); break;
        #endif
        #if CHARM_VIVID_ENABLE_WIDGET_Label
        case WidgetKind::Label: destroy_from(labels_, h, WidgetKind::Label); break;
        #endif
        #if CHARM_VIVID_ENABLE_WIDGET_Button
        case WidgetKind::Button: destroy_from(buttons_, h, WidgetKind::Button); break;
        #endif
        #if CHARM_VIVID_ENABLE_WIDGET_Checkbox
        case WidgetKind::Checkbox: destroy_from(checkboxes_, h, WidgetKind::Checkbox); break;
        #endif
        #if CHARM_VIVID_ENABLE_WIDGET_Slider
        case WidgetKind::Slider: destroy_from(sliders_, h, WidgetKind::Slider); break;
        #endif
        #if CHARM_VIVID_ENABLE_WIDGET_Switch
        case WidgetKind::Switch: destroy_from(switches_, h, WidgetKind::Switch); break;
        #endif
        #if CHARM_VIVID_ENABLE_WIDGET_Progress
        case WidgetKind::Progress: destroy_from(progresses_, h, WidgetKind::Progress); break;
        #endif
        #if CHARM_VIVID_ENABLE_WIDGET_List
        case WidgetKind::List: destroy_from(lists_, h, WidgetKind::List); break;
        #endif
        #if CHARM_VIVID_ENABLE_WIDGET_ListItem && CHARM_VIVID_ENABLE_WIDGET_List
        case WidgetKind::ListItem: destroy_from(list_items_, h, WidgetKind::ListItem); break;
        #endif
        #if CHARM_VIVID_ENABLE_WIDGET_ListView
        case WidgetKind::ListView: destroy_from(list_views_, h, WidgetKind::ListView); break;
        #endif
        #if CHARM_VIVID_ENABLE_WIDGET_ScrollBar
        case WidgetKind::ScrollBar: destroy_from(scroll_bars_, h, WidgetKind::ScrollBar); break;
        #endif
        #if CHARM_VIVID_ENABLE_WIDGET_TextArea
        case WidgetKind::TextArea: destroy_from(text_areas_, h, WidgetKind::TextArea); break;
        #endif
        #if CHARM_VIVID_ENABLE_WIDGET_TextInput
        case WidgetKind::TextInput: destroy_from(text_inputs_, h, WidgetKind::TextInput); break;
        #endif
        #if CHARM_VIVID_ENABLE_WIDGET_Radio
        case WidgetKind::Radio: destroy_from(radios_, h, WidgetKind::Radio); break;
        #endif
        default: break;
        }
        if (overlay_ == h) overlay_ = {};
    }

    void destroy_tree(const WidgetHandle& h) noexcept {
        auto* obj = get(h);
        if (!obj) return;
        while (obj->child_count() > 0) {
            auto child = obj->child_at(obj->child_count() - 1);
            destroy_tree(child);
            obj = get(h);
            if (!obj) break;
        }
        destroy(h);
    }

    struct TreeSanitizeReport {
        int removed{};
        int missing{};
        int self_ref{};
        int invalid_parent{};
        int cycle{};
        WidgetHandle last_parent{};
        WidgetHandle last_child{};
    };

    struct TreeValidateReport {
        int issues{};
        int missing{};
        int self_ref{};
        int invalid_parent{};
        int cycle{};
        WidgetHandle last_parent{};
        WidgetHandle last_child{};
    };

    const TreeSanitizeReport& last_sanitize_report() const noexcept {
        return last_report_;
    }

    TreeValidateReport validate_tree(const WidgetHandle& root) const noexcept {
        TreeValidateReport rep{};
        struct Frame { WidgetHandle h; std::size_t idx; };
        constexpr int kMaxDepth = 128;
        Frame stack[kMaxDepth]{};
        WidgetHandle path[kMaxDepth]{};
        int depth = 0;
        if (!root) return rep;
        stack[0] = {root, 0};
        path[0] = root;
        depth = 1;

        auto in_path = [&](WidgetHandle h) -> bool {
            for (int i = 0; i < depth; ++i) {
                if (path[i] == h) return true;
            }
            return false;
        };

        while (depth > 0) {
            auto& frame = stack[depth - 1];
            auto* obj = const_cast<UiFactory*>(this)->get(frame.h);
            if (!obj) {
                --depth;
                continue;
            }

            if (frame.idx >= obj->child_count()) {
                --depth;
                continue;
            }

            auto child = obj->child_at(frame.idx);
            ++frame.idx;

            auto* ch = const_cast<UiFactory*>(this)->get(child);
            if (!ch) {
                ++rep.issues; ++rep.missing;
                rep.last_parent = frame.h; rep.last_child = child;
                continue;
            }
            if (child == frame.h) {
                ++rep.issues; ++rep.self_ref;
                rep.last_parent = frame.h; rep.last_child = child;
                continue;
            }
            if (ch->parent() != frame.h) {
                ++rep.issues; ++rep.invalid_parent;
                rep.last_parent = frame.h; rep.last_child = child;
                continue;
            }
            if (in_path(child)) {
                ++rep.issues; ++rep.cycle;
                rep.last_parent = frame.h; rep.last_child = child;
                continue;
            }

            if (depth < kMaxDepth) {
                stack[depth] = {child, 0};
                path[depth] = child;
                ++depth;
            }
        }
        return rep;
    }

    void sanitize_tree(const WidgetHandle& root) noexcept {
        last_report_ = {};
        struct Frame { WidgetHandle h; std::size_t idx; };
        constexpr int kMaxDepth = 128;
        Frame stack[kMaxDepth]{};
        WidgetHandle path[kMaxDepth]{};
        int depth = 0;
        if (!root) return;
        stack[0] = {root, 0};
        path[0] = root;
        depth = 1;

        auto in_path = [&](WidgetHandle h) -> bool {
            for (int i = 0; i < depth; ++i) {
                if (path[i] == h) return true;
            }
            return false;
        };

        while (depth > 0) {
            auto& frame = stack[depth - 1];
            auto* obj = get(frame.h);
            if (!obj) {
                --depth;
                continue;
            }

            if (frame.idx >= obj->child_count()) {
                --depth;
                continue;
            }

            const std::size_t idx = frame.idx;
            auto child = obj->child_at(idx);
            ++frame.idx;

            auto* ch = get(child);
            if (!ch) {
                record_remove(frame.h, child, RemoveReason::Missing);
                obj->remove_child(child);
                frame.idx = (idx > 0) ? idx - 1 : 0;
                continue;
            }
            if (child == frame.h) {
                record_remove(frame.h, child, RemoveReason::Self);
                obj->remove_child(child);
                frame.idx = (idx > 0) ? idx - 1 : 0;
                continue;
            }
            if (ch->parent() != frame.h) {
                record_remove(frame.h, child, RemoveReason::InvalidParent);
                obj->remove_child(child);
                frame.idx = (idx > 0) ? idx - 1 : 0;
                continue;
            }
            if (in_path(child)) {
                record_remove(frame.h, child, RemoveReason::Cycle);
                obj->remove_child(child);
                frame.idx = (idx > 0) ? idx - 1 : 0;
                continue;
            }

            if (depth < kMaxDepth) {
                stack[depth] = {child, 0};
                path[depth] = child;
                ++depth;
            }
        }
    }

private:
    template <typename Handle>
    static WidgetHandle make_handle(std::optional<Handle> h, WidgetKind kind) noexcept {
        if (!h) return {};
        return WidgetHandle{kind, h->index, h->generation};
    }

    template <typename Pool>
    static typename Pool::Handle to_handle(const WidgetHandle& h) noexcept {
        return typename Pool::Handle{h.index, h.generation};
    }

    template <typename Pool>
    static auto get_from(Pool& pool, const WidgetHandle& h, WidgetKind kind) noexcept
        -> decltype(pool.get(typename Pool::Handle{})) {
        if (h.kind != kind) return nullptr;
        return pool.get(to_handle<Pool>(h));
    }

    template <typename Pool>
    static void destroy_from(Pool& pool, const WidgetHandle& h, WidgetKind kind) noexcept {
        if (h.kind != kind) return;
        pool.destroy(to_handle<Pool>(h));
    }

    enum class RemoveReason {
        Missing,
        Self,
        InvalidParent,
        Cycle
    };

    void record_remove(WidgetHandle parent, WidgetHandle child, RemoveReason reason) noexcept {
        ++last_report_.removed;
        last_report_.last_parent = parent;
        last_report_.last_child = child;
        switch (reason) {
        case RemoveReason::Missing: ++last_report_.missing; break;
        case RemoveReason::Self: ++last_report_.self_ref; break;
        case RemoveReason::InvalidParent: ++last_report_.invalid_parent; break;
        case RemoveReason::Cycle: ++last_report_.cycle; break;
        }
    }

    bool creates_cycle(const WidgetHandle& parent, const WidgetHandle& child) noexcept {
        // If parent is inside child's ancestor chain, linking would create a cycle.
        auto* obj = get(parent);
        while (obj) {
            const auto p = obj->parent();
            if (!p) break;
            if (p == child) return true;
            obj = get(p);
        }
        return false;
    }

#if CHARM_VIVID_ENABLE_WIDGET_Container
    HandlePool<Container, 16> containers_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ScrollContainer
    HandlePool<ScrollContainer, 16> scrolls_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Image
    HandlePool<Image, 32> images_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ImageBox
    HandlePool<ImageBox, 16> image_boxes_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Label
    HandlePool<Label, 128> labels_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Button
    HandlePool<Button, 64> buttons_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Checkbox
    HandlePool<Checkbox, 64> checkboxes_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Slider
    HandlePool<Slider, 64> sliders_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Switch
    HandlePool<Switch, 64> switches_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Progress
    HandlePool<Progress, 64> progresses_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_List
    HandlePool<List, 32> lists_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ListItem && CHARM_VIVID_ENABLE_WIDGET_List
    HandlePool<ListItem, 128> list_items_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ListView
    HandlePool<ListView, 16> list_views_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ScrollBar
    HandlePool<ScrollBar, 32> scroll_bars_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextArea
    HandlePool<TextArea, 32> text_areas_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextInput
    HandlePool<TextInput, 64> text_inputs_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Radio
    HandlePool<Radio, 64> radios_{};
#endif
    WidgetHandle overlay_{};
    TreeSanitizeReport last_report_{};
};
