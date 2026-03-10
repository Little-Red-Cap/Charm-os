module;
#include <cstdint>
#include <cstddef>
#include <optional>
#include <type_traits>
#include <utility>
#include <new>
export module charm.core.factory.full;

#include "features.hpp"

import charm.core.handle;
import charm.core.object;
import charm.core.container;
import service.handle_pool;
#define VIVID_WIDGET_REGISTRY(name, module_tag, cpp_type, theme_base_kind, factory_fn, factory_pool, factory_create, payload_on, payload_kind, style_mask, hit_test_false, focusable, clip_children, layout_list, click_on, click_behavior, click_index, group_kind, checkable, scroll_on, wheel_target, drag_behavior, drag_on, drag_behavior_only, wheel_on, wheel_target_only, extra_on, scroll_axis, wheel_axis, capture_on) \
    import module_tag;
#include "widgets.registry.def"
#undef VIVID_WIDGET_REGISTRY

template <typename T, std::size_t N>
using HandlePool = service::HandlePool<T, N>;

export
class UiFactory {
public:
    #define VIVID_FACTORY_CREATE_None(name, fn, pool) \
        WidgetHandle create_##fn() noexcept { return make_handle(pool.create(), WidgetKind::name); }
    #define VIVID_FACTORY_CREATE_Text(name, fn, pool) \
        WidgetHandle create_##fn(const char* text) noexcept { return make_handle(pool.create(text), WidgetKind::name); }
    #define VIVID_WIDGET_REGISTRY(name, module_tag, cpp_type, theme_base_kind, factory_fn, factory_pool, factory_create, payload_on, payload_kind, style_mask, hit_test_false, focusable, clip_children, layout_list, click_on, click_behavior, click_index, group_kind, checkable, scroll_on, wheel_target, drag_behavior, drag_on, drag_behavior_only, wheel_on, wheel_target_only, extra_on, scroll_axis, wheel_axis, capture_on) \
        VIVID_FACTORY_CREATE_##factory_create(name, factory_fn, factory_pool)
    #include "widgets.registry.def"
    #undef VIVID_WIDGET_REGISTRY
    #undef VIVID_FACTORY_CREATE_Text
    #undef VIVID_FACTORY_CREATE_None

    void set_overlay(WidgetHandle h) noexcept { overlay_ = h; }
    void clear_overlay(WidgetHandle h) noexcept { if (overlay_ == h) overlay_ = {}; }
    WidgetHandle overlay() const noexcept { return overlay_; }

    #define VIVID_WIDGET_REGISTRY(name, module_tag, cpp_type, theme_base_kind, factory_fn, factory_pool, factory_create, payload_on, payload_kind, style_mask, hit_test_false, focusable, clip_children, layout_list, click_on, click_behavior, click_index, group_kind, checkable, scroll_on, wheel_target, drag_behavior, drag_on, drag_behavior_only, wheel_on, wheel_target_only, extra_on, scroll_axis, wheel_axis, capture_on) \
        cpp_type* get_##factory_fn(const WidgetHandle& h) noexcept { return get_from(factory_pool, h, WidgetKind::name); }
    #include "widgets.registry.def"
    #undef VIVID_WIDGET_REGISTRY

