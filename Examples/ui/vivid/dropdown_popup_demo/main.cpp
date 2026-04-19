#include <cstdio>

import charm.core;
import charm.core.event;
import charm.core.soa_factory;
import charm.widgets.dropdown_popup;

namespace {
    struct Probe {
        int selected_changes{0};
        int selected_old{0};
        int selected_new{0};
        int select_edges{0};
        int last_confirmed{-1};

        void on_selected(const int& now, const int& old) noexcept {
            ++selected_changes;
            selected_old = old;
            selected_new = now;
        }

        void on_select(const int& index) noexcept {
            ++select_edges;
            last_confirmed = index;
        }
    };

    int g_legacy_selects = 0;

    void on_legacy_select() noexcept {
        ++g_legacy_selects;
    }

    [[nodiscard]] bool expect(bool condition, const char* message) noexcept {
        if (!condition) {
            std::printf("[ERR] %s\n", message);
            return false;
        }
        return true;
    }

    [[nodiscard]] int row_center_y(int popup_y, int index) noexcept {
        return popup_y + index * DropdownPopup::kRowHeight + DropdownPopup::kRowHeight / 2;
    }
}

int main() {
    constexpr int popup_x = 20;
    constexpr int popup_y = 40;
    constexpr int popup_w = 120;

    SoaKernel kernel{};
    SoaFactory factory{kernel};
    const auto root = factory.create_container();
    const auto host = factory.create_button_static("Mode");
    factory.link(root, host);
    kernel.set_rect(root, {0, 0, 240, 160});
    kernel.set_rect(host, {8, 8, 100, 28});

    DropdownPopup popup{factory, host, root};
    const char* const options[] = {
        "Low",
        "Mid",
        "High",
    };
    popup.set_options(options, 3);

    Probe probe{};
    popup.set_on_select(Callback::bind<&on_legacy_select>());
    const auto selected_conn =
        popup.observe_selected(util::delegate<const int&, const int&>::bind<&Probe::on_selected>(probe));
    const auto select_conn =
        popup.observe_select(util::delegate<const int&>::bind<&Probe::on_select>(probe));

    if (!expect(static_cast<bool>(selected_conn), "connect committed selection observer")) return 1;
    if (!expect(static_cast<bool>(select_conn), "connect confirm edge observer")) return 1;

    popup.set_selection(2);
    if (!expect(popup.selected() == 2, "programmatic selection updates committed truth")) return 1;
    if (!expect(probe.selected_changes == 1 && probe.selected_old == 0 && probe.selected_new == 2,
                "programmatic selection reports truth old/new")) {
        return 1;
    }
    if (!expect(probe.select_edges == 0, "programmatic selection stays silent for confirm edge")) return 1;
    if (!expect(g_legacy_selects == 0, "programmatic selection stays silent for legacy callback")) return 1;

    popup.open_at(popup_x, popup_y, popup_w);
    if (!expect(popup.is_open(), "popup opens")) return 1;
    if (!expect(popup.handle_event(Event::mouse(Event::Type::MouseMove, popup_x + 8, row_center_y(popup_y, 0), 0)),
                "mouse move inside popup is accepted")) {
        return 1;
    }
    if (!expect(popup.selected() == 2, "hover highlight does not change committed truth")) return 1;
    if (!expect(probe.selected_changes == 1 && probe.select_edges == 0 && g_legacy_selects == 0,
                "hover highlight stays silent")) {
        return 1;
    }

    popup.open_at(popup_x, popup_y, popup_w);
    if (!expect(popup.handle_event(Event::key(Event::Type::KeyDown, Event::Key::Down)),
                "keyboard navigation is accepted while open")) {
        return 1;
    }
    if (!expect(popup.selected() == 2, "keyboard navigation does not change committed truth")) return 1;
    if (!expect(popup.handle_event(Event::key(Event::Type::KeyDown, Event::Key::Enter)),
                "enter confirms highlighted option")) {
        return 1;
    }
    if (!expect(!popup.is_open(), "confirm closes popup")) return 1;
    if (!expect(popup.selected() == 0, "confirm commits highlighted option")) return 1;
    if (!expect(probe.selected_changes == 2 && probe.selected_old == 2 && probe.selected_new == 0,
                "confirm updates committed truth once")) {
        return 1;
    }
    if (!expect(probe.select_edges == 1 && probe.last_confirmed == 0,
                "confirm emits explicit edge with committed index")) {
        return 1;
    }
    if (!expect(g_legacy_selects == 1, "confirm still triggers legacy callback")) return 1;

    popup.open_at(popup_x, popup_y, popup_w);
    if (!expect(popup.handle_event(Event::key(Event::Type::KeyDown, Event::Key::Enter)),
                "enter confirms current selection")) {
        return 1;
    }
    if (!expect(popup.selected() == 0, "same-selection confirm keeps committed truth")) return 1;
    if (!expect(probe.selected_changes == 2, "same-selection confirm does not synthesize truth change")) return 1;
    if (!expect(probe.select_edges == 2 && probe.last_confirmed == 0,
                "same-selection confirm still emits edge")) {
        return 1;
    }
    if (!expect(g_legacy_selects == 2, "same-selection confirm still triggers legacy callback")) return 1;

    popup.open_at(popup_x, popup_y, popup_w);
    if (!expect(popup.handle_event(Event::mouse(Event::Type::Click, popup_x + 8, row_center_y(popup_y, 2), 1)),
                "click inside popup confirms row")) {
        return 1;
    }
    if (!expect(popup.selected() == 2, "click confirm commits clicked row")) return 1;
    if (!expect(probe.selected_changes == 3 && probe.selected_old == 0 && probe.selected_new == 2,
                "click confirm reports committed truth change")) {
        return 1;
    }
    if (!expect(probe.select_edges == 3 && probe.last_confirmed == 2,
                "click confirm emits edge with clicked row")) {
        return 1;
    }
    if (!expect(g_legacy_selects == 3, "click confirm still triggers legacy callback")) return 1;

    popup.open_at(popup_x, popup_y, popup_w);
    if (!expect(popup.handle_event(Event::mouse(Event::Type::Click, 4, 4, 1)),
                "outside click closes popup")) {
        return 1;
    }
    if (!expect(!popup.is_open(), "outside click closes popup without confirm")) return 1;
    if (!expect(probe.select_edges == 3 && g_legacy_selects == 3,
                "outside click does not emit hidden confirm edge")) {
        return 1;
    }

    if (!expect(popup.unobserve_select(select_conn.value()), "disconnect confirm edge observer")) return 1;
    if (!expect(!popup.unobserve_select(select_conn.value()), "stale confirm edge token rejected")) return 1;
    if (!expect(popup.unobserve_selected(selected_conn.value()), "disconnect committed truth observer")) return 1;
    if (!expect(!popup.unobserve_selected(selected_conn.value()), "stale committed truth token rejected")) return 1;

    popup.set_selection(1);
    if (!expect(popup.selected() == 1, "truth still updates after observers disconnect")) return 1;
    if (!expect(probe.selected_changes == 3, "disconnected truth observer stays silent")) return 1;

    popup.open_at(popup_x, popup_y, popup_w);
    if (!expect(popup.handle_event(Event::key(Event::Type::KeyDown, Event::Key::Enter)),
                "confirm still works after observer disconnect")) {
        return 1;
    }
    if (!expect(probe.select_edges == 3, "disconnected edge observer stays silent")) return 1;
    if (!expect(g_legacy_selects == 4, "legacy callback still works after observer disconnect")) return 1;

    std::printf("[dropdown_popup] selected=%d truth_changes=%d confirm_edges=%d legacy=%d\n",
                popup.selected(),
                probe.selected_changes,
                probe.select_edges,
                g_legacy_selects);
    std::puts("[dropdown_popup_demo] ok");
    return 0;
}
