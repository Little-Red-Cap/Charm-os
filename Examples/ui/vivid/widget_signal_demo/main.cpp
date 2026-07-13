#include <array>
#include <cstdint>
#include <cstdio>
#include <span>
#include <type_traits>

import charm.core;
import charm.core.config;
import charm.core.event;
import charm.core.input_interaction;
import charm.core.object;
import charm.core.style;
import charm.widgets.button;
import charm.widgets.chart;
import charm.widgets.console_box;
import charm.widgets.foldable_panel;
import charm.widgets.image;
import charm.widgets.histogram;
import charm.widgets.histogram_view;
import charm.widgets.list;
import charm.widgets.list_view;
import charm.widgets.menu_item;
import charm.widgets.scroll_container;
import charm.widgets.spin_zoom_widget;
import charm.widgets.spectrum_view;
import charm.widgets.table_view;
import charm.widgets.tree_view;
import charm.widgets.waveform_view;
import charm.gfx.canvas;
import charm.gfx.framebuffer;

namespace {
    template<typename T>
    concept OwnsObjectChildren = requires(T& object, WidgetHandle child) {
        object.add_child(child);
        object.remove_child(child);
        object.child_count();
        object.child_at(0);
    };

    static_assert(!OwnsObjectChildren<Button>);
    static_assert(sizeof(Button) <= 360,
                  "Button must not retain per-instance style or nine-slice skin storage");
    static_assert(OwnsObjectChildren<List>);
    static_assert(OwnsObjectChildren<FoldablePanel>);
    static_assert(OwnsObjectChildren<ScrollContainer>);
    static_assert(!std::is_copy_constructible_v<InteractionList<>>);
    static_assert(!std::is_move_constructible_v<InteractionList<>>);
    static_assert(!std::is_copy_constructible_v<Image>);
    static_assert(!std::is_move_constructible_v<Image>);
    static_assert(!std::is_copy_constructible_v<ScrollContainer>);
    static_assert(!std::is_move_constructible_v<ScrollContainer>);
    static_assert(sizeof(ScrollContainer) <= 672,
                  "ScrollContainer must not retain per-child layout or visual override storage");
    static_assert(!std::is_copy_constructible_v<SpinZoomWidget>);
    static_assert(!std::is_move_constructible_v<SpinZoomWidget>);
    static_assert(!std::is_copy_constructible_v<ListView>);
    static_assert(!std::is_move_constructible_v<ListView>);
    static_assert(!std::is_copy_constructible_v<ListView::ItemPoolWorkspace>);
    static_assert(!std::is_move_constructible_v<ListView::ItemPoolWorkspace>);
    static_assert(!std::is_copy_constructible_v<TreeView>);
    static_assert(!std::is_move_constructible_v<TreeView>);
    static_assert(!std::is_copy_constructible_v<TreeView::ItemPoolWorkspace>);
    static_assert(!std::is_move_constructible_v<TreeView::ItemPoolWorkspace>);
    static_assert(sizeof(ListView) <= 256);
    static_assert(sizeof(TreeView) <= 224);
    static_assert(!std::is_copy_constructible_v<ConsoleBox::Buffer>);
    static_assert(!std::is_move_constructible_v<ConsoleBox::Buffer>);
    static_assert(sizeof(ConsoleBox::Buffer::Line) <= ConsoleBox::Buffer::line_length + 2);
    static_assert(sizeof(ConsoleBox)
                  <= sizeof(ObjectBase) + sizeof(void*) + alignof(void*) - 1);
    static_assert(sizeof(Chart)
                  <= sizeof(ObjectBase) + sizeof(std::span<const int>)
                      + sizeof(void*) * 2 + alignof(void*) * 2);
    static_assert(sizeof(Histogram)
                  <= sizeof(ObjectBase) + sizeof(std::span<const int>)
                      + sizeof(void*) * 2 + sizeof(int) * 2 + sizeof(bool) * 2
                      + alignof(void*) * 2);
    static_assert(sizeof(HistogramView)
                  <= sizeof(ObjectBase) + sizeof(std::span<const int>)
                      + sizeof(int) * 2 + sizeof(bool) + alignof(void*) * 2);
    static_assert(sizeof(WaveformView)
                  <= sizeof(ObjectBase) + sizeof(std::span<const int>)
                      + sizeof(int) * 2 + sizeof(bool) + alignof(void*) * 2);
    static_assert(!std::is_copy_constructible_v<SpectrumView>);
    static_assert(!std::is_move_constructible_v<SpectrumView>);
    static_assert(!std::is_copy_constructible_v<SpectrumView::PeakWorkspace>);
    static_assert(!std::is_move_constructible_v<SpectrumView::PeakWorkspace>);
    static_assert(sizeof(SpectrumView)
                  <= sizeof(ObjectBase) + sizeof(std::span<const float>)
                      + sizeof(void*) + sizeof(SpectrumView::Mode)
                      + sizeof(float) + sizeof(std::uint32_t) + alignof(void*) * 2);
    static_assert(sizeof(SpectrumView::PeakWorkspace)
                  <= sizeof(std::span<float>) + sizeof(void*) + alignof(void*) - 1);
    static_assert(std::is_same_v<Callback, util::delegate<>>);
    static_assert(sizeof(Callback) == sizeof(void*) + sizeof(Callback::stub_t));

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