    ObjectBase* get(const WidgetHandle& h) noexcept {
        switch (h.kind) {
    #define VIVID_WIDGET_REGISTRY(name, module_tag, cpp_type, theme_base_kind, factory_fn, factory_pool, factory_create, payload_on, payload_kind, style_mask, hit_test_false, focusable, clip_children, layout_list, click_on, click_behavior, click_index, group_kind, checkable, scroll_on, wheel_target, drag_behavior, drag_on, drag_behavior_only, wheel_on, wheel_target_only, extra_on, scroll_axis, wheel_axis, capture_on) \
            case WidgetKind::name: return get_##factory_fn(h);
    #include "widgets.registry.def"
    #undef VIVID_WIDGET_REGISTRY
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
    #define VIVID_WIDGET_REGISTRY(name, module_tag, cpp_type, theme_base_kind, factory_fn, factory_pool, factory_create, payload_on, payload_kind, style_mask, hit_test_false, focusable, clip_children, layout_list, click_on, click_behavior, click_index, group_kind, checkable, scroll_on, wheel_target, drag_behavior, drag_on, drag_behavior_only, wheel_on, wheel_target_only, extra_on, scroll_axis, wheel_axis, capture_on) \
            case WidgetKind::name: destroy_from(factory_pool, h, WidgetKind::name); break;
    #include "widgets.registry.def"
    #undef VIVID_WIDGET_REGISTRY
        default: break;
        }
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

    HandlePool<Container, 16> containers_{};
#if CHARM_VIVID_ENABLE_WIDGET_ScrollContainer
    HandlePool<ScrollContainer, 16> scrolls_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Dial
    HandlePool<Dial, 16> dials_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Arc
    HandlePool<Arc, 16> arcs_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Image
    HandlePool<Image, 32> images_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ImageBox
    HandlePool<ImageBox, 16> image_boxes_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_BusyWheel
    HandlePool<BusyWheel, 16> busy_wheels_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ConsoleBox
    HandlePool<ConsoleBox, 8> console_boxes_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_BatteryGasGauge
    HandlePool<BatteryGasGauge, 16> battery_gasgauge_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Histogram
    HandlePool<Histogram, 16> histogram_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Label
    HandlePool<Label, 128> labels_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Button
    HandlePool<Button, 64> buttons_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Led
    HandlePool<Led, 32> leds_{};
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
#if CHARM_VIVID_ENABLE_WIDGET_ProgressBarRound
    HandlePool<ProgressBarRound, 16> progress_round_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressBarSimple
    HandlePool<ProgressBarSimple, 32> progress_simple_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressBarDrill
    HandlePool<ProgressBarDrill, 16> progress_drill_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_List
    HandlePool<List, 32> lists_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ListItem
    HandlePool<ListItem, 128> list_items_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ListView
    HandlePool<ListView, 16> list_views_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_IconList
    HandlePool<IconList, 16> icon_lists_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextTrackingList
    HandlePool<TextTrackingList, 16> text_tracking_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextList
    HandlePool<TextList, 16> text_lists_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ModalDialog
    HandlePool<ModalDialog, 8> modals_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ScrollBar
    HandlePool<ScrollBar, 32> scroll_bars_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_SegmentedControl
    HandlePool<SegmentedControl, 32> segments_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextArea
    HandlePool<TextArea, 32> text_areas_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextInput
    HandlePool<TextInput, 64> text_inputs_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_NumberInput
    HandlePool<NumberInput, 32> number_inputs_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_NumberList
    HandlePool<NumberList, 16> number_lists_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ToggleGroup
    HandlePool<ToggleGroup, 32> toggles_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TableView
    HandlePool<TableView, 8> tables_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TreeView
    HandlePool<TreeView, 8> trees_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Dropdown
    HandlePool<Dropdown, 64> dropdowns_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TabView
    HandlePool<TabView, 16> tabviews_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Roller
    HandlePool<Roller, 32> rollers_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Spinner
    HandlePool<Spinner, 32> spinners_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Bar
    HandlePool<Bar, 64> bars_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_PopupLayer
    HandlePool<PopupLayer, 8> popups_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_MessageBox
    HandlePool<MessageBox, 8> message_boxes_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Menu
    HandlePool<Menu, 16> menus_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_MenuItem
    HandlePool<MenuItem, 64> menu_items_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Radio
    HandlePool<Radio, 64> radios_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_RadioGroup
    HandlePool<RadioGroup, 16> radio_groups_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Chart
    HandlePool<Chart, 16> charts_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Waveform
    HandlePool<Waveform, 8> waveforms_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Gauge
    HandlePool<Gauge, 16> gauges_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_MeterPointer
    HandlePool<MeterPointer, 16> meters_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_PrimitivesCanvas
    HandlePool<PrimitivesCanvas, 8> prim_canvas_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_PerfOverlay
    HandlePool<PerfOverlay, 8> perf_overlays_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Stepper
    HandlePool<Stepper, 16> steppers_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_Timeline
    HandlePool<Timeline, 16> timelines_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_RichText
    HandlePool<RichText, 16> rich_texts_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_CodeBlock
    HandlePool<CodeBlock, 16> code_blocks_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressWheel
    HandlePool<ProgressWheel, 16> progress_wheels_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_WaveformView
    HandlePool<WaveformView, 16> waveform_views_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_BatteryGauge
    HandlePool<BatteryGauge, 16> batteries_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_HistogramView
    HandlePool<HistogramView, 16> histograms_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_RingIndication
    HandlePool<RingIndication, 16> rings_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_TextBox
    HandlePool<TextBox, 32> text_boxes_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_FoldablePanel
    HandlePool<FoldablePanel, 16> fold_panels_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_ProgressFlowing
    HandlePool<ProgressFlowing, 16> progress_flow_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_CloudyGlass
    HandlePool<CloudyGlass, 16> glass_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_SpinZoomWidget
    HandlePool<SpinZoomWidget, 8> spin_zoom_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_SpinningWheel
    HandlePool<SpinningWheel, 16> spinning_wheel_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_DynamicNebula
    HandlePool<DynamicNebula, 8> nebula_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_CrtScreen
    HandlePool<CrtScreen, 8> crt_{};
#endif
#if CHARM_VIVID_ENABLE_WIDGET_SpectrumView
    HandlePool<SpectrumView, 16> spectrum_views_{};
#endif
    WidgetHandle overlay_{};
    TreeSanitizeReport last_report_{};
};
