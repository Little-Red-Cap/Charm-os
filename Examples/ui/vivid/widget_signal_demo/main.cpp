#include <cstdint>
#include <cstdio>

import charm.core;
import charm.core.event;
import charm.core.input_interaction;
import charm.core.object;
import charm.widgets.button;
import charm.widgets.foldable_panel;
import charm.widgets.image;
import charm.widgets.list;
import charm.widgets.menu_item;
import charm.widgets.scroll_container;
import charm.widgets.spin_zoom_widget;

namespace {
    template<typename T>
    concept OwnsObjectChildren = requires(T& object, WidgetHandle child) {
        object.add_child(child);
        object.remove_child(child);
        object.child_count();
        object.child_at(0);
    };

    static_assert(!OwnsObjectChildren<Button>);
    static_assert(OwnsObjectChildren<List>);
    static_assert(OwnsObjectChildren<FoldablePanel>);
    static_assert(OwnsObjectChildren<ScrollContainer>);

    struct Probe {
        int primary_clicks{0};
        int secondary_clicks{0};
        int menu_clicks{0};
        int list_clicks{0};
        int list_secondary_clicks{0};

        void on_primary_click() noexcept {
            ++primary_clicks;
        }

        void on_secondary_click() noexcept {
            ++secondary_clicks;
        }

        void on_menu_click() noexcept {
            ++menu_clicks;
        }

        void on_list_click() noexcept {
            ++list_clicks;
        }

        void on_list_secondary_click() noexcept {
            ++list_secondary_clicks;
        }
    };

    struct InteractionProbe {
        int sequence{0};
        int double_taps{0};
        int pinch_begin_order{0};
        int pinch_update_order{0};
        int pinch_end_order{0};
        int pinch_dy{0};
        int drag_begin_order{0};
        int drag_update_order{0};
        int drag_end_order{0};
        int drag_x{0};
        int drag_y{0};
        int drag_dx{0};
        int drag_dy{0};
        int long_presses{0};

        void on_double_tap() noexcept {
            ++double_taps;
        }

        void on_pinch_begin() noexcept {
            pinch_begin_order = ++sequence;
        }

        void on_pinch_update(int dy) noexcept {
            pinch_update_order = ++sequence;
            pinch_dy = dy;
        }

        void on_pinch_end() noexcept {
            pinch_end_order = ++sequence;
        }

        void on_drag_begin(int x, int y) noexcept {
            drag_begin_order = ++sequence;
            drag_x = x;
            drag_y = y;
        }

        void on_drag_update(int x, int y, int dx, int dy) noexcept {
            drag_update_order = ++sequence;
            drag_x = x;
            drag_y = y;
            drag_dx = dx;
            drag_dy = dy;
        }

        void on_drag_end(int x, int y) noexcept {
            drag_end_order = ++sequence;
            drag_x = x;
            drag_y = y;
        }

        void on_long_press() noexcept {
            ++long_presses;
        }
    };

    int g_button_legacy_clicks = 0;
    int g_menu_legacy_clicks = 0;
    int g_list_legacy_clicks = 0;

    void on_button_legacy_click() noexcept {
        ++g_button_legacy_clicks;
    }

    void on_menu_legacy_click() noexcept {
        ++g_menu_legacy_clicks;
    }

    void on_list_legacy_click() noexcept {
        ++g_list_legacy_clicks;
    }

    [[nodiscard]] bool expect(bool condition, const char* message) noexcept {
        if (!condition) {
            std::printf("[ERR] %s\n", message);
            return false;
        }
        return true;
    }

    unsigned widget_signal_summary_case_count{0};

    void print_widget_signal_run_begin() noexcept {
        std::printf("[ws] run=widget_signal_demo phase=begin\n");
    }

    void print_widget_signal_run_end(bool ok) noexcept {
        std::printf("[ws] run=widget_signal_demo phase=end result=%s cases=%u\n",
                    ok ? "ok" : "fail",
                    widget_signal_summary_case_count);
    }

    void print_widget_signal_case(const char* name) noexcept {
        ++widget_signal_summary_case_count;
        std::printf("[ws] case=%s", name);
    }
}