    struct StructuredCallbackProbe {
        int count_calls{0};
        int draw_calls{0};
        int row_height_calls{0};
        int row_flags_calls{0};
        int column_width_calls{0};
        int select_calls{0};
        int scroll_calls{0};
        int toggle_calls{0};
        int pool_create_calls{0};
        int pool_bind_calls{0};
        int pool_recycle_calls{0};
        int last_slot{-2};
        int last_index{-1};

        static int count(void* ctx) noexcept {
            ++static_cast<StructuredCallbackProbe*>(ctx)->count_calls;
            return 1;
        }

        static void draw_list(void* ctx, CanvasBase&, const ListView::DrawInfo& info) noexcept {
            auto& probe = *static_cast<StructuredCallbackProbe*>(ctx);
            ++probe.draw_calls;
            probe.last_slot = info.slot;
        }

        static int list_row_height(void* ctx, int) noexcept {
            ++static_cast<StructuredCallbackProbe*>(ctx)->row_height_calls;
            return 16;
        }

        static std::uint8_t list_row_flags(const void* ctx, std::uint16_t) noexcept {
            auto* probe = const_cast<StructuredCallbackProbe*>(
                static_cast<const StructuredCallbackProbe*>(ctx));
            ++probe->row_flags_calls;
            return 0;
        }

        static void list_scroll(void* ctx, int, int, int, int) noexcept {
            ++static_cast<StructuredCallbackProbe*>(ctx)->scroll_calls;
        }

        static void list_select(void* ctx, int) noexcept {
            ++static_cast<StructuredCallbackProbe*>(ctx)->select_calls;
        }

        static void draw_table(void* ctx, CanvasBase&, const TableView::CellInfo&) noexcept {
            ++static_cast<StructuredCallbackProbe*>(ctx)->draw_calls;
        }

        static int table_column_width(void* ctx, int) noexcept {
            ++static_cast<StructuredCallbackProbe*>(ctx)->column_width_calls;
            return 32;
        }

        static void table_select(void* ctx, int, int) noexcept {
            ++static_cast<StructuredCallbackProbe*>(ctx)->select_calls;
        }

        static TreeView::NodeInfo tree_node(void*, int) noexcept {
            return TreeView::NodeInfo{0, false, true, nullptr};
        }

        static void draw_tree(void* ctx, CanvasBase&, const TreeView::DrawInfo& info) noexcept {
            auto& probe = *static_cast<StructuredCallbackProbe*>(ctx);
            ++probe.draw_calls;
            probe.last_slot = info.slot;
        }

