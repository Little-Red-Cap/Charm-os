#include <cstdio>
#include <string_view>
#include <type_traits>

import charm.core;
import charm.core.event;
import charm.core.soa_factory;
import charm.widgets.dropdown_popup;

static_assert(sizeof(DropdownPopup) <= 200,
              "DropdownPopup must not retain an implicit confirm-edge signal table");
static_assert(!std::is_copy_constructible_v<DropdownPopup>);
static_assert(!std::is_move_constructible_v<DropdownPopup>);

namespace {
    struct Probe {
        int selected_changes{0};
        int selected_old{0};
        int selected_new{0};
        int select_calls{0};
        int last_confirmed{-1};

        void on_selected(const int& now, const int& old) noexcept {
            ++selected_changes;
            selected_old = old;
            selected_new = now;
        }

        void on_select(const int& index) noexcept {
            ++select_calls;
            last_confirmed = index;
        }
    };

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
    std::printf("[dropdown-popup-abi] bytes=%zu selected_state=%zu select_callback=%zu\n",
                sizeof(DropdownPopup),
                sizeof(DropdownPopup::selected_state_type),
                sizeof(DropdownPopup::select_callback));

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
    char middle_option[] = "Mid";
    const char* options[] = {
        "Low",
        middle_option,
        nullptr,
    };
    popup.set_options(options, 3);

    const auto popup_container = kernel.next_sibling(host);
    const auto popup_list = kernel.first_child(popup_container);
    if (!expect(popup_container && popup_list,
                "popup exposes its generated list through the SoA hierarchy")) return 1;
    if (!expect(std::string_view{kernel.list_view_item_text(popup_list, 0)} == "Low"
                    && std::string_view{kernel.list_view_item_text(popup_list, 1)} == "Mid"
                    && std::string_view{kernel.list_view_item_text(popup_list, 2)}.empty(),
                "popup list borrows option text and normalizes null labels")) return 1;
    middle_option[0] = 'm';
    options[0] = "Minimum";
    if (!expect(std::string_view{kernel.list_view_item_text(popup_list, 0)} == "Minimum"
                    && std::string_view{kernel.list_view_item_text(popup_list, 1)} == "mid",
                "popup list observes caller-owned text and pointer updates")) return 1;

    Probe probe{};
    popup.set_on_select(DropdownPopup::select_callback::bind<&Probe::on_select>(probe));
    const auto selected_conn =
        popup.observe_selected(util::delegate<const int&, const int&>::bind<&Probe::on_selected>(probe));

    if (!expect(static_cast<bool>(selected_conn), "connect committed selection observer")) return 1;

    popup.set_selection(2);
    if (!expect(popup.selected() == 2, "programmatic selection updates committed truth")) return 1;
    if (!expect(probe.selected_changes == 1 && probe.selected_old == 0 && probe.selected_new == 2,
                "programmatic selection reports truth old/new")) {
        return 1;
    }
    if (!expect(probe.select_calls == 0, "programmatic selection stays silent for confirm callback")) return 1;

    popup.open_at(popup_x, popup_y, popup_w);
    if (!expect(popup.is_open(), "popup opens")) return 1;
    if (!expect(popup.handle_event(Event::mouse(Event::Type::MouseMove, popup_x + 8, row_center_y(popup_y, 0), 0)),
                "mouse move inside popup is accepted")) {
        return 1;
    }
    if (!expect(popup.selected() == 2, "hover highlight does not change committed truth")) return 1;
    if (!expect(probe.selected_changes == 1 && probe.select_calls == 0,
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
    if (!expect(probe.select_calls == 1 && probe.last_confirmed == 0,
                "confirm invokes typed callback with committed index")) {
        return 1;
    }

    popup.open_at(popup_x, popup_y, popup_w);
    if (!expect(popup.handle_event(Event::key(Event::Type::KeyDown, Event::Key::Enter)),
                "enter confirms current selection")) {
        return 1;
    }
    if (!expect(popup.selected() == 0, "same-selection confirm keeps committed truth")) return 1;
    if (!expect(probe.selected_changes == 2, "same-selection confirm does not synthesize truth change")) return 1;
    if (!expect(probe.select_calls == 2 && probe.last_confirmed == 0,
                "same-selection confirm still invokes callback")) {
        return 1;
    }

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
    if (!expect(probe.select_calls == 3 && probe.last_confirmed == 2,
                "click confirm invokes callback with clicked row")) {
        return 1;
    }

    popup.open_at(popup_x, popup_y, popup_w);
    if (!expect(popup.handle_event(Event::mouse(Event::Type::Click, 4, 4, 1)),
                "outside click closes popup")) {
        return 1;
    }
    if (!expect(!popup.is_open(), "outside click closes popup without confirm")) return 1;
    if (!expect(probe.select_calls == 3,
                "outside click does not emit hidden confirm edge")) {
        return 1;
    }

    popup.set_on_select({});
    if (!expect(popup.unobserve_selected(selected_conn.value()), "disconnect committed truth observer")) return 1;
    if (!expect(!popup.unobserve_selected(selected_conn.value()), "stale committed truth token rejected")) return 1;

    popup.set_selection(1);
    if (!expect(popup.selected() == 1, "truth still updates after observers disconnect")) return 1;
    if (!expect(probe.selected_changes == 3, "disconnected truth observer stays silent")) return 1;

    popup.open_at(popup_x, popup_y, popup_w);
    if (!expect(popup.handle_event(Event::key(Event::Type::KeyDown, Event::Key::Enter)),
                "confirm still works after callback clear")) {
        return 1;
    }
    if (!expect(probe.select_calls == 3, "cleared confirm callback stays silent")) return 1;

    std::printf("[dropdown_popup] selected=%d truth_changes=%d confirm_calls=%d borrowed=1\n",
                popup.selected(),
                probe.selected_changes,
                probe.select_calls);
    std::puts("[dropdown_popup_demo] ok");
    return 0;
}