int main() {
    print_widget_signal_run_begin();

    std::printf("[ws-abi] object_base=%zu scroll_container=%zu list=%zu foldable_panel=%zu interaction_list=%zu double_tap=%zu pinch=%zu drag=%zu long_press=%zu\n",
                sizeof(ObjectBase),
                sizeof(ScrollContainer),
                sizeof(List),
                sizeof(FoldablePanel),
                sizeof(InteractionList<>),
                sizeof(DoubleTapRestoreStrategy),
                sizeof(PinchScrollStrategy),
                sizeof(DragStrategy),
                sizeof(LongPressStrategy));

    Probe probe{};

    Button button{"Apply"};
    button.set_on_click(Callback::bind<&on_button_legacy_click>());

    const auto primary =
        button.observe_click(util::delegate<>::bind<&Probe::on_primary_click>(probe));
    const auto secondary =
        button.observe_click(util::delegate<>::bind<&Probe::on_secondary_click>(probe));

    if (!expect(static_cast<bool>(primary), "connect primary button edge observer")) return 1;
    if (!expect(static_cast<bool>(secondary), "connect secondary button edge observer")) return 1;

    button.set_text("Apply Now");
    if (!expect(probe.primary_clicks == 0 && probe.secondary_clicks == 0,
                "programmatic text update does not create click edge")) {
        return 1;
    }
    if (!expect(g_button_legacy_clicks == 0, "programmatic text update stays silent for legacy callback")) return 1;

    if (!expect(button.on_event(Event::mouse(Event::Type::Click, 1, 1, 1)), "button accepts in-bounds click")) {
        return 1;
    }
    if (!expect(probe.primary_clicks == 1 && probe.secondary_clicks == 1,
                "button click synchronously broadcasts edge observers")) {
        return 1;
    }
    if (!expect(g_button_legacy_clicks == 1, "button click still triggers legacy callback")) return 1;

    if (!expect(button.unobserve_click(primary.value()), "disconnect primary button edge observer")) return 1;
    if (!expect(!button.unobserve_click(primary.value()), "stale button edge token rejected")) return 1;

    if (!expect(button.on_event(Event::mouse(Event::Type::Click, 1, 1, 1)), "button accepts second click")) {
        return 1;
    }
    if (!expect(probe.primary_clicks == 1, "disconnected primary observer stays silent")) return 1;
    if (!expect(probe.secondary_clicks == 2, "remaining observer still sees button edge")) return 1;
    if (!expect(g_button_legacy_clicks == 2, "legacy callback still works after observer disconnect")) return 1;

    button.set_enabled(false);
    if (!expect(!button.on_event(Event::mouse(Event::Type::Click, 1, 1, 1)), "disabled button rejects click")) {
        return 1;
    }
    if (!expect(probe.secondary_clicks == 2, "disabled button does not emit hidden edge")) return 1;
    if (!expect(g_button_legacy_clicks == 2, "disabled button does not invoke legacy callback")) return 1;

    MenuItem menu{"Open"};
    menu.set_on_click(Callback::bind<&on_menu_legacy_click>());
    const auto menu_click =
        menu.observe_click(util::delegate<>::bind<&Probe::on_menu_click>(probe));
    if (!expect(static_cast<bool>(menu_click), "connect menu edge observer")) return 1;

    menu.set_text("Open File");
    if (!expect(probe.menu_clicks == 0, "menu text update does not create click edge")) return 1;
    if (!expect(g_menu_legacy_clicks == 0, "menu text update stays silent for legacy callback")) return 1;

    if (!expect(menu.on_event(Event::mouse(Event::Type::Click, 1, 1, 1)), "menu accepts in-bounds click")) {
        return 1;
    }
    if (!expect(probe.menu_clicks == 1, "menu click synchronously broadcasts edge observers")) return 1;
    if (!expect(g_menu_legacy_clicks == 1, "menu click still triggers legacy callback")) return 1;

    if (!expect(menu.unobserve_click(menu_click.value()), "disconnect menu edge observer")) return 1;
    if (!expect(!menu.unobserve_click(menu_click.value()), "stale menu edge token rejected")) return 1;

    if (!expect(menu.on_event(Event::mouse(Event::Type::Click, 1, 1, 1)), "menu accepts second click")) {
        return 1;
    }
    if (!expect(probe.menu_clicks == 1, "disconnected menu observer stays silent")) return 1;
    if (!expect(g_menu_legacy_clicks == 2, "menu legacy callback still works after observer disconnect")) return 1;

    menu.set_enabled(false);
    if (!expect(!menu.on_event(Event::mouse(Event::Type::Click, 1, 1, 1)), "disabled menu rejects click")) {
        return 1;
    }
    if (!expect(probe.menu_clicks == 1, "disabled menu does not emit hidden edge")) return 1;
    if (!expect(g_menu_legacy_clicks == 2, "disabled menu does not invoke legacy callback")) return 1;

    ListItem list{"Alpha"};
    list.set_on_click(Callback::bind<&on_list_legacy_click>());
    const auto list_primary =
        list.observe_click(util::delegate<>::bind<&Probe::on_list_click>(probe));
    const auto list_secondary =
        list.observe_click(util::delegate<>::bind<&Probe::on_list_secondary_click>(probe));
    if (!expect(static_cast<bool>(list_primary), "connect primary list edge observer")) return 1;
    if (!expect(static_cast<bool>(list_secondary), "connect secondary list edge observer")) return 1;

    list.set_text("Alpha Prime");
    if (!expect(probe.list_clicks == 0 && probe.list_secondary_clicks == 0,
                "list text update does not create click edge")) {
        return 1;
    }
    if (!expect(g_list_legacy_clicks == 0, "list text update stays silent for legacy callback")) return 1;

    if (!expect(list.on_event(Event::mouse(Event::Type::Click, 1, 1, 1)), "list accepts in-bounds click")) {
        return 1;
    }
    if (!expect(probe.list_clicks == 1 && probe.list_secondary_clicks == 1,
                "list click synchronously broadcasts edge observers")) {
        return 1;
    }
    if (!expect(g_list_legacy_clicks == 1, "list click still triggers legacy callback")) return 1;

    if (!expect(list.unobserve_click(list_primary.value()), "disconnect primary list edge observer")) return 1;
    if (!expect(!list.unobserve_click(list_primary.value()), "stale list edge token rejected")) return 1;

    if (!expect(list.on_event(Event::mouse(Event::Type::Click, 1, 1, 1)), "list accepts second click")) {
        return 1;
    }
    if (!expect(probe.list_clicks == 1, "disconnected primary list observer stays silent")) return 1;
    if (!expect(probe.list_secondary_clicks == 2, "remaining list observer still sees edge")) return 1;
    if (!expect(g_list_legacy_clicks == 2, "list legacy callback still works after observer disconnect")) return 1;

    list.set_enabled(false);
    if (!expect(!list.on_event(Event::mouse(Event::Type::Click, 1, 1, 1)), "disabled list rejects click")) {
        return 1;
    }
    if (!expect(probe.list_secondary_clicks == 2, "disabled list does not emit hidden edge")) return 1;
    if (!expect(g_list_legacy_clicks == 2, "disabled list does not invoke legacy callback")) return 1;

    InteractionProbe interaction_probe{};

    DoubleTapRestoreStrategy double_tap{};
    double_tap.set_callback(
        DoubleTapRestoreStrategy::callback_delegate::bind<&InteractionProbe::on_double_tap>(interaction_probe));
    double_tap.set_threshold(300, 12);
    if (!expect(!double_tap.on_event(Event::mouse(Event::Type::Click, 20, 20, 0, 100)),
                "double tap waits for the second click")) return 1;
    if (!expect(double_tap.on_event(Event::mouse(Event::Type::Click, 22, 21, 0, 200)),
                "double tap accepts an in-threshold second click")) return 1;
    if (!expect(interaction_probe.double_taps == 1, "double tap invokes its delegate once")) return 1;
    double_tap.set_enabled(false);
    if (!expect(!double_tap.on_event(Event::mouse(Event::Type::Click, 22, 21, 0, 250)),
                "disabled double tap stays silent")) return 1;

    PinchScrollStrategy pinch{};
    pinch.set_callbacks(
        PinchScrollStrategy::begin_delegate::bind<&InteractionProbe::on_pinch_begin>(interaction_probe),
        PinchScrollStrategy::update_delegate::bind<&InteractionProbe::on_pinch_update>(interaction_probe),
        PinchScrollStrategy::end_delegate::bind<&InteractionProbe::on_pinch_end>(interaction_probe));
    if (!expect(pinch.on_event(Event::gesture(Event::Type::GesturePinch, 10, 10, 0, 0,
                                               Event::GesturePhase::Begin)),
                "pinch accepts begin")) return 1;
    if (!expect(pinch.on_event(Event::gesture(Event::Type::GesturePinch, 10, 10, 0, 7,
                                               Event::GesturePhase::Update)),
                "pinch accepts update")) return 1;
    if (!expect(pinch.on_event(Event::gesture(Event::Type::GesturePinch, 10, 10, 0, 0,
                                               Event::GesturePhase::End)),
                "pinch accepts end")) return 1;
    if (!expect(interaction_probe.pinch_begin_order == 1
                    && interaction_probe.pinch_update_order == 2
                    && interaction_probe.pinch_end_order == 3
                    && interaction_probe.pinch_dy == 7,
                "pinch delegates preserve order and payload")) return 1;

    interaction_probe.sequence = 0;
    DragStrategy drag{};
    drag.set_callbacks(
        DragStrategy::begin_delegate::bind<&InteractionProbe::on_drag_begin>(interaction_probe),
        DragStrategy::update_delegate::bind<&InteractionProbe::on_drag_update>(interaction_probe),
        DragStrategy::end_delegate::bind<&InteractionProbe::on_drag_end>(interaction_probe));
    if (!expect(drag.on_event(Event::drag(Event::Type::DragStart, 4, 5, 0, 0)),
                "drag accepts start")) return 1;
    if (!expect(drag.on_event(Event::drag(Event::Type::DragMove, 7, 9, 3, 4)),
                "drag accepts update")) return 1;
    if (!expect(drag.on_event(Event::drag(Event::Type::DragEnd, 8, 10, 0, 0)),
                "drag accepts end")) return 1;
    if (!expect(interaction_probe.drag_begin_order == 1
                    && interaction_probe.drag_update_order == 2
                    && interaction_probe.drag_end_order == 3
                    && interaction_probe.drag_x == 8
                    && interaction_probe.drag_y == 10
                    && interaction_probe.drag_dx == 3
                    && interaction_probe.drag_dy == 4,
                "drag delegates preserve order and payload")) return 1;

    LongPressStrategy long_press{};
    long_press.set_callback(
        LongPressStrategy::callback_delegate::bind<&InteractionProbe::on_long_press>(interaction_probe));
    long_press.set_threshold(100, 3);
    if (!expect(!long_press.on_event(Event::mouse(Event::Type::MouseDown, 10, 10, 0, 1000)),
                "long press starts without consuming mouse down")) return 1;
    if (!expect(long_press.on_event(Event::mouse(Event::Type::MouseUp, 10, 10, 0, 1120)),
                "long press fires after the threshold")) return 1;
    if (!expect(interaction_probe.long_presses == 1, "long press invokes its delegate once")) return 1;
    if (!expect(!long_press.on_event(Event::mouse(Event::Type::MouseDown, 10, 10, 0, 2000)),
                "second long press starts")) return 1;
    if (!expect(!long_press.on_event(Event::mouse(Event::Type::MouseMove, 20, 10, 0, 2020)),
                "movement cancels long press without consuming move")) return 1;
    if (!expect(!long_press.on_event(Event::mouse(Event::Type::MouseUp, 20, 10, 0, 2200)),
                "canceled long press stays silent on release")) return 1;
    long_press.set_enabled(false);
    if (!expect(!long_press.on_event(Event::mouse(Event::Type::MouseDown, 10, 10, 0, 3000)),
                "disabled long press rejects input")) return 1;
    if (!expect(interaction_probe.long_presses == 1, "canceled and disabled long press do not invoke delegate")) return 1;

    Image image{};
    if (!expect(!image.on_event(Event::mouse(Event::Type::Click, 4, 4, 0, 100)),
                "image double tap waits for second click")) return 1;
    if (!expect(image.on_event(Event::mouse(Event::Type::Click, 4, 4, 0, 200)),
                "image directly dispatches owned double tap strategy")) return 1;
    image.set_double_tap_restore(false);
    if (!expect(!image.on_event(Event::mouse(Event::Type::Click, 4, 4, 0, 300)),
                "image disables owned double tap strategy")) return 1;

    ScrollContainer scroll{};
    scroll.set_rect({0, 0, 100, 100});
    const WidgetHandle child_a{WidgetKind::Button, 1, 1};
    const WidgetHandle child_b{WidgetKind::Label, 2, 1};
    const WidgetHandle child_c{WidgetKind::Image, 3, 1};
    if (!expect(scroll.add_child(child_a) && scroll.add_child(child_c),
                "scroll container owns child handles")) return 1;
    if (!expect(scroll.insert_child_before(child_b, child_c)
                    && scroll.child_count() == 3
                    && scroll.child_at(0) == child_a
                    && scroll.child_at(1) == child_b
                    && scroll.child_at(2) == child_c,
                "owned child storage preserves insertion order")) return 1;
    if (!expect(scroll.move_child_to_front(child_b)
                    && scroll.child_at(2) == child_b,
                "owned child storage moves a child to the front")) return 1;
    if (!expect(scroll.move_child_to_back(child_b)
                    && scroll.child_at(0) == child_b,
                "owned child storage moves a child to the back")) return 1;
    if (!expect(scroll.remove_child(child_a)
                    && !scroll.has_child(child_a)
                    && scroll.child_count() == 2,
                "owned child storage removes a child")) return 1;
    scroll.clear_children();
    bool filled_to_capacity = true;
    for (std::size_t i = 0; i < ScrollContainer::child_capacity; ++i) {
        filled_to_capacity = filled_to_capacity
            && scroll.add_child(WidgetHandle{WidgetKind::Button, static_cast<std::uint16_t>(i), 1});
    }
    if (!expect(filled_to_capacity
                    && scroll.child_count() == ScrollContainer::child_capacity
                    && !scroll.add_child(WidgetHandle{WidgetKind::Button, 65, 1}),
                "owned child storage reports fixed-capacity overflow")) return 1;
    scroll.clear_children();
    const auto pinch_begin = Event::gesture(Event::Type::GesturePinch, 50, 50, 0, 0,
                                            Event::GesturePhase::Begin);
    if (!expect(scroll.on_event(pinch_begin), "scroll container directly dispatches owned pinch strategy")) return 1;
    scroll.set_pinch_enabled(false);
    if (!expect(!scroll.on_event(pinch_begin), "scroll container disables owned pinch strategy")) return 1;

    SpinZoomWidget spin_zoom{};
    if (!expect(!spin_zoom.on_event(Event::mouse(Event::Type::Click, 4, 4, 0, 100)),
                "spin zoom double tap waits for second click")) return 1;
    if (!expect(spin_zoom.on_event(Event::mouse(Event::Type::Click, 4, 4, 0, 200)),
                "spin zoom directly dispatches owned double tap strategy")) return 1;
    spin_zoom.set_double_tap_restore(false);
    if (!expect(!spin_zoom.on_event(Event::mouse(Event::Type::Click, 4, 4, 0, 300)),
                "spin zoom disables owned double tap strategy")) return 1;

    if constexpr (sizeof(void*) == 8) {
        if (!expect(sizeof(ObjectBase) <= 32, "ObjectBase retained legacy ownership or dispatch storage")) {
            return 1;
        }
    }

    print_widget_signal_case("button_click_edge");
    std::printf(" primary=%d secondary=%d legacy=%d\n",
                probe.primary_clicks,
                probe.secondary_clicks,
                g_button_legacy_clicks);
    print_widget_signal_case("menu_item_click_edge");
    std::printf(" clicks=%d legacy=%d\n",
                probe.menu_clicks,
                g_menu_legacy_clicks);
    print_widget_signal_case("list_item_click_edge");
    std::printf(" primary=%d secondary=%d legacy=%d\n",
                probe.list_clicks,
                probe.list_secondary_clicks,
                g_list_legacy_clicks);
    print_widget_signal_case("interaction_strategies");
    std::printf(" double_tap=%d pinch_order=%d/%d/%d drag_order=%d/%d/%d long_press=%d\n",
                interaction_probe.double_taps,
                interaction_probe.pinch_begin_order,
                interaction_probe.pinch_update_order,
                interaction_probe.pinch_end_order,
                interaction_probe.drag_begin_order,
                interaction_probe.drag_update_order,
                interaction_probe.drag_end_order,
                interaction_probe.long_presses);
    print_widget_signal_case("owned_interaction_dispatch");
    std::printf(" image=direct scroll=direct spin_zoom=direct\n");
    print_widget_signal_case("object_runtime_footprint");
    std::printf(" object_base=%zu child_capacity=%zu opt_in_components=%zu\n",
                sizeof(ObjectBase),
                ScrollContainer::child_capacity,
                sizeof(InteractionList<>) + sizeof(DragStrategy) + sizeof(LongPressStrategy));
    print_widget_signal_run_end(true);
    std::puts("[widget_signal_demo] ok");
    return 0;
}