        static int tree_row_height(void* ctx, int, const TreeView::NodeInfo&) noexcept {
            ++static_cast<StructuredCallbackProbe*>(ctx)->row_height_calls;
            return 16;
        }

        static void tree_toggle(void* ctx, int) noexcept {
            ++static_cast<StructuredCallbackProbe*>(ctx)->toggle_calls;
        }

        static void tree_select(void* ctx, int) noexcept {
            ++static_cast<StructuredCallbackProbe*>(ctx)->select_calls;
        }

        static void pool_create(void* ctx, int slot) noexcept {
            auto& probe = *static_cast<StructuredCallbackProbe*>(ctx);
            ++probe.pool_create_calls;
            probe.last_slot = slot;
        }

        static void list_pool_bind(void* ctx, int slot, int index) noexcept {
            auto& probe = *static_cast<StructuredCallbackProbe*>(ctx);
            ++probe.pool_bind_calls;
            probe.last_slot = slot;
            probe.last_index = index;
        }

        static void tree_pool_bind(void* ctx,
                                   int slot,
                                   int index,
                                   const TreeView::NodeInfo&) noexcept {
            list_pool_bind(ctx, slot, index);
        }

        static void pool_recycle(void* ctx, int slot, int index) noexcept {
            auto& probe = *static_cast<StructuredCallbackProbe*>(ctx);
            ++probe.pool_recycle_calls;
            probe.last_slot = slot;
            probe.last_index = index;
        }
    };

    struct DataViewProbe {
        std::array<int, 4> values{-4, 2, 8, 0};
        int calls{0};

        static std::span<const int> read(void* ctx) noexcept {
            auto& probe = *static_cast<DataViewProbe*>(ctx);
            ++probe.calls;
            return probe.values;
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

    std::printf("[ws-abi] object_base=%zu callback=%zu button=%zu scroll_container=%zu list=%zu foldable_panel=%zu interaction_list=%zu double_tap=%zu pinch=%zu drag=%zu long_press=%zu\n",
                sizeof(ObjectBase),
                sizeof(Callback),
                sizeof(Button),
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

    ScrollContainer translated_scroll{};
    translated_scroll.set_rect({10, 20, 100, 50});
    ObjectBase translated_child_a{};
    ObjectBase translated_child_b{};
    translated_child_a.set_rect({14, 26, 20, 20});
    translated_child_b.set_rect({14, 70, 20, 20});
    const WidgetHandle translated_handle_a{WidgetKind::Button, 70, 1};
    const WidgetHandle translated_handle_b{WidgetKind::Button, 71, 1};
    if (!expect(translated_scroll.add_child(translated_handle_a)
                    && translated_scroll.add_child(translated_handle_b),
                "scroll translation fixture owns child handles")) return 1;
    const auto resolve_translated = [&](WidgetHandle handle) noexcept -> ObjectBase* {
        if (handle == translated_handle_a) return &translated_child_a;
        if (handle == translated_handle_b) return &translated_child_b;
        return nullptr;
    };
    if (!expect(translated_scroll.sync_child_bases(resolve_translated),
                "scroll synchronizes a fully resolved layout")) return 1;
    translated_scroll.set_scroll_y(15);
    if (!expect(translated_scroll.apply_scroll(resolve_translated, false)
                    && translated_child_a.get_rect().x == 14
                    && translated_child_a.get_rect().y == 11
                    && translated_child_b.get_rect().x == 14
                    && translated_child_b.get_rect().y == 55,
                "scroll applies one common child translation")) return 1;
    translated_scroll.set_pos(30, 40);
    if (!expect(translated_scroll.apply_scroll(resolve_translated, false)
                    && translated_child_a.get_rect().x == 34
                    && translated_child_a.get_rect().y == 31
                    && translated_child_b.get_rect().x == 34
                    && translated_child_b.get_rect().y == 75,
                "container movement preserves child-local positions")) return 1;

    translated_scroll.set_scroll_y(20);
    const Rect before_failed_apply_a = translated_child_a.get_rect();
    const Rect before_failed_apply_b = translated_child_b.get_rect();
    const auto resolve_with_gap = [&](WidgetHandle handle) noexcept -> ObjectBase* {
        return handle == translated_handle_b ? nullptr : resolve_translated(handle);
    };
    if (!expect(!translated_scroll.apply_scroll(resolve_with_gap, false)
                    && translated_child_a.get_rect().x == before_failed_apply_a.x
                    && translated_child_a.get_rect().y == before_failed_apply_a.y
                    && translated_child_b.get_rect().x == before_failed_apply_b.x
                    && translated_child_b.get_rect().y == before_failed_apply_b.y,
                "unresolved scroll child fails before partial mutation")) return 1;
    if (!expect(translated_scroll.apply_scroll(resolve_translated, false)
                    && translated_child_a.get_rect().y == 26
                    && translated_child_b.get_rect().y == 70,
                "successful retry applies the pending scroll delta once")) return 1;

    SpinZoomWidget spin_zoom{};
    if (!expect(!spin_zoom.on_event(Event::mouse(Event::Type::Click, 4, 4, 0, 100)),
                "spin zoom double tap waits for second click")) return 1;
    if (!expect(spin_zoom.on_event(Event::mouse(Event::Type::Click, 4, 4, 0, 200)),
                "spin zoom directly dispatches owned double tap strategy")) return 1;
    spin_zoom.set_double_tap_restore(false);
    if (!expect(!spin_zoom.on_event(Event::mouse(Event::Type::Click, 4, 4, 0, 300)),
                "spin zoom disables owned double tap strategy")) return 1;

    using CallbackFrameBuffer = FrameBuffer<PixelFormat::RGB565, 96, 96>;
    static CallbackFrameBuffer callback_fb{};
    Canvas<PixelFormat::RGB565, 96, 96> callback_canvas{callback_fb};

    const Style saved_scroll_style = Theme::instance().get<ScrollContainer>();
    Style themed_scroll_style = saved_scroll_style;
    themed_scroll_style.colors.bg_color = {248, 8, 8, 255};
    themed_scroll_style.colors.border_color = {8, 8, 8, 255};
    Theme::instance().set<ScrollContainer>(themed_scroll_style);
    callback_fb.clear({0, 0, 0, 255});
    translated_scroll.draw(callback_canvas);
    const auto themed_scroll_pixel = callback_fb.get_pixel(40, 50);
    Theme::instance().set<ScrollContainer>(saved_scroll_style);
    if (!expect(themed_scroll_pixel.r >= 240
                    && themed_scroll_pixel.g <= 12
                    && themed_scroll_pixel.b <= 12,
                "scroll container resolves visual state from Theme")) return 1;

    const Style saved_button_style = Theme::instance().get<Button>();
    Style themed_button_style = saved_button_style;
    themed_button_style.colors.bg_color = {248, 8, 8, 255};
    themed_button_style.colors.border_color = {8, 8, 8, 255};
    themed_button_style.metrics.corner_radius = 0;
    Theme::instance().set<Button>(themed_button_style);
    Button themed_button{""};
    themed_button.set_rect({8, 8, 40, 24});
    callback_fb.clear({0, 0, 0, 255});
    themed_button.draw(callback_canvas);
    const auto themed_button_pixel = callback_fb.get_pixel(24, 18);
    Theme::instance().set<Button>(saved_button_style);
    if (!expect(themed_button_pixel.r >= 240
                    && themed_button_pixel.g <= 12
                    && themed_button_pixel.b <= 12,
                "button resolves visual state from Theme")) return 1;

    StructuredCallbackProbe list_data{};
    StructuredCallbackProbe list_draw{};
    StructuredCallbackProbe list_height{};
    StructuredCallbackProbe list_flags{};
    StructuredCallbackProbe list_select{};
    StructuredCallbackProbe list_scroll{};
    ListView list_view{};
    list_view.set_rect({0, 0, 64, 48});
    list_view.set_show_scrollbar(false);
    list_view.set_data_source(&StructuredCallbackProbe::count,
                              &StructuredCallbackProbe::draw_list,
                              &list_data);
    list_view.set_row_height_fn(&StructuredCallbackProbe::list_row_height, &list_height);
    list_view.set_row_flags_fn(&StructuredCallbackProbe::list_row_flags, &list_flags);
    list_view.set_on_select(&StructuredCallbackProbe::list_select, &list_select);
    list_view.set_on_scroll(&StructuredCallbackProbe::list_scroll, &list_scroll);
    list_view.set_on_draw(&StructuredCallbackProbe::draw_list, &list_draw);
    list_view.draw(callback_canvas);
    list_view.set_selected(0);
    if (!expect(list_view.on_event(Event::mouse(Event::Type::Click, 8, 16)),
                "list view dispatches callback-backed row click")) return 1;
    if (!expect(list_data.draw_calls == 0
                    && list_draw.draw_calls > 0
                    && list_data.row_height_calls == 0
                    && list_height.row_height_calls > 0
                    && list_data.row_flags_calls == 0
                    && list_flags.row_flags_calls > 0
                    && list_select.select_calls > 0
                    && list_scroll.scroll_calls > 0,
                "list view keeps callback contexts isolated")) return 1;
    const int list_override_draw_calls = list_draw.draw_calls;
    list_view.set_data_source(&StructuredCallbackProbe::count,
                              &StructuredCallbackProbe::draw_list,
                              &list_data);
    list_view.draw(callback_canvas);
    if (!expect(list_data.draw_calls > 0 && list_draw.draw_calls == list_override_draw_calls,
                "list data-source rebind resets the draw context")) return 1;
    if (!expect(list_data.last_slot == -1,
                "list view skips item-pool slots without an attached workspace")) return 1;

    StructuredCallbackProbe list_pool{};
    ListView::ItemPoolWorkspace list_workspace{};
    list_workspace.set_item_pool(&StructuredCallbackProbe::pool_create,
                                 &StructuredCallbackProbe::list_pool_bind,
                                 &StructuredCallbackProbe::pool_recycle,
                                 &list_pool);
    if (!expect(list_view.attach_item_pool_workspace(list_workspace)
                    && list_view.has_item_pool_workspace(),
                "list view attaches an explicit item-pool workspace")) return 1;
    list_view.draw(callback_canvas);
    if (!expect(list_pool.pool_create_calls == 1
                    && list_pool.pool_bind_calls == 1
                    && list_data.last_slot == 0,
                "list view creates and binds item-pool slots on demand")) return 1;
    ListView competing_list_view{};
    if (!expect(!competing_list_view.attach_item_pool_workspace(list_workspace),
                "list item-pool workspace rejects concurrent attachment")) return 1;
    list_workspace.set_item_pool(&StructuredCallbackProbe::pool_create,
                                 &StructuredCallbackProbe::list_pool_bind,
                                 &StructuredCallbackProbe::pool_recycle,
                                 &list_pool);
    list_view.draw(callback_canvas);
    if (!expect(list_pool.pool_recycle_calls == 1
                    && list_pool.pool_create_calls == 2
                    && list_pool.pool_bind_calls == 2,
                "list item-pool reconfiguration recycles and rebinds live slots")) return 1;
    list_view.detach_item_pool_workspace();
    list_view.draw(callback_canvas);
    if (!expect(!list_view.has_item_pool_workspace()
                    && list_pool.pool_recycle_calls == 2
                    && list_data.last_slot == -1,
                "list item-pool detach recycles slots and restores zero-cache drawing")) return 1;

    StructuredCallbackProbe table_data{};
    StructuredCallbackProbe table_width{};
    StructuredCallbackProbe table_select{};
    TableView table_view{};
    table_view.set_rect({0, 0, 64, 48});
    table_view.set_data_source(&StructuredCallbackProbe::count,
                               &StructuredCallbackProbe::count,
                               &StructuredCallbackProbe::draw_table,
                               &table_data);
    table_view.set_column_width_fn(&StructuredCallbackProbe::table_column_width, &table_width);
    table_view.set_on_select(&StructuredCallbackProbe::table_select, &table_select);
    table_view.draw(callback_canvas);
    table_view.set_selected(0, 0);
    if (!expect(table_data.draw_calls > 0
                    && table_data.column_width_calls == 0
                    && table_width.column_width_calls > 0
                    && table_select.select_calls == 1,
                "table view keeps column and selection contexts isolated")) return 1;

    StructuredCallbackProbe tree_data{};
    StructuredCallbackProbe tree_height{};
    StructuredCallbackProbe tree_toggle{};
    StructuredCallbackProbe tree_select{};
    TreeView tree_view{};
    tree_view.set_rect({0, 0, 64, 48});
    tree_view.set_data_source(&StructuredCallbackProbe::count,
                              &StructuredCallbackProbe::tree_node,
                              &StructuredCallbackProbe::draw_tree,
                              &tree_data);
    tree_view.set_row_height_fn(&StructuredCallbackProbe::tree_row_height, &tree_height);
    tree_view.set_on_toggle(&StructuredCallbackProbe::tree_toggle, &tree_toggle);
    tree_view.set_on_select(&StructuredCallbackProbe::tree_select, &tree_select);
    tree_view.draw(callback_canvas);
    if (!expect(tree_view.on_event(Event::mouse(Event::Type::Click, 8, 8)),
                "tree view dispatches callback-backed row click")) return 1;
    if (!expect(tree_data.draw_calls > 0
                    && tree_data.row_height_calls == 0
                    && tree_height.row_height_calls > 0
                    && tree_data.toggle_calls == 0
                    && tree_toggle.toggle_calls == 1
                    && tree_select.select_calls == 1,
                "tree view keeps row, toggle and selection contexts isolated")) return 1;
    if (!expect(tree_data.last_slot == -1,
                "tree view skips item-pool slots without an attached workspace")) return 1;

    StructuredCallbackProbe tree_pool{};
    TreeView::ItemPoolWorkspace tree_workspace{};
    tree_workspace.set_item_pool(&StructuredCallbackProbe::pool_create,
                                 &StructuredCallbackProbe::tree_pool_bind,
                                 &StructuredCallbackProbe::pool_recycle,
                                 &tree_pool);
    if (!expect(tree_view.attach_item_pool_workspace(tree_workspace)
                    && tree_view.has_item_pool_workspace(),
                "tree view attaches an explicit item-pool workspace")) return 1;
    tree_view.draw(callback_canvas);
    if (!expect(tree_pool.pool_create_calls == 1
                    && tree_pool.pool_bind_calls == 1
                    && tree_data.last_slot == 0,
                "tree view creates and binds item-pool slots on demand")) return 1;
    TreeView competing_tree_view{};
    if (!expect(!competing_tree_view.attach_item_pool_workspace(tree_workspace),
                "tree item-pool workspace rejects concurrent attachment")) return 1;
    tree_workspace.set_item_pool(&StructuredCallbackProbe::pool_create,
                                 &StructuredCallbackProbe::tree_pool_bind,
                                 &StructuredCallbackProbe::pool_recycle,
                                 &tree_pool);
    tree_view.draw(callback_canvas);
    if (!expect(tree_pool.pool_recycle_calls == 1
                    && tree_pool.pool_create_calls == 2
                    && tree_pool.pool_bind_calls == 2,
                "tree item-pool reconfiguration recycles and rebinds live slots")) return 1;
    tree_view.detach_item_pool_workspace();
    tree_view.draw(callback_canvas);
    if (!expect(!tree_view.has_item_pool_workspace()
                    && tree_pool.pool_recycle_calls == 2
                    && tree_data.last_slot == -1,
                "tree item-pool detach recycles slots and restores zero-cache drawing")) return 1;

    if constexpr (sizeof(void*) == 8) {
        if (!expect(sizeof(ObjectBase) <= 32, "ObjectBase retained legacy ownership or dispatch storage")) {
            return 1;
        }
    }

    std::array<ConsoleBox::Buffer::Line, 2> console_lines{};
    ConsoleBox::Buffer console_buffer{console_lines};
    ConsoleBox console_box{};
    console_box.append("ignored");
    if (!expect(console_buffer.line_count() == 1 && console_buffer.line_at(0).empty(),
                "console box does not write without an attached buffer")) return 1;
    console_box.attach_buffer(console_buffer);
    console_box.append("alpha\nbeta\ngamma");
    if (!expect(console_box.has_buffer()
                    && console_buffer.capacity() == 2
                    && console_buffer.line_count() == 2
                    && console_buffer.line_at(0) == "beta"
                    && console_buffer.line_at(1) == "gamma",
                "console buffer preserves bounded ring order")) return 1;
    console_box.draw(callback_canvas);
    console_box.detach_buffer();
    console_box.append("ignored-after-detach");
    if (!expect(!console_box.has_buffer()
                    && console_buffer.line_at(0) == "beta"
                    && console_buffer.line_at(1) == "gamma",
                "console box detach preserves caller-owned data")) return 1;

    std::array<int, 4> data_view_values{-3, 1, 7, 0};
    Chart chart{};
    chart.set_points(data_view_values);
    chart.draw(callback_canvas);
    DataViewProbe chart_source{};
    chart.set_data_source(&chart_source, &DataViewProbe::read);
    if (!expect(chart_source.calls == 0,
                "chart data source stays lazy until draw")) return 1;
    chart.draw(callback_canvas);
    if (!expect(chart_source.calls == 1,
                "chart reads one stable span per draw")) return 1;

    Histogram histogram{};
    histogram.set_values(data_view_values);
    histogram.draw(callback_canvas);
    DataViewProbe histogram_source{};
    histogram.set_data_source(&histogram_source, &DataViewProbe::read);
    if (!expect(histogram_source.calls == 0,
                "histogram data source stays lazy until draw")) return 1;
    histogram.draw(callback_canvas);
    if (!expect(histogram_source.calls == 1,
                "histogram reads one stable span per draw")) return 1;

    HistogramView histogram_view{};
    histogram_view.set_values(data_view_values);
    histogram_view.draw(callback_canvas);
    WaveformView waveform_view{};
    waveform_view.set_samples(data_view_values);
    waveform_view.draw(callback_canvas);

    std::array<float, 3> spectrum_values{0.2f, 0.8f, 0.4f};
    std::array<float, 3> spectrum_peaks{1.0f, 1.0f, 1.0f};
    SpectrumView::PeakWorkspace spectrum_workspace{std::span<float>{spectrum_peaks}};
    SpectrumView spectrum_view{};
    spectrum_view.set_values(spectrum_values);
    if constexpr (enable_float_widgets) {
        spectrum_view.draw(callback_canvas);
        if (!expect(spectrum_workspace.peak_at(0) == 0.0f
                        && spectrum_workspace.peak_at(1) == 0.0f
                        && spectrum_workspace.peak_at(2) == 0.0f,
                    "spectrum view does not retain peaks without a workspace")) return 1;
        if (!expect(spectrum_view.attach_peak_workspace(spectrum_workspace)
                        && spectrum_view.has_peak_workspace()
                        && spectrum_workspace.capacity() == spectrum_values.size(),
                    "spectrum view attaches an explicit peak workspace")) return 1;
        spectrum_view.draw(callback_canvas);
        if (!expect(spectrum_workspace.peak_at(0) == 0.2f
                        && spectrum_workspace.peak_at(1) == 0.8f
                        && spectrum_workspace.peak_at(2) == 0.4f,
                    "spectrum workspace captures current peaks")) return 1;
        SpectrumView competing_spectrum{};
        if (!expect(!competing_spectrum.attach_peak_workspace(spectrum_workspace),
                    "spectrum peak workspace rejects concurrent attachment")) return 1;
        spectrum_view.set_peak_decay(0.1f);
        spectrum_values = {0.1f, 0.3f, 0.2f};
        spectrum_view.draw(callback_canvas);
        if (!expect(spectrum_workspace.peak_at(0) >= 0.099f
                        && spectrum_workspace.peak_at(0) <= 0.101f
                        && spectrum_workspace.peak_at(1) >= 0.699f
                        && spectrum_workspace.peak_at(1) <= 0.701f
                        && spectrum_workspace.peak_at(2) >= 0.299f
                        && spectrum_workspace.peak_at(2) <= 0.301f,
                    "spectrum peaks decay without crossing current values")) return 1;
        spectrum_view.set_values(std::span<const float>{spectrum_values.data(), 1});
        spectrum_view.draw(callback_canvas);
        if (!expect(spectrum_workspace.peak_at(0) >= 0.099f
                        && spectrum_workspace.peak_at(0) <= 0.101f
                        && spectrum_workspace.peak_at(1) == 0.0f
                        && spectrum_workspace.peak_at(2) == 0.0f,
                    "spectrum workspace clears inactive peak slots")) return 1;
        spectrum_view.detach_peak_workspace();
        spectrum_values[0] = 0.9f;
        spectrum_view.draw(callback_canvas);
        if (!expect(!spectrum_view.has_peak_workspace()
                        && spectrum_workspace.peak_at(0) >= 0.099f
                        && spectrum_workspace.peak_at(0) <= 0.101f,
                    "spectrum detach preserves caller-owned peak state")) return 1;
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
    std::printf(" object_base=%zu child_capacity=%zu opt_in_components=%zu console_box=%zu console_line=%zu console_buffer=%zu chart=%zu histogram=%zu histogram_view=%zu waveform_view=%zu spectrum_view=%zu spectrum_workspace=%zu data_source_calls=%d/%d\n",
                sizeof(ObjectBase),
                ScrollContainer::child_capacity,
                sizeof(InteractionList<>) + sizeof(DragStrategy) + sizeof(LongPressStrategy),
                sizeof(ConsoleBox),
                sizeof(ConsoleBox::Buffer::Line),
                sizeof(ConsoleBox::Buffer),
                sizeof(Chart),
                sizeof(Histogram),
                sizeof(HistogramView),
                sizeof(WaveformView),
                sizeof(SpectrumView),
                sizeof(SpectrumView::PeakWorkspace),
                chart_source.calls,
                histogram_source.calls);
    print_widget_signal_case("structured_callback_contexts");
    std::printf(" list_draw=%d table_width=%d tree_toggle=%d list_pool=%d/%d/%d tree_pool=%d/%d/%d list_size=%zu tree_size=%zu\n",
                list_draw.draw_calls,
                table_width.column_width_calls,
                tree_toggle.toggle_calls,
                list_pool.pool_create_calls,
                list_pool.pool_bind_calls,
                list_pool.pool_recycle_calls,
                tree_pool.pool_create_calls,
                tree_pool.pool_bind_calls,
                tree_pool.pool_recycle_calls,
                sizeof(ListView),
                sizeof(TreeView));
    print_widget_signal_run_end(true);
    std::puts("[widget_signal_demo] ok");
    return 0;
}
